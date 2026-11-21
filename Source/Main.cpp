#include <juce_gui_extra/juce_gui_extra.h>
#include "engine/StageEngine.h"
#include "plugins/PluginCatalog.h"
#include "plugins/ScannerProcess.h"
#include "settings/AppPaths.h"
#include "ui/MainComponent.h"
#include "ui/StageLookAndFeel.h"

class StageApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "MBStudio91 Stage"; }
    const juce::String getApplicationVersion() override { return STAGE_VERSION; }
    // true: il processo figlio di scansione e' una seconda istanza dello stesso eseguibile
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        if (ScannerProcess::startWorkerIfRequested (commandLine))
            return;   // siamo il processo figlio di scansione: nessuna finestra

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        const bool listExisted = AppPaths::pluginListFile().existsAsFile();
        catalog = std::make_unique<PluginCatalog> (AppPaths::pluginListFile(), AppPaths::deadMansPedalFile());
        catalog->load();

        engine = std::make_unique<StageEngine>();
        engine->start();
        engine->loadKeyboardSettings();

        auto content = std::make_unique<MainComponent> (*engine, *catalog);
        auto* contentPtr = content.get();
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), std::move (content));
        contentPtr->startInitialScanIfNeeded (listExisted);
    }

    void shutdown() override
    {
        mainWindow = nullptr;   // chiude prima le finestre dei plugin
        if (engine != nullptr)
        {
            engine->saveKeyboardSettings();
            engine->stop();     // salva le impostazioni audio
        }
        engine = nullptr;
        if (catalog != nullptr)
            catalog->save();
        catalog = nullptr;
        ScannerProcess::shutdownWorker();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, std::unique_ptr<juce::Component> content)
            : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content.release(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    StageLookAndFeel lookAndFeel;
    std::unique_ptr<PluginCatalog> catalog;
    std::unique_ptr<StageEngine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (StageApplication)
