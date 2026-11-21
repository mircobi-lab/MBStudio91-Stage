#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include "NoteTracker.h"

// Una postazione: strumento + fino a 3 effetti + volume/mute.
// I metodi set* si chiamano dal message thread; renderAdd dal thread audio.
class Station
{
public:
    static constexpr int maxEffects = 3;

    Station();
    ~Station();

    void prepare (double sampleRate, int maxBlockSize);

    void setInstrument (std::unique_ptr<juce::AudioPluginInstance>);        // nullptr = svuota
    bool setEffect (int slot0to2, std::unique_ptr<juce::AudioPluginInstance>);
    juce::AudioPluginInstance* getInstrument() const;
    juce::AudioPluginInstance* getEffect (int slot) const;

    void setLoadError (juce::String);
    juce::String getLoadError() const;

    void setGainDb (float);
    float getGainDb() const;
    void setMuted (bool);
    bool isMuted() const;

    // Thread-safe. Il MIDI di panic parte al prossimo blocco audio; le code (riverberi, delay)
    // vengono azzerate subito dopo sul message thread, mai sul thread audio.
    void requestPanic();

    // Chiamato sul message thread prima che un'istanza venga rimossa (per chiudere il suo editor).
    std::function<void (juce::AudioPluginInstance&)> onBeforeInstanceRemoved;

    // Thread audio: SOMMA l'uscita della postazione in `out`.
    void renderAdd (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn);

private:
    using InstancePtr = std::unique_ptr<juce::AudioPluginInstance>;

    void prepareInstance (juce::AudioPluginInstance&) const;
    void retire (InstancePtr);   // message thread: avvisa, rilascia, distrugge
    void resetTailsOnMessageThread();
    int channelsNeeded() const;  // da chiamare con il lock preso

    juce::SpinLock lock;          // protegge le istanze e lo scratch
    InstancePtr instrument;
    std::array<InstancePtr, maxEffects> effects;

    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midiScratch, emptyMidi;
    NoteTracker tracker;

    juce::SmoothedValue<float> gain;
    std::atomic<float> targetGain { 1.0f };
    std::atomic<bool> muted { false }, panicPending { false };

    double sampleRate = 44100.0;
    int blockSize = 512;

    mutable juce::CriticalSection errorLock;
    juce::String loadError;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Station)
    JUCE_DECLARE_NON_COPYABLE (Station)
};
