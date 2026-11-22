#pragma once

#include <juce_core/juce_core.h>

// Percorsi dei dati dell'app: tutto in ~/Music/MBStudio91 Stage/
namespace AppPaths
{
    juce::File dataDir();             // crea la cartella se manca
    juce::File pluginListFile();      // plugins.xml
    juce::File deadMansPedalFile();   // scan-in-corso.txt
    juce::File machineSettingsFile(); // macchina.settings
    juce::File keyboardsFile();       // tastiere.xml
    juce::File chosenPluginsFile();   // plugin-scelti.txt, uno per riga
    juce::File scanFoldersFile();     // cartelle-plugin.txt, una cartella per riga
    juce::File stationsFile();        // postazioni.xml
    juce::File libraryFile();         // libreria.xml
}
