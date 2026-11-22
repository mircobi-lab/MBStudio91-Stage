#include "Station.h"
#include "MidiPanic.h"

namespace
{
    constexpr double gainRampSeconds = 0.02;
    constexpr double tailFadeSeconds = 0.1;     // sfumatura della coda dopo i 3 s
    constexpr double quickFadeSeconds = 0.02;   // coda chiusa da un nuovo cambio
}

Station::Station()
{
    gain.reset (sampleRate, gainRampSeconds);
    gain.setCurrentAndTargetValue (1.0f);
    deferredMidi.ensureSize (4096);
    incoming.ensureSize (4096);
    fadeGains.resize ((size_t) blockSize);
}

Station::~Station()
{
    retireChain (active.chain);
    for (auto& t : tails)
        retireChain (t.chain);
}

//==============================================================================
void Station::setPlayHead (juce::AudioPlayHead* ph)
{
    playHead = ph;
}

void Station::prepareInstance (juce::AudioPluginInstance& p) const
{
    p.setPlayHead (playHead);
    p.enableAllBuses();
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
}

int Station::channelsNeeded (const Chain& c)
{
    int n = 2;
    auto consider = [&n] (const InstancePtr& p)
    {
        if (p != nullptr)
            n = juce::jmax (n, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
    };
    consider (c.instrument);
    for (const auto& fx : c.effects)
        consider (fx);
    return n;
}

void Station::prepareChain (LiveChain& lc)
{
    // releaseResources prima: il wrapper VST3 di JUCE ignora prepareToPlay se gia' attivo con gli stessi valori.
    auto prep = [this] (InstancePtr& p)
    {
        if (p != nullptr)
        {
            p->releaseResources();
            prepareInstance (*p);
        }
    };
    prep (lc.chain.instrument);
    for (auto& fx : lc.chain.effects)
        prep (fx);

    lc.buffer.setSize (channelsNeeded (lc.chain), blockSize, false, true, false);
    lc.midi.ensureSize (2048);
}

bool Station::anySustain (const NoteTracker& t)
{
    for (int ch = 1; ch <= 16; ++ch)
        if (t.isSustainDown (ch))
            return true;
    return false;
}

void Station::startFade (LiveChain& lc, double seconds) const
{
    if (lc.state == TailState::fading && lc.fadeStep >= 1.0f / (float) (seconds * sampleRate))
        return;   // gia' in sfumatura piu' rapida
    lc.state = TailState::fading;
    lc.fadeStep = 1.0f / (float) juce::jmax (1.0, seconds * sampleRate);
}

void Station::prepare (double newSampleRate, int maxBlockSize)
{
    const juce::SpinLock::ScopedLockType sl (lock);

    sampleRate = newSampleRate;
    blockSize = juce::jmax (1, maxBlockSize);

    const float current = gain.getTargetValue();
    gain.reset (sampleRate, gainRampSeconds);
    gain.setCurrentAndTargetValue (current);

    prepareChain (active);
    for (auto& t : tails)
        if (t.state != TailState::none)
            prepareChain (t);

    mix.setSize (2, blockSize, false, true, false);
    fadeGains.resize ((size_t) blockSize);
}

//==============================================================================
void Station::retire (InstancePtr old)
{
    if (old == nullptr)
        return;
    if (onBeforeInstanceRemoved)
        onBeforeInstanceRemoved (*old);
    old->releaseResources();
    old.reset();
}

void Station::retireChain (Chain& c)
{
    retire (std::move (c.instrument));
    for (auto& fx : c.effects)
        retire (std::move (fx));
}

void Station::setInstrument (InstancePtr newInstrument)
{
    if (newInstrument != nullptr)
    {
        newInstrument->releaseResources();
        prepareInstance (*newInstrument);   // fuori dal lock: puo' essere lento
    }

    InstancePtr old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (active.chain.instrument);
        active.chain.instrument = std::move (newInstrument);
        active.buffer.setSize (channelsNeeded (active.chain), blockSize, false, true, false);
        active.tracker.reset();
    }

    if (active.chain.instrument != nullptr)
        setLoadError ({});

    retire (std::move (old));   // distrutto qui, sul thread chiamante, mai sul thread audio
}

