#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

// Scansione dei plugin in un processo figlio: se un plugin manda in crash la scansione,
// muore il figlio e non l'app. Il figlio e' lo stesso eseguibile avviato con un UID speciale.
namespace ScannerProcess
{
    constexpr const char* processUID = "mbstudio91stagescanner";

    // Da chiamare in JUCEApplication::initialise. Se la riga di comando e' quella del figlio,
    // avvia il lavoratore e restituisce true: l'app non deve aprire finestre.
    bool startWorkerIfRequested (const juce::String& commandLine);
    void shutdownWorker();

    // Scanner da passare a KnownPluginList::setCustomScanner nel processo principale.
    std::unique_ptr<juce::KnownPluginList::CustomScanner> makeOutOfProcessScanner();
}
