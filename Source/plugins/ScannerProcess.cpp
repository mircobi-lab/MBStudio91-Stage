#include "ScannerProcess.h"
#include "PluginCatalog.h"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace
{
    // 0 = timeout predefinito di JUCE per il ping tra i processi (i ping girano su thread propri,
    // quindi un plugin che blocca il figlio NON viene rilevato da qui: vedi perFileDeadlineMs).
    constexpr int workerTimeoutMs = 0;

    // Tempo massimo per esaminare un singolo file. Oltre, il figlio viene chiuso
    // e il file finisce nella lista dei plugin scartati.
    constexpr juce::uint32 perFileDeadlineMs = 120000;

    //======================================================================
    // Processo figlio: riceve <formato, identificatore>, risponde con un XML <LIST> di PluginDescription.
    class Worker final : private juce::ChildProcessWorker,
                         private juce::AsyncUpdater
    {
    public:
        Worker() { addStageFormats (formatManager); }

        using juce::ChildProcessWorker::initialiseFromCommandLine;

    private:
        void handleMessageFromCoordinator (const juce::MemoryBlock& mb) override
        {
            if (mb.isEmpty())
                return;

            const std::lock_guard<std::mutex> guard (mutex);
            pending.push (mb);
            triggerAsyncUpdate();   // scansione sul message thread (necessario per gli AU)
        }

        void handleConnectionLost() override
        {
            juce::JUCEApplicationBase::quit();
        }

        void handleAsyncUpdate() override
        {
            for (;;)
            {
                juce::MemoryBlock block;
                {
                    const std::lock_guard<std::mutex> guard (mutex);
                    if (pending.empty())
                        return;
                    block = pending.front();
                    pending.pop();
                }
                sendResults (doScan (block));
            }
        }

        juce::OwnedArray<juce::PluginDescription> doScan (const juce::MemoryBlock& block)
        {
            juce::MemoryInputStream stream (block, false);
            const auto formatName = stream.readString();
            const auto identifier = stream.readString();

            juce::OwnedArray<juce::PluginDescription> results;
            for (auto* format : formatManager.getFormats())
                if (format->getName() == formatName)
                    format->findAllTypesForFile (results, identifier);
            return results;
        }

        void sendResults (const juce::OwnedArray<juce::PluginDescription>& results)
        {
            juce::XmlElement xml ("LIST");
            for (const auto* d : results)
                xml.addChildElement (d->createXml().release());
            const auto str = xml.toString();
            sendMessageToCoordinator ({ str.toRawUTF8(), str.getNumBytesAsUTF8() });
        }

        std::mutex mutex;
        std::queue<juce::MemoryBlock> pending;
        juce::AudioPluginFormatManager formatManager;
    };

    std::unique_ptr<Worker>& workerInstance()
    {
        static std::unique_ptr<Worker> w;
        return w;
    }

    //======================================================================
    // Lato processo principale: lancia il figlio e aspetta le risposte.
    class Coordinator final : private juce::ChildProcessCoordinator
    {
    public:
        Coordinator()
        {
            launched = launchWorkerProcess (juce::File::getSpecialLocation (juce::File::currentExecutableFile),
                                            ScannerProcess::processUID, workerTimeoutMs, 0);
        }

        ~Coordinator() override { killWorkerProcess(); }

        bool isLaunched() const { return launched; }

        enum class State { timeout, gotResult, connectionLost };

        State waitForResult (std::unique_ptr<juce::XmlElement>& xmlOut)
        {
            std::unique_lock<std::mutex> lock (mutex);
            if (! condvar.wait_for (lock, std::chrono::milliseconds (50), [this] { return gotResult || connectionLost; }))
                return State::timeout;

            const auto state = connectionLost ? State::connectionLost : State::gotResult;
            gotResult = connectionLost = false;
            xmlOut = std::move (result);
            return state;
        }

        using juce::ChildProcessCoordinator::sendMessageToWorker;

    private:
        void handleMessageFromWorker (const juce::MemoryBlock& mb) override
        {
            const std::lock_guard<std::mutex> guard (mutex);
            result = juce::parseXML (mb.toString());
            gotResult = true;
            condvar.notify_one();
        }

        void handleConnectionLost() override
        {
            const std::lock_guard<std::mutex> guard (mutex);
            connectionLost = true;
            condvar.notify_one();
        }

        bool launched = false;
        std::mutex mutex;
        std::condition_variable condvar;
        std::unique_ptr<juce::XmlElement> result;
        bool gotResult = false, connectionLost = false;
    };

    class OutOfProcessScanner final : public juce::KnownPluginList::CustomScanner
    {
    public:
        bool findPluginTypesFor (juce::AudioPluginFormat& format,
                                 juce::OwnedArray<juce::PluginDescription>& result,
                                 const juce::String& fileOrIdentifier) override
        {
            if (coordinator == nullptr)
                coordinator = std::make_unique<Coordinator>();

            if (! coordinator->isLaunched())
            {
                coordinator = nullptr;
                return false;
            }

            juce::MemoryBlock block;
            {
                juce::MemoryOutputStream stream (block, true);
                stream.writeString (format.getName());
                stream.writeString (fileOrIdentifier);
            }

            if (! coordinator->sendMessageToWorker (block))
            {
                coordinator = nullptr;
                return false;
            }

            const auto deadline = juce::Time::getMillisecondCounter() + perFileDeadlineMs;

            for (;;)
            {
                if (shouldExit())
                    return true;

                if (juce::Time::getMillisecondCounter() > deadline)
                {
                    coordinator = nullptr;   // plugin bloccato: chiude il figlio, il file viene scartato
                    return false;
                }

                std::unique_ptr<juce::XmlElement> xml;
                const auto state = coordinator->waitForResult (xml);

                if (state == Coordinator::State::timeout)
                    continue;

                if (state == Coordinator::State::connectionLost)
                {
                    coordinator = nullptr;   // il figlio e' morto su questo plugin: si riparte col prossimo
                    return false;
                }

                if (xml != nullptr)
                    for (const auto* item : xml->getChildIterator())
                    {
                        auto d = std::make_unique<juce::PluginDescription>();
                        if (d->loadFromXml (*item))
                            result.add (std::move (d));
                    }
                return true;
            }
        }

        void scanFinished() override { coordinator = nullptr; }

    private:
        std::unique_ptr<Coordinator> coordinator;
    };
}

namespace ScannerProcess
{
    bool startWorkerIfRequested (const juce::String& commandLine)
    {
        auto w = std::make_unique<Worker>();
        if (! w->initialiseFromCommandLine (commandLine, processUID, workerTimeoutMs))
            return false;
        workerInstance() = std::move (w);
        return true;
    }

    void shutdownWorker() { workerInstance() = nullptr; }

    std::unique_ptr<juce::KnownPluginList::CustomScanner> makeOutOfProcessScanner()
    {
        return std::make_unique<OutOfProcessScanner>();
    }
}