bool Station::setEffect (int slot, InstancePtr newEffect)
{
    if (! juce::isPositiveAndBelow (slot, maxEffects))
        return false;

    if (newEffect != nullptr)
    {
        newEffect->releaseResources();
        prepareInstance (*newEffect);
    }

    InstancePtr old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (active.chain.effects[(size_t) slot]);
        active.chain.effects[(size_t) slot] = std::move (newEffect);
        active.buffer.setSize (channelsNeeded (active.chain), blockSize, false, true, false);
    }

    retire (std::move (old));
    return true;
}

void Station::setChain (Chain newChain)
{
    // Preparazione fuori dal lock: puo' richiedere qualche millisecondo.
    LiveChain incoming;
    incoming.chain = std::move (newChain);
    {
        auto prep = [this] (InstancePtr& p)
        {
            if (p != nullptr)
            {
                p->releaseResources();
                prepareInstance (*p);
            }
        };
        prep (incoming.chain.instrument);
        for (auto& fx : incoming.chain.effects)
            prep (fx);
    }

    Chain dropped;
    {
        const juce::SpinLock::ScopedLockType sl (lock);

        // Le code esistenti sfumano in fretta.
        for (auto& t : tails)
            if (t.state == TailState::held)
                startFade (t, quickFadeSeconds);

        // Slot libero per la catena che diventa coda; se non c'e', si sacrifica la coda piu' avanti.
        LiveChain* slot = nullptr;
        for (auto& t : tails)
            if (t.state == TailState::none) { slot = &t; break; }
        if (slot == nullptr)
            for (auto& t : tails)
                if (t.state == TailState::finished) { slot = &t; break; }
        if (slot == nullptr)
            slot = &tails[0];

        if (slot->state != TailState::none)
        {
            dropped = std::move (slot->chain);
            slot->chain = {};
        }

        std::swap (slot->chain, active.chain);
        std::swap (slot->tracker, active.tracker);
        std::swap (slot->buffer, active.buffer);
        std::swap (slot->midi, active.midi);
        slot->state = (slot->chain.instrument != nullptr) ? TailState::held : TailState::finished;
        if (slot->state == TailState::finished)
            tailFinished = true;
        slot->samplesLeft = (int) (tailSeconds * sampleRate);
        slot->fadeGain = 1.0f;
        slot->fadeStep = 0.0f;

        active.chain = std::move (incoming.chain);
        active.tracker.reset();
        active.buffer.setSize (channelsNeeded (active.chain), blockSize, false, true, false);
        active.midi.ensureSize (2048);
    }

    retireChain (dropped);
    if (active.chain.instrument != nullptr)
        setLoadError ({});
}

std::vector<std::unique_ptr<juce::AudioPluginInstance>> Station::takeFinishedInstances()
{
    std::vector<InstancePtr> out;
    if (! tailFinished.exchange (false))
        return out;   // niente da ritirare: il thread audio non viene disturbato
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& t : tails)
        {
            if (t.state != TailState::finished)
                continue;
            if (t.chain.instrument != nullptr) out.push_back (std::move (t.chain.instrument));
            for (auto& fx : t.chain.effects)
                if (fx != nullptr) out.push_back (std::move (fx));
            t.chain = {};
            t.tracker.reset();
            t.state = TailState::none;
        }
    }

    for (auto& p : out)
        if (onBeforeInstanceRemoved)
            onBeforeInstanceRemoved (*p);
    return out;
}

bool Station::hasTailChain() const
{
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (lock));
    for (const auto& t : tails)
        if (t.state == TailState::held || t.state == TailState::fading)
            return true;
    return false;
}

juce::AudioPluginInstance* Station::getInstrument() const { return active.chain.instrument.get(); }

juce::AudioPluginInstance* Station::getEffect (int slot) const
{
    return juce::isPositiveAndBelow (slot, maxEffects) ? active.chain.effects[(size_t) slot].get() : nullptr;
}

