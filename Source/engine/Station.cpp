#include "Station.h"
#include "MidiPanic.h"

namespace
{
    constexpr double gainRampSeconds = 0.02;
}

Station::Station()
{
    gain.reset (sampleRate, gainRampSeconds);
    gain.setCurrentAndTargetValue (1.0f);
}

Station::~Station()
{
    retire (std::move (instrument));
    for (auto& fx : effects)
        retire (std::move (fx));
}

void Station::prepareInstance (juce::AudioPluginInstance& p) const
{
    p.enableAllBuses();
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
}

int Station::channelsNeeded() const
{
    int n = 2;
    auto consider = [&n] (const InstancePtr& p)
    {
        if (p != nullptr)
            n = juce::jmax (n, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
    };
    consider (instrument);
    for (const auto& fx : effects)
        consider (fx);
    return n;
}

void Station::prepare (double newSampleRate, int maxBlockSize)
{
    const juce::SpinLock::ScopedLockType sl (lock);

    sampleRate = newSampleRate;
    blockSize = juce::jmax (1, maxBlockSize);

    const float current = gain.getTargetValue();
    gain.reset (sampleRate, gainRampSeconds);
    gain.setCurrentAndTargetValue (current);

    // releaseResources prima di preparare: il wrapper VST3 di JUCE ignora prepareToPlay
    // se il plugin e' gia' attivo con i valori appena impostati.
    if (instrument != nullptr)
    {
        instrument->releaseResources();
        prepareInstance (*instrument);
    }
    for (auto& fx : effects)
        if (fx != nullptr)
        {
            fx->releaseResources();
            prepareInstance (*fx);
        }

    scratch.setSize (channelsNeeded(), blockSize, false, true, false);
    midiScratch.ensureSize (2048);
}

void Station::retire (InstancePtr old)
{
    if (old == nullptr)
        return;
    if (onBeforeInstanceRemoved)
        onBeforeInstanceRemoved (*old);
    old->releaseResources();
    old.reset();
}

void Station::setInstrument (InstancePtr newInstrument)
{
    if (newInstrument != nullptr)
        prepareInstance (*newInstrument);   // fuori dal lock: puo' essere lento

    InstancePtr old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (instrument);
        instrument = std::move (newInstrument);
        scratch.setSize (channelsNeeded(), blockSize, false, true, false);
        tracker.reset();
    }

    if (instrument != nullptr)
        setLoadError ({});

    retire (std::move (old));   // distrutto qui, sul thread chiamante, mai sul thread audio
}

bool Station::setEffect (int slot, InstancePtr newEffect)
{
    if (! juce::isPositiveAndBelow (slot, maxEffects))
        return false;

    if (newEffect != nullptr)
        prepareInstance (*newEffect);

    InstancePtr old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (effects[(size_t) slot]);
        effects[(size_t) slot] = std::move (newEffect);
        scratch.setSize (channelsNeeded(), blockSize, false, true, false);
    }

    retire (std::move (old));
    return true;
}

juce::AudioPluginInstance* Station::getInstrument() const { return instrument.get(); }

juce::AudioPluginInstance* Station::getEffect (int slot) const
{
    return juce::isPositiveAndBelow (slot, maxEffects) ? effects[(size_t) slot].get() : nullptr;
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
    if (instrument != nullptr)
    {
        instrument->releaseResources();
        prepareInstance (*instrument);
    }
    for (auto& fx : effects)
        if (fx != nullptr)
        {
            fx->releaseResources();
            prepareInstance (*fx);
        }
}

void Station::renderAdd (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn)
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked())
        return;   // sostituzione in corso: un blocco di silenzio

    const int numSamples = out.getNumSamples();
    gain.setTargetValue (muted ? 0.0f : targetGain.load());

    if (instrument == nullptr)
    {
        panicPending = false;
        tracker.reset();
        gain.skip (numSamples);
        return;
    }

    if (scratch.getNumSamples() < numSamples)   // host che supera il blocco dichiarato
        scratch.setSize (scratch.getNumChannels(), numSamples, false, true, true);

    midiScratch.clear();
    midiScratch.addEvents (midiIn, 0, numSamples, 0);

    const bool doPanic = panicPending.exchange (false);
    if (doPanic)
        midiScratch.addEvents (MidiPanic::makePanicBuffer (tracker, 0), 0, -1, 0);

    tracker.process (midiScratch);

    juce::AudioBuffer<float> work (scratch.getArrayOfWritePointers(), scratch.getNumChannels(), numSamples);
    work.clear();

    instrument->processBlock (work, midiScratch);

    for (auto& fx : effects)
    {
        if (fx == nullptr)
            continue;
        emptyMidi.clear();
        juce::AudioBuffer<float> fxView (work.getArrayOfWritePointers(),
                                         juce::jmax (fx->getTotalNumInputChannels(), fx->getTotalNumOutputChannels()),
                                         numSamples);
        fx->processBlock (fxView, emptyMidi);
    }

    if (doPanic)
        work.clear();   // dopo un panic il primo blocco e' silenzio garantito

    const int outChannels = out.getNumChannels();
    const int srcChannels = juce::jmax (1, juce::jmin (2, instrument->getTotalNumOutputChannels()));

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gain.getNextValue();
        for (int ch = 0; ch < outChannels; ++ch)
            out.addSample (ch, i, work.getSample (juce::jmin (ch, srcChannels - 1), i) * g);
    }
}
