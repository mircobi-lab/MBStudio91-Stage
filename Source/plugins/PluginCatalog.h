#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <optional>

// "<formato>|<produttore>|<nome>|<uniqueId esadecimale>", es. "VST3|LennarDigital|Sylenth1|4c4e5331"
juce::String makePortableId (const juce::PluginDescription&);

// Registra i formati ospitati da Stage: VST3 sempre, AudioUnit su macOS.
void addStageFormats (juce::AudioPluginFormatManager&);

class PluginCatalog
{
public:
    PluginCatalog (juce::File listFile, juce::File deadMansPedal);
    ~PluginCatalog();

    juce::AudioPluginFormatManager& formats();
    juce::KnownPluginList& list();

    void load();
    void save() const;

    // Scansione fuori processo, su un thread separato. onDone arriva sul message thread.
    void scanAsync (std::function<void (int found, juce::StringArray failed)> onDone);
    bool isScanning() const;

    std::optional<juce::PluginDescription> findByPortableId (const juce::String&) const;

    using LoadCallback = std::function<void (std::unique_ptr<juce::AudioPluginInstance>, juce::String error)>;
    void createInstanceAsync (const juce::PluginDescription&, double sampleRate, int blockSize, LoadCallback);

private:
    class ScanThread;

    juce::File listFile, deadMansPedal;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownList;
    std::unique_ptr<ScanThread> scanThread;

    JUCE_DECLARE_NON_COPYABLE (PluginCatalog)
};
