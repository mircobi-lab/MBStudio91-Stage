#include "PluginCatalog.h"
#include "ScannerProcess.h"

juce::String makePortableId (const juce::PluginDescription& d)
{
    return d.pluginFormatName + "|" + d.manufacturerName + "|" + d.name + "|"
         + juce::String::toHexString ((juce::uint32) d.uniqueId);
}

void addStageFormats (juce::AudioPluginFormatManager& fm)
{
   #if JUCE_PLUGINHOST_VST3
    fm.addFormat (std::make_unique<juce::VST3PluginFormat>());
   #endif
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    fm.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
   #endif
}

//==============================================================================
// Thread di scansione: percorre le cartelle standard di ogni formato.
// Ogni file viene esaminato nel processo figlio (ScannerProcess).
class PluginCatalog::ScanThread final : public juce::Thread
{
public:
    ScanThread (PluginCatalog& c, juce::StringArray extra, std::function<void (int, juce::StringArray)> done)
        : juce::Thread ("MBStudio91 Stage plugin scan"), catalog (c),
          extraFolders (std::move (extra)), onDone (std::move (done)) {}

    ~ScanThread() override { stopThread (30000); }

    void run() override
    {
        const int before = catalog.knownList.getNumTypes();
        juce::StringArray failed;

        for (auto* format : catalog.formatManager.getFormats())
        {
            juce::PluginDirectoryScanner scanner (catalog.knownList, *format,
                                                  PluginCatalog::searchPathFor (*format, extraFolders),
                                                  true, catalog.deadMansPedal, false);
            juce::String name;
            while (! threadShouldExit() && scanner.scanNextFile (true, name)) {}
            failed.addArray (scanner.getFailedFiles());
        }

        catalog.knownList.scanFinished();

        const int found = catalog.knownList.getNumTypes() - before;
        juce::MessageManager::callAsync ([safe = juce::WeakReference<ScanThread> (this), found, failed]
        {
            if (auto* self = safe.get())
            {
                self->catalog.save();
                if (self->onDone)
                    self->onDone (found, failed);
            }
        });
    }

private:
    PluginCatalog& catalog;
    juce::StringArray extraFolders;
    std::function<void (int, juce::StringArray)> onDone;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ScanThread)
};

//==============================================================================
PluginCatalog::PluginCatalog (juce::File list, juce::File pedal)
    : listFile (std::move (list)), deadMansPedal (std::move (pedal))
{
    addStageFormats (formatManager);
}

PluginCatalog::~PluginCatalog()
{
    scanThread = nullptr;
}

// Alle AudioUnit non servono cartelle: le trova il sistema. Le cartelle in piu'
// valgono solo per i formati che cercano file, cioe' VST3.
juce::FileSearchPath PluginCatalog::searchPathFor (juce::AudioPluginFormat& format,
                                                   const juce::StringArray& extra)
{
    auto path = format.getDefaultLocationsToSearch();
    if (path.getNumPaths() == 0)
        return path;

    for (const auto& f : extra)
    {
        const juce::File dir (f);
        if (dir.isDirectory())
            path.addIfNotAlreadyThere (dir);
    }
    return path;
}

void PluginCatalog::setExtraFolders (juce::StringArray folders)
{
    folders.removeEmptyStrings();
    folders.removeDuplicates (true);
    extraFolders = std::move (folders);
}

juce::StringArray PluginCatalog::getExtraFolders() const { return extraFolders; }

juce::StringArray PluginCatalog::foldersToScan() const
{
    juce::StringArray all;
    for (auto* format : formatManager.getFormats())
    {
        const auto path = searchPathFor (*format, extraFolders);
        for (int i = 0; i < path.getNumPaths(); ++i)
            all.addIfNotAlreadyThere (path[i].getFullPathName());
    }
    return all;
}

juce::AudioPluginFormatManager& PluginCatalog::formats() { return formatManager; }
juce::KnownPluginList& PluginCatalog::list()             { return knownList; }

void PluginCatalog::load()
{
    if (auto xml = juce::parseXML (listFile))
        knownList.recreateFromXml (*xml);

    // Se l'ultima scansione e' stata interrotta da un crash, il plugin colpevole va in blacklist.
    juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (knownList, deadMansPedal);
}

void PluginCatalog::save() const
{
    if (auto xml = knownList.createXml())
        xml->writeTo (listFile);
}

void PluginCatalog::scanAsync (std::function<void (int, juce::StringArray)> onDone)
{
    if (isScanning())
        return;

    knownList.setCustomScanner (ScannerProcess::makeOutOfProcessScanner());
    scanThread = std::make_unique<ScanThread> (*this, extraFolders, std::move (onDone));
    scanThread->startThread();
}

bool PluginCatalog::isScanning() const
{
    return scanThread != nullptr && scanThread->isThreadRunning();
}

std::optional<juce::PluginDescription> PluginCatalog::findByPortableId (const juce::String& id) const
{
    for (const auto& d : knownList.getTypes())
        if (makePortableId (d) == id)
            return d;
    return std::nullopt;
}

void PluginCatalog::createInstanceAsync (const juce::PluginDescription& desc, double sampleRate,
                                         int blockSize, LoadCallback callback)
{
    formatManager.createPluginInstanceAsync (desc, sampleRate, blockSize,
        [cb = std::move (callback), name = desc.name] (std::unique_ptr<juce::AudioPluginInstance> inst,
                                                       const juce::String& error)
        {
            if (inst == nullptr)
                cb (nullptr, "Impossibile caricare " + name + (error.isNotEmpty() ? ": " + error : juce::String()));
            else
                cb (std::move (inst), {});
        });
}
