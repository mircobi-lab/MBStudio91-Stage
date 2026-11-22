#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Push2Display.h"
#include "Push2Screen.h"

#include <array>
#include <map>
#include <optional>

class MainComponent final : public juce::Component,
                            private juce::MidiInputCallback,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Setup
    {
        juce::String name;   // nome del plugin
        juce::Colour colour;
        int device;          // indice in deviceNames
        TextPart::Face nameFace = TextPart::Face::logoBold;
    };

    // MIDI
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleMessage (const juce::MidiMessage&);
    void timerCallback() override;
    bool connectMidi();
    void disconnectMidi (bool restore);
    void send (const juce::MidiMessage&);
    void sendSysex (const std::vector<std::uint8_t>& bytes);

    // Push 2
    void applyPalette();
    void restorePalette();
    void refreshLeds();
    void allLedsOff();
    void selectSetup (int index);
    void renderDisplay();

    void updateStatus();

    static constexpr int numSetups = 8;
    static constexpr int firstPaletteIndex = 1; // 1..8 colori pieni, 9..16 attenuati
    std::array<Setup, numSetups> setups;
    int activeSetup = 0;

    std::unique_ptr<juce::MidiInput> midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;
    juce::String midiPortName;

    std::optional<juce::String> firmware;
    std::map<int, std::vector<std::uint8_t>> paletteBackup; // indice -> voce originale (comando 0x03)
    bool paletteApplied = false;
    int paletteWaitTicks = 0;

    std::array<bool, 128> padDown {};
    juce::String lastEvent { "Premi un pad o un tasto sopra il display" };

    Push2Display display;
    Push2Display::State shownDisplayState = Push2Display::State::searching;

    juce::Typeface::Ptr bebas;
    Faces faces;
    juce::Label title, midiStatus, displayStatus, firmwareStatus, eventLabel;
    std::array<juce::TextButton, numSetups> setupButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
