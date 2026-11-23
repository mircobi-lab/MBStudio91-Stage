#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <optional>
#include <functional>

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

    static constexpr juce::uint32 learnTimeoutMs = 10000;   // ASSEGNA si annulla da solo dopo 10 s
    std::function<juce::uint32()> clock = [] { return juce::Time::getMillisecondCounter(); };

    static int suggestStationForDeviceName (const juce::String& name);

    // Che periferica e': serve all'intestazione della postazione.
    enum class DeviceKind { keyboard, surface, other };
    static DeviceKind kindForDeviceName (const juce::String& name);
    static juce::String kindLabel (DeviceKind);   // KEYB, SURF, MIDI

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement&);

private:
    mutable juce::CriticalSection lock;
    std::array<std::optional<Assignment>, numStations> assignments;
    int learning = -1;
    juce::uint32 learnStartedMs = 0;
    bool learnExpired() const;   // con il lock preso
};
