#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include "Station.h"
#include "NoteProcessor.h"
#include "KeyboardRouter.h"
#include "TempoTracker.h"
#include <functional>

// Motore audio: 4 postazioni mescolate su un'uscita stereo, MIDI dalle tastiere, PANIC.
class StageEngine : private juce::AudioIODeviceCallback,
                    private juce::MidiInputCallback
{
public:
    static constexpr int numStations = 4;

    StageEngine();
    ~StageEngine() override;

    void start();   // ripristina il device audio salvato e apre gli ingressi MIDI
    void stop();    // chiude il device e salva le impostazioni audio

    juce::AudioDeviceManager& deviceManager();
    Station& station (int index0to3);

    void panicAll();
    void setMasterGainDb (float);

    double getSampleRate() const;
    int getBlockSize() const;

    // Usati dal callback audio e dai test.
    void prepare (double sampleRate, int blockSize);
    void renderBlock (juce::AudioBuffer<float>& out);
    void injectMidiForTest (int station0to3, const juce::MidiBuffer&);   // consumato al prossimo renderBlock

    // MIDI in arrivo da un dispositivo: apprendimento, poi la postazione assegnata (o nulla).
    void routeIncoming (const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage&);

    KeyboardRouter& keyboards();
    void setZone (int station0to3, const KeyZone&);
    KeyZone getZone (int station0to3) const;
    bool isStationDeviceConnected (int station0to3) const;
    juce::uint32 lastActivityMs (int station0to3) const;

    // Una tastiera assegnata e' sparita: la sua postazione rilascia note e pedale.
    void deviceDisconnected (const juce::String& deviceId);

    TempoTracker& tempo();
    void setClockSourceDevice (const juce::String& deviceId);   // "" = nessuno
    juce::String getClockSourceDevice() const;

    void loadKeyboardSettings();        // tastiere.xml + postazioni.xml
    void saveKeyboardSettings() const;

    std::function<void (int station)> onLearnFinished;   // chiamato sul message thread

    // Dispositivi riservati al controllo (Launchpad): non suonano le postazioni.
    static bool isReservedControlDevice (const juce::String& deviceName);

    // Ingressi da chiudere (abilitati ma non piu' presenti) e da aprire (presenti, non riservati).
    struct MidiInputChanges { juce::StringArray toDisable, toOpen; };
    static MidiInputChanges planMidiInputChanges (const juce::StringArray& enabledIds,
                                                  const juce::Array<juce::MidiDeviceInfo>& available);

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    void openAllMidiInputs();
    void releaseStation (int station);   // qualsiasi thread: note-off, pedale su, mappa azzerata
    void saveDeviceState();

    juce::AudioDeviceManager devices;
    TempoTracker tempoTracker;
    StagePlayHead playHead { tempoTracker };
    mutable juce::CriticalSection clockSourceLock;
    juce::String clockSource;
    std::array<Station, numStations> stations;
    std::array<juce::MidiMessageCollector, numStations> collectors;
    std::array<NoteProcessor, numStations> processors;
    std::array<std::atomic<bool>, numStations> resetProcessorPending {};
    std::array<std::atomic<juce::uint32>, numStations> activity {};
    std::array<juce::MidiBuffer, numStations> rawMidi;
    KeyboardRouter router;

    juce::SpinLock injectLock;
    std::array<juce::MidiBuffer, numStations> injected, stationMidi;

    juce::SmoothedValue<float> masterGain;
    // Limitatore di sicurezza: trasparente sotto -1 dB, attacco istantaneo, rilascio 100 ms.
    float limiterEnvelope = 0.0f, limiterRelease = 0.0f;
    static constexpr float limiterThreshold = 0.891251f;   // -1 dBFS
    std::atomic<float> masterTarget { 1.0f };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBlockSize { 512 };

    juce::MidiDeviceListConnection midiDevicesChanged;
    juce::StringArray openedMidiIds;   // ingressi abilitati da noi
    bool started = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (StageEngine)
    JUCE_DECLARE_NON_COPYABLE (StageEngine)
};
