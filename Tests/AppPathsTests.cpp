#include <juce_core/juce_core.h>
#include "settings/AppPaths.h"

class AppPathsTests final : public juce::UnitTest
{
public:
    AppPathsTests() : juce::UnitTest ("AppPaths", "Stage") {}

    void runTest() override
    {
        beginTest ("la cartella dati e' ~/Music/MBStudio91 Stage ed esiste");
        const auto dir = AppPaths::dataDir();
        expectEquals (dir.getFileName(), juce::String ("MBStudio91 Stage"));
        expectEquals (dir.getParentDirectory().getFileName(), juce::String ("Music"));
        expect (dir.isDirectory());

        beginTest ("i file stanno nella cartella dati");
        expect (AppPaths::pluginListFile().getParentDirectory() == dir);
        expectEquals (AppPaths::pluginListFile().getFileName(), juce::String ("plugins.xml"));
        expectEquals (AppPaths::deadMansPedalFile().getFileName(), juce::String ("scan-in-corso.txt"));
        expectEquals (AppPaths::machineSettingsFile().getFileName(), juce::String ("macchina.settings"));
    }
};

static AppPathsTests appPathsTests;
