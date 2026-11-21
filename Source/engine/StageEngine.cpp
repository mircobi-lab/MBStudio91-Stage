#include "StageEngine.h"
#include "settings/AppPaths.h"

namespace
{
    constexpr double masterRampSeconds = 0.02;
}

StageEngine::StageEngine()
{
    masterGain.reset (currentSampleRate.load(), masterRampSeconds);
    masterGain.setCurrentAndTargetValue (1.0f);
    for (auto& b : stationMidi)
        b.ensureSize (2048);
    for (auto& b : rawMidi)
        b.ensureSize (2048);
    for (auto& c : collectors)
        c.reset (currentSampleRate.load());

    // La Keystation (postazione 1) usa il suo fader come volume (spec 5.3).
    KeyZone first;
    first.faderControlsVolume = true;
    processors[0].setZone (first);
}

StageEngine::~StageEngine()
{
    stop();
}

void StageEngine::start()
{
    if (started)
        return;
    started = true;

    std::unique_ptr<juce::XmlElement> saved = juce::parseXML (AppPaths::machineSettingsFile());
    const auto err = devices.initialise (0, 2, saved.get(), true);
    if (err.isNotEmpty())
        DBG ("Audio: " + err);

    devices.addAudioCallback (this);
    openAllMidiInputs();

    // Tastiere scollegate e ricollegate tornano attive senza riavviare.
    midiDevicesChanged = juce::MidiDeviceListConnection::make ([this] { openAllMidiInputs(); });
}

void StageEngine::stop()
{
    if (! started)
        return;
    started = false;

    midiDevicesChanged = {};
    for (const auto& id : openedMidiIds)
        devices.removeMidiInputDeviceCallback (id, this);
    openedMidiIds.clear();

    saveDeviceState();
    devices.removeAudioCallback (this);
    devices.closeAudioDevice();
}

void StageEngine::saveDeviceState()
{
    if (auto xml = devices.createStateXml())
        xml->writeTo (AppPaths::machineSettingsFile());
}

bool StageEngine::isReservedControlDevice (const juce::String& name)
{
    return name.containsIgnoreCase ("Launchpad");
}

StageEngine::MidiInputChanges StageEngine::planMidiInputChanges (const juce::StringArray& enabledIds,
                                                                const juce::Array<juce::MidiDeviceInfo>& available)
{
    MidiInputChanges c;
    for (const auto& id : enabledIds)
    {
        bool present = false;
        for (const auto& d : available)
            present = present || d.identifier == id;
        if (! present)
            c.toDisable.add (id);
    }
    for (const auto& d : available)
        if (! isReservedControlDevice (d.name))
            c.toOpen.add (d.identifier);
    return c;
}

void StageEngine::openAllMidiInputs()
{
    const auto changes = planMidiInputChanges (openedMidiIds, juce::MidiInput::getAvailableDevices());

    // Una tastiera scollegata va chiusa davvero: al ricollegamento verra' riaperta da capo.
    for (const auto& id : changes.toDisable)
    {
        deviceDisconnected (id);
        devices.removeMidiInputDeviceCallback (id, this);
        devices.setMidiInputDeviceEnabled (id, false);
        openedMidiIds.removeString (id);
    }

    for (const auto& id : changes.toOpen)
    {
        if (! devices.isMidiInputDeviceEnabled (id))
            devices.setMidiInputDeviceEnabled (id, true);

        devices.removeMidiInputDeviceCallback (id, this);   // evita doppie registrazioni
        devices.addMidiInputDeviceCallback (id, this);
        openedMidiIds.addIfNotAlreadyThere (id);
    }
}

juce::AudioDeviceManager& StageEngine::deviceManager() { return devices; }

Station& StageEngine::station (int i)
{
    jassert (juce::isPositiveAndBelow (i, numStations));
    return stations[(size_t) juce::jlimit (0, numStations - 1, i)];
}

void StageEngine::panicAll()
{
    for (auto& s : stations)
        s.requestPanic();
    for (auto& r : resetProcessorPending)
        r = true;   // le mappe delle note tenute si azzerano sul thread audio
    router.cancelLearn();
}

void StageEngine::setMasterGainDb (float db) { masterTarget = juce::Decibels::decibelsToGain (db); }
double StageEngine::getSampleRate() const   { return currentSampleRate; }
int StageEngine::getBlockSize() const       { return currentBlockSize; }

void StageEngine::prepare (double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    const float g = masterGain.getTargetValue();
    masterGain.reset (sampleRate, masterRampSeconds);
    masterGain.setCurrentAndTargetValue (g);

    for (auto& s : stations)
        s.prepare (sampleRate, blockSize);

    for (auto& c : collectors)
        c.reset (sampleRate);
}

void StageEngine::injectMidiForTest (int station0to3, const juce::MidiBuffer& midi)
{
    const juce::SpinLock::ScopedLockType sl (injectLock);
    injected[(size_t) juce::jlimit (0, numStations - 1, station0to3)].addEvents (midi, 0, -1, 0);
}

