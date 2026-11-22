#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

// Tempo globale: BPM libero con tap, oppure MIDI clock esterno. Thread-safe:
// tap/setBpm dal message thread, clock dal thread MIDI, advance/position dal thread audio.
class TempoTracker
{
public:
    enum class Source { internal, external };

    void setSource (Source);
    Source getSource() const;

    void setBpm (double);                 // 20..300, tempo libero
    void tap (double nowSeconds);
    void handleClockMessage (const juce::MidiMessage&, double nowSeconds);

    double getBpm() const;
    bool isPlaying() const;
    bool isClockLost (double nowSeconds) const;

    void advance (int numSamples, double sampleRate);
    juce::AudioPlayHead::PositionInfo position() const;

    static constexpr double tapResetSeconds = 2.0;
    static constexpr double clockLostSeconds = 2.0;

private:
    std::atomic<Source> source { Source::internal };
    std::atomic<double> internalBpm { 120.0 }, externalBpm { 120.0 }, ppq { 0.0 };
    std::atomic<bool> externalPlaying { true };

    juce::CriticalSection lock;   // tap e clock (mai dal thread audio)
    juce::Array<double> taps;
    std::array<double, 25> clockTimes {};
    int clockCount = 0;           // tempi validi in clockTimes (finestra mobile)
    std::atomic<double> lastClock { 0.0 };
};

class StagePlayHead final : public juce::AudioPlayHead
{
public:
    explicit StagePlayHead (const TempoTracker& t) : tracker (t) {}
    juce::Optional<PositionInfo> getPosition() const override { return tracker.position(); }

private:
    const TempoTracker& tracker;
};
