#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include "Station.h"

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
    void renderBlock (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiForStation0);
    void injectMidiForTest (int station0to3, const juce::MidiBuffer&);   // consumato al prossimo renderBlock

    // Dispositivi riservati al controllo (Launchpad): non suonano le postazioni.
    static bool isReservedControlDevice (const juce::String& deviceName);

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    void openAllMidiInputs();
    void saveDeviceState();

    juce::AudioDeviceManager devices;
    std::array<Station, numStations> stations;
    juce::MidiMessageCollector keyboardMidi;

    juce::SpinLock injectLock;
    std::array<juce::MidiBuffer, numStations> injected, stationMidi;

    juce::SmoothedValue<float> masterGain;
    std::atomic<float> masterTarget { 1.0f };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBlockSize { 512 };

    juce::MidiDeviceListConnection midiDevicesChanged;
    bool started = false;

    JUCE_DECLARE_NON_COPYABLE (StageEngine)
};