void Station::setLoadError (juce::String e)
{
    const juce::ScopedLock sl (errorLock);
    loadError = std::move (e);
}

juce::String Station::getLoadError() const
{
    const juce::ScopedLock sl (errorLock);
    return loadError;
}

void Station::setGainDb (float db) { targetGain = juce::Decibels::decibelsToGain (db); }
float Station::getGainDb() const    { return juce::Decibels::gainToDecibels (targetGain.load(), -60.0f); }
void Station::setMuted (bool m)    { muted = m; }
bool Station::isMuted() const      { return muted; }

void Station::requestPanic()
{
    panicPending = true;

    juce::MessageManager::callAsync ([safe = juce::WeakReference<Station> (this)]
    {
        if (auto* self = safe.get())
            self->resetTailsOnMessageThread();
    });
}

void Station::resetTailsOnMessageThread()
{
    // Con il lock preso il thread audio salta i blocchi: silenzio mentre i plugin ripartono.
    const juce::SpinLock::ScopedLockType sl (lock);
    auto reprep = [this] (InstancePtr& p)
    {
        if (p != nullptr)
        {
            p->releaseResources();
            prepareInstance (*p);
        }
    };
    reprep (active.chain.instrument);
    for (auto& fx : active.chain.effects)
        reprep (fx);
}

//==============================================================================
void Station::processChain (LiveChain& lc, int numSamples)
{
    juce::AudioBuffer<float> work (lc.buffer.getArrayOfWritePointers(), lc.buffer.getNumChannels(), numSamples);
    work.clear();

    lc.tracker.process (lc.midi);
    lc.chain.instrument->processBlock (work, lc.midi);

    for (auto& fx : lc.chain.effects)
    {
        if (fx == nullptr)
            continue;
        emptyMidi.clear();
        juce::AudioBuffer<float> fxView (work.getArrayOfWritePointers(),
                                         juce::jmax (fx->getTotalNumInputChannels(), fx->getTotalNumOutputChannels()),
                                         numSamples);
        fx->processBlock (fxView, emptyMidi);
    }
}

