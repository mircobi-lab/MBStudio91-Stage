#include <juce_gui_extra/juce_gui_extra.h>
#include "plugins/ScannerProcess.h"

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

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        ScannerProcess::shutdownWorker();
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            centreWithSize (1100, 700);
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (StageApplication)
