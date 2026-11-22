#include <juce_gui_basics/juce_gui_basics.h>

#include "ViewerComponent.h"

class Push2ViewerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "MBStudio91 Push2 Viewer"; }
    const juce::String getApplicationVersion() override { return PUSH2TEST_VERSION; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise (const juce::String&) override { window = std::make_unique<Window> (getApplicationName()); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff0b0b0b), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ViewerComponent(), true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (Push2ViewerApplication)