void Station::renderAdd (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn)
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked())
    {
        // Sostituzione in corso: un blocco di silenzio, ma il MIDI si tiene per il blocco dopo.
        for (const auto meta : midiIn)
            deferredMidi.addEvent (meta.data, meta.numBytes, 0);
        return;
    }

    const int numSamples = out.getNumSamples();
    incoming.clear();
    if (! deferredMidi.isEmpty())
    {
        for (const auto meta : deferredMidi)
            incoming.addEvent (meta.data, meta.numBytes, 0);
        deferredMidi.clear();
    }
    incoming.addEvents (midiIn, 0, numSamples, 0);
    gain.setTargetValue (muted ? 0.0f : targetGain.load());

    // Buffer piu' grandi del blocco dichiarato: caso raro, si adeguano.
    auto ensure = [numSamples] (juce::AudioBuffer<float>& b)
    {
        if (b.getNumSamples() < numSamples)
            b.setSize (juce::jmax (2, b.getNumChannels()), numSamples, false, true, true);
    };
    ensure (active.buffer);
    ensure (mix);
    if ((int) fadeGains.size() < numSamples)
        fadeGains.resize ((size_t) numSamples);   // caso raro: blocco piu' grande di quello dichiarato
    for (auto& t : tails)
        if (t.state == TailState::held || t.state == TailState::fading)
            ensure (t.buffer);

    active.midi.clear();
    for (auto& t : tails)
        t.midi.clear();

    auto isLive = [] (const LiveChain& t) { return t.state == TailState::held || t.state == TailState::fading; };

    // Code "impegnate": hanno ancora note o pedale giu'. Solo loro ricevono controlli, bend e pressione.
    auto isEngaged = [&isLive] (const LiveChain& t) { return isLive (t) && (t.tracker.heldNoteCount() > 0 || anySustain (t.tracker)); };
    std::array<bool, maxTails> engaged {};
    for (size_t i = 0; i < tails.size(); ++i)
        engaged[i] = isEngaged (tails[i]);

    // Instradamento: note nuove alla catena attiva, note-off e aftertouch a chi tiene la nota,
    // controlli (pedale, mod wheel, espressione), bend e pressione anche alle code impegnate.
    for (const auto meta : incoming)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jmin (meta.samplePosition, numSamples - 1);

        if (m.isNoteOff() || m.isAftertouch())
        {
            LiveChain* target = &active;
            for (auto& t : tails)
                if (isLive (t) && t.tracker.isNoteHeld (m.getChannel(), m.getNoteNumber()))
                    target = &t;
            target->midi.addEvent (m, pos);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            active.midi.addEvent (m, pos);
            for (auto& t : tails)
                if (isLive (t))
                    t.midi.addEvent (m, pos);
        }
        else if (m.isController() || m.isPitchWheel() || m.isChannelPressure())
        {
            active.midi.addEvent (m, pos);
            for (size_t i = 0; i < tails.size(); ++i)
                if (engaged[i])
                    tails[i].midi.addEvent (m, pos);
        }
        else
        {
            active.midi.addEvent (m, pos);
        }
    }

    const bool doPanic = panicPending.exchange (false);
    if (doPanic)
    {
        active.midi.addEvents (MidiPanic::makePanicBuffer (active.tracker, 0), 0, -1, 0);
        for (auto& t : tails)
            if (isLive (t))
            {
                t.midi.addEvents (MidiPanic::makePanicBuffer (t.tracker, 0), 0, -1, 0);
                startFade (t, quickFadeSeconds);
            }
    }

    juce::AudioBuffer<float> sum (mix.getArrayOfWritePointers(), 2, numSamples);
    sum.clear();

    auto addInto = [&sum, numSamples] (const juce::AudioBuffer<float>& src, int srcChannels, const float* perSampleGain)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* s = src.getReadPointer (juce::jmin (ch, srcChannels - 1));
            float* d = sum.getWritePointer (ch);
            if (perSampleGain == nullptr)
                juce::FloatVectorOperations::add (d, s, numSamples);
            else
                for (int i = 0; i < numSamples; ++i)
                    d[i] += s[i] * perSampleGain[i];
        }
    };

    if (active.chain.instrument != nullptr)
    {
        processChain (active, numSamples);
        if (! doPanic)   // dopo un panic il primo blocco e' silenzio garantito
            addInto (active.buffer, juce::jmax (1, juce::jmin (2, active.chain.instrument->getTotalNumOutputChannels())), nullptr);
    }
    else
    {
        active.tracker.reset();
    }

    // Code: suonano finche' hanno note o pedale, poi 3 s, poi sfumano.
    for (auto& t : tails)
    {
        if (! isLive (t))
            continue;

        processChain (t, numSamples);

        if (t.state == TailState::held)
        {
            if (t.tracker.heldNoteCount() == 0 && ! anySustain (t.tracker))
            {
                t.samplesLeft -= numSamples;
                if (t.samplesLeft <= 0)
                    startFade (t, tailFadeSeconds);
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            if (t.state == TailState::fading)
                t.fadeGain = juce::jmax (0.0f, t.fadeGain - t.fadeStep);
            fadeGains[(size_t) i] = t.fadeGain;
        }

        if (! doPanic)
            addInto (t.buffer, juce::jmax (1, juce::jmin (2, t.chain.instrument->getTotalNumOutputChannels())), fadeGains.data());

        if (t.state == TailState::fading && t.fadeGain <= 0.0f)
        {
            t.state = TailState::finished;   // le istanze vengono ritirate dal message thread
            tailFinished = true;
        }
    }

    const int outChannels = out.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gain.getNextValue();
        for (int ch = 0; ch < outChannels; ++ch)
            out.addSample (ch, i, sum.getSample (juce::jmin (ch, 1), i) * g);
    }
}

void Station::runWithLockHeldForTest (const std::function<void()>& f) { const juce::SpinLock::ScopedLockType sl (lock); f(); }
