#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "ControlSurface.h"
#include "LaunchpadMapping.h"

// Launchpad Mini MK1: apre il primo dispositivo con "Launchpad" nel nome, lo riapre se viene
// ricollegato, aggiorna i LED a 20 Hz e trasforma la pressione lunga di PANIC in un'azione.
class LaunchpadController final : public ControlSurface,
                                  private juce::Timer,
                                  private juce::MidiInputCallback
{
public:
    static constexpr juce::uint32 panicHoldMs = 500;

    explicit LaunchpadController (bool openDevices = true);
    ~LaunchpadController() override;

    juce::String getName() const override { return "Launchpad Mini"; }
    bool isConnected() const override;
    void showState (const SurfaceState&) override;

    // Message thread. Pubblici per i test.
    void handleAction (SurfaceAction, juce::uint32 nowMs);
    void tick (juce::uint32 nowMs);

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void reconnect();

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::MidiDeviceListConnection devicesChanged;

    SurfaceState wanted, shown;
    bool needsFullRefresh = true;

    juce::uint32 panicPressedAt = 0;
    bool panicPressed = false, panicFired = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (LaunchpadController)
    JUCE_DECLARE_NON_COPYABLE (LaunchpadController)
};