void StageEngine::renderBlock (juce::AudioBuffer<float>& out)
{
    const int numSamples = out.getNumSamples();
    out.clear();

    for (auto& b : rawMidi)
        b.clear();
    for (auto& b : stationMidi)
        b.clear();

    for (size_t i = 0; i < collectors.size(); ++i)
        collectors[i].removeNextBlockOfMessages (rawMidi[i], numSamples);

    {
        const juce::SpinLock::ScopedTryLockType sl (injectLock);
        if (sl.isLocked())
            for (size_t i = 0; i < injected.size(); ++i)
            {
                rawMidi[i].addEvents (injected[i], 0, numSamples, 0);
                injected[i].clear();
            }
    }

    for (size_t i = 0; i < stations.size(); ++i)
    {
        if (resetProcessorPending[i].exchange (false))
            processors[i].reset();

        std::optional<float> faderDb;
        processors[i].process (rawMidi[i], stationMidi[i], faderDb);
        if (faderDb.has_value())
            stations[i].setGainDb (*faderDb);

        stations[i].renderAdd (out, stationMidi[i]);
    }

    masterGain.setTargetValue (masterTarget.load());
    if (masterGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = masterGain.getNextValue();
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.setSample (ch, i, out.getSample (ch, i) * g);
        }
    }
    else
    {
        out.applyGain (masterGain.getTargetValue());
    }
}

void StageEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::AudioBuffer<float> out (outputChannelData, numOutputChannels, numSamples);
    renderBlock (out);
}

void StageEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void StageEngine::audioDeviceStopped() {}

void StageEngine::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m)
{
    if (source != nullptr)
        routeIncoming (source->getIdentifier(), source->getName(), m);
}

void StageEngine::routeIncoming (const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage& m)
{
    // Prima di un'eventuale riassegnazione: dove suonava questa tastiera e chi aveva la postazione.
    const int previousStation = router.stationFor (deviceId);
    const int learningStation = router.learningStation();
    const auto previousOwner = learningStation >= 0 ? router.assignmentFor (learningStation) : std::nullopt;

    const int learned = router.handleLearn (deviceId, deviceName, m);
    if (learned >= 0)
    {
        // Nessuna nota resta appesa sulle postazioni che cambiano tastiera.
        if (previousStation >= 0 && previousStation != learned)
            releaseStation (previousStation);
        if (previousOwner.has_value() && previousOwner->deviceId != deviceId)
            releaseStation (learned);

        juce::MessageManager::callAsync ([safe = juce::WeakReference<StageEngine> (this), learned]
        {
            if (auto* self = safe.get())
            {
                self->saveKeyboardSettings();
                if (self->onLearnFinished)
                    self->onLearnFinished (learned);
            }
        });
        return;   // il tasto premuto per assegnare non suona
    }

    const int station = router.stationFor (deviceId);
    if (station < 0)
        return;   // porte non assegnate (DAW, InControl, tastiere non usate)

    activity[(size_t) station] = juce::jmax ((juce::uint32) 1, juce::Time::getMillisecondCounter());
    collectors[(size_t) station].addMessageToQueue (m);
}

KeyboardRouter& StageEngine::keyboards() { return router; }

void StageEngine::setZone (int station, const KeyZone& z)
{
    processors[(size_t) juce::jlimit (0, numStations - 1, station)].setZone (z);
}

KeyZone StageEngine::getZone (int station) const
{
    return processors[(size_t) juce::jlimit (0, numStations - 1, station)].getZone();
}

bool StageEngine::isStationDeviceConnected (int station) const
{
    const auto a = router.assignmentFor (station);
    return a.has_value() && openedMidiIds.contains (a->deviceId);
}

juce::uint32 StageEngine::lastActivityMs (int station) const
{
    return activity[(size_t) juce::jlimit (0, numStations - 1, station)].load();
}

void StageEngine::loadKeyboardSettings()
{
    if (auto xml = juce::parseXML (AppPaths::keyboardsFile()))
    {
        router.fromXml (*xml);
    }
    else
    {
        // Primo avvio: Keystation e Launchkey 61 si riconoscono dal nome; le Mini si assegnano con ASSEGNA.
        for (const auto& d : juce::MidiInput::getAvailableDevices())
        {
            const int s = KeyboardRouter::suggestStationForDeviceName (d.name);
            if (s >= 0 && ! router.assignmentFor (s).has_value())
                router.assign (s, d.identifier, d.name);
        }
    }

    if (auto xml = juce::parseXML (AppPaths::stationsFile()))
    {
        const auto tree = juce::ValueTree::fromXml (*xml);
        for (int i = 0; i < numStations && i < tree.getNumChildren(); ++i)
            processors[(size_t) i].setZone (KeyZone::fromValueTree (tree.getChild (i)));
    }
}

void StageEngine::saveKeyboardSettings() const
{
    if (auto xml = router.toXml())
        xml->writeTo (AppPaths::keyboardsFile());

    juce::ValueTree tree ("POSTAZIONI");
    for (const auto& p : processors)
        tree.appendChild (p.getZone().toValueTree(), nullptr);
    if (auto xml = tree.createXml())
        xml->writeTo (AppPaths::stationsFile());
}

void StageEngine::releaseStation (int station)
{
    if (! juce::isPositiveAndBelow (station, numStations))
        return;
    stations[(size_t) station].requestPanic();          // note-off per le note che suonano, pedale su
    resetProcessorPending[(size_t) station] = true;     // la mappa dei tasti tenuti si azzera nel blocco audio
}

void StageEngine::deviceDisconnected (const juce::String& deviceId)
{
    releaseStation (router.stationFor (deviceId));
}
