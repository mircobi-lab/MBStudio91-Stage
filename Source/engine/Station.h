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
    static constexpr double tailSeconds = 3.0;   // coda della catena precedente dopo un cambio patch

    struct Chain
    {
        std::unique_ptr<juce::AudioPluginInstance> instrument;
        std::array<std::unique_ptr<juce::AudioPluginInstance>, maxEffects> effects;
    };

    Station();
    ~Station();

    void prepare (double sampleRate, int maxBlockSize);
    void setPlayHead (juce::AudioPlayHead*);   // dato a ogni plugin caricato

    void setInstrument (std::unique_ptr<juce::AudioPluginInstance>);        // nullptr = svuota
    bool setEffect (int slot0to2, std::unique_ptr<juce::AudioPluginInstance>);
    juce::AudioPluginInstance* getInstrument() const;
    juce::AudioPluginInstance* getEffect (int slot) const;

    // Message thread. La catena attiva diventa coda (note tenute e pedale restano li'),
    // `chain` diventa attiva. Una coda gia' presente sfuma in 20 ms.
    void setChain (Chain);
    // Message thread: istanze delle code terminate (onBeforeInstanceRemoved gia' chiamato).
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> takeFinishedInstances();
    bool hasTailChain() const;
    void runWithLockHeldForTest (const std::function<void()>&);

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

    enum class TailState { none, held, fading, finished };

    // Una catena che suona: istanze, note tenute, buffer di lavoro e (per le code) lo stato di uscita.
    struct LiveChain
    {
        Chain chain;
        NoteTracker tracker;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer;
        TailState state = TailState::none;
        int samplesLeft = 0;      // coda: campioni prima di sfumare
        float fadeGain = 1.0f, fadeStep = 0.0f;
    };

    static constexpr int maxTails = 2;

    void prepareInstance (juce::AudioPluginInstance&) const;
    void prepareChain (LiveChain&);                 // con il lock preso
    void retire (InstancePtr);                      // message thread: avvisa, rilascia, distrugge
    void retireChain (Chain&);
    void resetTailsOnMessageThread();
    static int channelsNeeded (const Chain&);
    static bool anySustain (const NoteTracker&);
    void startFade (LiveChain&, double seconds) const;
    void processChain (LiveChain&, int numSamples);

    juce::SpinLock lock;          // protegge catene e buffer
    LiveChain active;
    std::array<LiveChain, maxTails> tails;

    juce::MidiBuffer emptyMidi, incoming, deferredMidi;   // deferred: MIDI arrivato mentre il lock era occupato
    juce::AudioBuffer<float> mix;
    std::vector<float> fadeGains;                          // guadagno per campione delle code
    std::atomic<bool> tailFinished { false };              // evita di prendere il lock se non c'e' nulla da ritirare

    juce::SmoothedValue<float> gain;
    std::atomic<float> targetGain { 1.0f };
    std::atomic<bool> muted { false }, panicPending { false };

    double sampleRate = 44100.0;
    int blockSize = 512;
    juce::AudioPlayHead* playHead = nullptr;

    mutable juce::CriticalSection errorLock;
    juce::String loadError;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Station)
    JUCE_DECLARE_NON_COPYABLE (Station)
};
