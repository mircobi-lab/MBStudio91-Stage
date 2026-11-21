#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include "engine/StageEngine.h"
#include "plugins/PluginCatalog.h"

// Interfaccia minima di Fase 1: postazione 1, impostazioni audio, scansione plugin, PANIC.
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent (StageEngine&, PluginCatalog&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void startInitialScanIfNeeded (bool listFileExisted);

private:
    static constexpr int numSlots = 1 + Station::maxEffects;   // slot 0 = strumento

    void timerCallback() override;
    void refreshPluginMenus();
    void slotChosen (int slot);
    void openEditor (int slot);
    void closeEditorFor (juce::AudioPluginInstance&);
    void showAudioSettings();
    void startScan();
    juce::AudioPluginInstance* instanceInSlot (int slot) const;

    StageEngine& engine;
    PluginCatalog& catalog;
    Station& station;

    juce::TextButton audioButton { "IMPOSTAZIONI AUDIO" }, scanButton { "SCANSIONA PLUGIN" },
                     panicButton { "PANIC" }, muteButton { "MUTE" };
    juce::Label title, stationTitle, statusLabel, volumeLabel { {}, "VOLUME" };
    std::array<juce::Label, numSlots> slotLabels;
    std::array<juce::ComboBox, numSlots> slotMenus;
    std::array<juce::TextButton, numSlots> openButtons;
    juce::Slider volume { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };

    std::vector<juce::PluginDescription> instrumentTypes, effectTypes;

    class EditorWindow;
    std::map<juce::AudioPluginInstance*, std::unique_ptr<EditorWindow>> editors;

    juce::String scanMessage;
    juce::Component::SafePointer<juce::DialogWindow> audioSettingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
