#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <optional>

// Quale tastiera (per identificativo del dispositivo MIDI) suona quale postazione.
// Thread-safe: usato dal thread MIDI e dal message thread.
class KeyboardRouter
{
public:
    static constexpr int numStations = 4;

    struct Assignment { juce::String deviceId, deviceName; };

    void assign (int station0to3, const juce::String& deviceId, const juce::String& deviceName);
    void clear (int station0to3);
    int stationFor (const juce::String& deviceId) const;
    std::optional<Assignment> assignmentFor (int station0to3) const;

    void startLearn (int station0to3);
    void cancelLearn();
    int learningStation() const;
    int handleLearn (const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage&);

    static int suggestStationForDeviceName (const juce::String& name);

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement&);

private:
    mutable juce::CriticalSection lock;
    std::array<std::optional<Assignment>, numStations> assignments;
    int learning = -1;
};
