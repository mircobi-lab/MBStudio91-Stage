#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include "engine/StageEngine.h"
#include "plugins/PluginCatalog.h"

// Pannello di una postazione: tastiera assegnata, strumento + 3 effetti, volume, zona di tastiera.
class StationPanel final : public juce::Component,
                           private juce::Timer
{
public:
    StationPanel (StageEngine&, PluginCatalog&, int index0to3);
    ~StationPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void refreshPluginMenus();
    void refreshFromEngine();                              // dopo il richiamo di una patch
    std::function<void (const juce::String&)> onStatus;   // messaggi per la riga di stato
    std::function<void()> onUserEdit;                      // l'utente ha cambiato qualcosa: patch modificata

private:
    static constexpr int numSlots = 1 + Station::maxEffects;   // slot 0 = strumento

    void timerCallback() override;
    void slotChosen (int slot);
    void openEditor (int slot);
    void closeEditorFor (juce::AudioPluginInstance&);
    juce::AudioPluginInstance* instanceInSlot (int slot) const;
    void zoneControlChanged();
    void loadZoneIntoControls();
    void assignClicked();

    StageEngine& engine;
    PluginCatalog& catalog;
    const int index;
    Station& station;

    juce::Label title, deviceLabel, errorLabel;
    juce::TextButton assignButton { "ASSEGNA" }, muteButton { "MUTE" }, faderButton { "FADER VOL" };
    std::array<juce::Label, numSlots> slotLabels;
    std::array<juce::ComboBox, numSlots> slotMenus;
    std::array<juce::TextButton, numSlots> openButtons;
    juce::Slider volume { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider transpose { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 lowNote { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 highNote { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 gamma { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label transposeLabel { {}, "TRASP" }, lowLabel { {}, "DA" }, highLabel { {}, "A" },
                curveLabel { {}, "CURVA" }, gammaLabel { {}, "GAMMA" };
    juce::ComboBox curveMenu;

    juce::Rectangle<int> ledArea;
    juce::Colour ledColour { juce::Colours::black };

    std::vector<juce::PluginDescription> instrumentTypes, effectTypes;

    class EditorWindow;
    std::map<juce::AudioPluginInstance*, std::unique_ptr<EditorWindow>> editors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StationPanel)
};
