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
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        devices.removeMidiInputDeviceCallback (d.identifier, this);

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

void StageEngine::openAllMidiInputs()
{
    for (const auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (isReservedControlDevice (d.name))
            continue;

        if (! devices.isMidiInputDeviceEnabled (d.identifier))
            devices.setMidiInputDeviceEnabled (d.identifier, true);

        devices.removeMidiInputDeviceCallback (d.identifier, this);   // evita doppie registrazioni
        devices.addMidiInputDeviceCallback (d.identifier, this);
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

    keyboardMidi.reset (sampleRate);
}

void StageEngine::injectMidiForTest (int station0to3, const juce::MidiBuffer& midi)
{
    const juce::SpinLock::ScopedLockType sl (injectLock);
    injected[(size_t) juce::jlimit (0, numStations - 1, station0to3)].addEvents (midi, 0, -1, 0);
}

void StageEngine::renderBlock (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiForStation0)
{
    const int numSamples = out.getNumSamples();
    out.clear();

    for (auto& b : stationMidi)
        b.clear();
    stationMidi[0].addEvents (midiForStation0, 0, numSamples, 0);

    {
        const juce::SpinLock::ScopedTryLockType sl (injectLock);
        if (sl.isLocked())
            for (size_t i = 0; i < injected.size(); ++i)
            {
                stationMidi[i].addEvents (injected[i], 0, numSamples, 0);
                injected[i].clear();
            }
    }

    for (size_t i = 0; i < stations.size(); ++i)
        stations[i].renderAdd (out, stationMidi[i]);

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
    juce::MidiBuffer keys;
    keyboardMidi.removeNextBlockOfMessages (keys, numSamples);
    renderBlock (out, keys);
}

void StageEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void StageEngine::audioDeviceStopped() {}

void StageEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    keyboardMidi.addMessageToQueue (m);
}
