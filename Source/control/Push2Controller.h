#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <optional>
#include "ControlSurface.h"
#include "Push2Mapping.h"
#include "push2/Push2Display.h"

// Ableton Push 2 come selettore di patch. Lavora sulla porta User, lasciando libera
// la Live per Ableton, e dice al Push di passare in User mode: appena acceso e' in
// Live mode e quello che arriva sulla User lo ignora. All'uscita lo rimette in Live
// mode. Aggiorna i LED a 20 Hz,
// scrive il nome della patch sul display e tratta PANIC come pressione lunga.
class Push2Controller final : public ControlSurface,
                              private juce::Timer,
                              private juce::MidiInputCallback
{
public:
    static constexpr juce::uint32 panicHoldMs = 500;

    explicit Push2Controller (juce::Typeface::Ptr displayFont, bool openDevices = true);
    ~Push2Controller() override;

    juce::String getName() const override { return "Ableton Push 2"; }
    juce::String getDetail() const override { return portName; }

    // Fra le porte del Push 2 sceglie la Live, o la User se preferUserPort.
    // Se ce n'e' una sola prende quella. Vuoto se il Push non c'e'.
    static std::optional<juce::MidiDeviceInfo> choosePort (const juce::Array<juce::MidiDeviceInfo>&,
                                                           bool preferUserPort);
    void setUseUserPort (bool);

private:
    static juce::MidiMessage sysexFor (const std::vector<std::uint8_t>&);
public:
    bool isConnected() const override;
    void showState (const SurfaceState&) override;

    // Message thread. Pubblici per i test.
    void handleAction (SurfaceAction, juce::uint32 nowMs);
    void tick (juce::uint32 nowMs);

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void reconnect();
    void send (const std::vector<juce::MidiMessage>&);
    void restorePalette();

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::MidiDeviceListConnection devicesChanged;
    juce::Typeface::Ptr font;
    Push2Display display;

    SurfaceState wanted, shown;
    bool needsFullRefresh = true, screenDirty = true;
    std::vector<juce::MidiMessage> originalPalette;   // voci lette dal Push, per rimetterle
    juce::String portName;
    bool useUserPort = true;

    juce::uint32 panicPressedAt = 0;
    bool panicPressed = false, panicFired = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Push2Controller)
    JUCE_DECLARE_NON_COPYABLE (Push2Controller)
};
