#include <juce_gui_extra/juce_gui_extra.h>
#include "control/LaunchpadController.h"
#include "control/Push2Controller.h"
#include "control/SurfaceCoordinator.h"
#include "engine/StageEngine.h"
#include "patches/PatchManager.h"
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
        engine->loadKeyboardSettings();
        engine->start();

        // Libreria delle patch, con una copia di sicurezza al giorno.
        const auto libFile = AppPaths::libraryFile();
        juce::String libraryWarning;
        if (libFile.existsAsFile())
        {
            if (library.loadFrom (libFile))
            {
                const auto backup = Library::dailyBackupFor (libFile, juce::Time::getCurrentTime());
                if (! backup.existsAsFile())
                    libFile.copyFileTo (backup);
            }
            else
            {
                // Il file illeggibile non viene sovrascritto: si sposta da parte e lo si dice.
                const auto moved = Library::setAsideUnreadable (libFile);
                libraryWarning = "La libreria delle patch non si e' potuta leggere ed e' stata messa da parte come\n"
                               + (moved != juce::File() ? moved.getFullPathName() : libFile.getFullPathName())
                               + "\n\nNella stessa cartella ci sono le copie giornaliere (libreria-AAAA-MM-GG.xml): "
                                 "per recuperarne una, chiudi l'app e rinominala libreria.xml.";
            }
        }

        patches = std::make_unique<PatchManager> (*engine, library,
            [this] (const juce::String& id, double sr, int bs, PatchManager::LoadCallback cb)
            {
                if (const auto desc = catalog->findByPortableId (id))
                    catalog->createInstanceAsync (*desc, sr, bs, std::move (cb));
                else
                    cb (nullptr, "Plugin non installato: " + id.fromFirstOccurrenceOf ("|", false, false)
                                                              .fromFirstOccurrenceOf ("|", false, false)
                                                              .upToFirstOccurrenceOf ("|", false, false));
            });

        coordinator = std::make_unique<SurfaceCoordinator> (*engine, library, *patches);
        launchpad = std::make_unique<LaunchpadController>();
        coordinator->addSurface (*launchpad);

        push2 = std::make_unique<Push2Controller> (lookAndFeel.getTypefaceForFont (juce::Font (juce::FontOptions())));
        coordinator->addSurface (*push2);

        auto content = std::make_unique<MainComponent> (*engine, *catalog, *patches, library, *coordinator);
        content->onLibraryChanged = [this] { saveLibrary(); };
        auto* contentPtr = content.get();
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), std::move (content));
        contentPtr->startInitialScanIfNeeded (listExisted);

        if (libraryWarning.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Libreria delle patch", libraryWarning);
    }

    void shutdown() override
    {
        mainWindow = nullptr;   // chiude prima le finestre dei plugin
        coordinator = nullptr;
        launchpad = nullptr;
        push2 = nullptr;
        if (patches != nullptr)
            saveLibrary();
        patches = nullptr;
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
    void saveLibrary() { library.saveTo (AppPaths::libraryFile()); }

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
    Library library;
    std::unique_ptr<PluginCatalog> catalog;
    std::unique_ptr<StageEngine> engine;
    std::unique_ptr<PatchManager> patches;
    std::unique_ptr<LaunchpadController> launchpad;
    std::unique_ptr<Push2Controller> push2;
    std::unique_ptr<SurfaceCoordinator> coordinator;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (StageApplication)
