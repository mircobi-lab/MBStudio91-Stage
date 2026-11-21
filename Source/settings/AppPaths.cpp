#include "AppPaths.h"

namespace AppPaths
{
    juce::File dataDir()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                             .getChildFile ("MBStudio91 Stage");
        if (! dir.isDirectory())
            dir.createDirectory();
        return dir;
    }

    juce::File pluginListFile()      { return dataDir().getChildFile ("plugins.xml"); }
    juce::File deadMansPedalFile()   { return dataDir().getChildFile ("scan-in-corso.txt"); }
    juce::File machineSettingsFile() { return dataDir().getChildFile ("macchina.settings"); }
    juce::File keyboardsFile()       { return dataDir().getChildFile ("tastiere.xml"); }
    juce::File stationsFile()        { return dataDir().getChildFile ("postazioni.xml"); }
    juce::File libraryFile()         { return dataDir().getChildFile ("libreria.xml"); }
}
