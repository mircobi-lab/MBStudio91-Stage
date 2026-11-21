#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include "Patch.h"
#include "engine/StageEngine.h"

// Cattura e richiama le patch sulle 4 postazioni. Vive sul message thread.
class PatchManager : private juce::Timer,
                     private juce::AudioProcessorListener
{
public:
    using LoadCallback = std::function<void (std::unique_ptr<juce::AudioPluginInstance>, juce::String error)>;
    using InstanceFactory = std::function<void (const juce::String& portableId, double sampleRate, int blockSize, LoadCallback)>;

    static constexpr int maxPooled = 16;   // istanze inattive tenute pronte (4 per postazione)

    PatchManager (StageEngine&, Library&, InstanceFactory);
    ~PatchManager() override;

    Patch capture (const juce::String& name) const;
    void recall (int bank, int index);   // asincrono: l'ultimo richiamo vince
    void saveCurrentTo (int bank, int index, const juce::String& name);

    int activeBank() const;
    int activePatch() const;
    int loadingPatch() const;            // -1 se nessuno
    bool isLoading() const;
    bool isModified() const;
    void setActiveBank (int);
    void markModified();
    juce::StringArray lastErrors() const;

    void collectFinished();              // istanze delle code terminate -> pool (chiamato dal timer)
    int pooledCount() const;

    std::function<void()> onStateChanged;   // message thread

private:
    void timerCallback() override;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override;

    struct PendingRecall;
    void apply (PendingRecall&);
    std::unique_ptr<juce::AudioPluginInstance> takeFromPool (const juce::String& portableId);
    void addToPool (std::unique_ptr<juce::AudioPluginInstance>);
    void listenToActive (bool listen);

    StageEngine& engine;
    Library& library;
    InstanceFactory factory;

    std::vector<std::unique_ptr<juce::AudioPluginInstance>> pool;   // il piu' vecchio in testa
    int generation = 0;
    int bank = 0, patch = -1, loading = -1;
    std::atomic<bool> modified { false };
    std::atomic<bool> applying { false };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // per i caricamenti che finiscono dopo la chiusura
    juce::StringArray errors;

    JUCE_DECLARE_NON_COPYABLE (PatchManager)
};
