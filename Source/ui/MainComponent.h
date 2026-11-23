#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "engine/StageEngine.h"
#include "plugins/PluginCatalog.h"
#include "StationPanel.h"
#include "PluginChooser.h"
#include "PatchPanel.h"
#include "control/SurfaceCoordinator.h"
#include "StageLookAndFeel.h"

// Finestra principale: barra superiore, 4 postazioni affiancate, riga di stato.
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent (StageEngine&, PluginCatalog&, PatchManager&, Library&, SurfaceCoordinator&);

    std::function<void()> onLibraryChanged;
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void startInitialScanIfNeeded (bool listFileExisted);

private:
    void timerCallback() override;
    void showAudioSettings();
    void startScan();
    void showScanMenu();
    void showPluginChooser();
    void loadChosenPlugins();
    void saveChosenPlugins (const juce::StringArray&);
    void showScanFolders();
    void loadScanFolders();
    void saveScanFolders (const juce::StringArray&);
    void refreshClockMenu();
    void tempoControlsChanged();

    StageEngine& engine;
    PluginCatalog& catalog;
    PatchManager& patches;
    SurfaceCoordinator& coordinator;

    juce::TextButton audioButton { "IMPOSTAZIONI AUDIO" }, scanButton { "SCANSIONA PLUGIN" },
                     panicButton { "PANIC" };
    MBStudioBrand brand;
    juce::Rectangle<int> brandArea, debossArea;
    juce::Label title, statusLabel, clockLabel { {}, "CLOCK DA" }, clockLost;
    juce::TextButton tempoSourceButton { "INT" }, tapButton { "TAP" };
    juce::Slider bpm { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ComboBox clockMenu;
    juce::StringArray clockIds;
    juce::MidiDeviceListConnection midiDevicesChanged;
    std::array<std::unique_ptr<StationPanel>, StageEngine::numStations> panels;
    std::unique_ptr<PatchPanel> patchPanel;

    juce::String statusMessage;
    juce::Component::SafePointer<juce::DialogWindow> audioSettingsWindow, foldersWindow, chooserWindow;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
