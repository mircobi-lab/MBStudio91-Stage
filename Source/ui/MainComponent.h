#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "engine/StageEngine.h"
#include "plugins/PluginCatalog.h"
#include "StationPanel.h"
#include <functional>

// Finestra principale: barra superiore, 4 postazioni affiancate, riga di stato.
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
    void timerCallback() override;
    void showAudioSettings();
    void startScan (std::function<void()> afterwards = {});
    void showScanFolders();
    void loadScanFolders();
    void saveScanFolders (const juce::StringArray&);
    void restoreSavedPlugins();

    StageEngine& engine;
    PluginCatalog& catalog;

    juce::TextButton audioButton { "IMPOSTAZIONI AUDIO" }, foldersButton { "CARTELLE" },
                     scanButton { "SCANSIONA PLUGIN" }, panicButton { "PANIC" };
    juce::Label title, statusLabel;
    std::array<std::unique_ptr<StationPanel>, StageEngine::numStations> panels;

    juce::String statusMessage;
    juce::Component::SafePointer<juce::DialogWindow> audioSettingsWindow, foldersWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    bool scannedForMissingPlugins = false;
    int pluginsToRestore = 0, pluginsRestored = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
