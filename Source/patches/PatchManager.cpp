#include "PatchManager.h"
#include "plugins/PluginCatalog.h"

namespace
{
    PluginSlot captureSlot (juce::AudioPluginInstance* p)
    {
        PluginSlot s;
        if (p == nullptr)
            return s;
        s.portableId = makePortableId (p->getPluginDescription());
        s.name = p->getName();
        p->getStateInformation (s.state);
        return s;
    }
}

// Un richiamo in corso: le catene si riempiono man mano che i plugin arrivano.
struct PatchManager::PendingRecall
{
    int generation = 0, bank = 0, index = 0, pending = 0;
    Patch patch;
    std::array<Station::Chain, StageEngine::numStations> chains;
    std::array<juce::StringArray, StageEngine::numStations> stationErrors;
};

PatchManager::PatchManager (StageEngine& e, Library& l, InstanceFactory f)
    : engine (e), library (l), factory (std::move (f))
{
    startTimerHz (10);
}

PatchManager::~PatchManager()
{
    stopTimer();
    listenToActive (false);
    for (auto& p : pool)
        p->releaseResources();
}

//==============================================================================
Patch PatchManager::capture (const juce::String& name) const
{
    Patch p;
    p.name = name;
    for (int i = 0; i < StageEngine::numStations; ++i)
    {
        auto& st = engine.station (i);
        auto& sp = p.stations[(size_t) i];
        const auto& failed = failedSlots[(size_t) i];
        sp.instrument = st.getInstrument() != nullptr || ! failed[0].has_value() ? captureSlot (st.getInstrument()) : *failed[0];
        for (int fx = 0; fx < Station::maxEffects; ++fx)
            sp.effects[(size_t) fx] = st.getEffect (fx) != nullptr || ! failed[(size_t) fx + 1].has_value()
                                        ? captureSlot (st.getEffect (fx)) : *failed[(size_t) fx + 1];
        sp.gainDb = st.getGainDb();
        sp.muted = st.isMuted();
        sp.zone = engine.getZone (i);
    }
    return p;
}

void PatchManager::saveCurrentTo (int b, int index, const juce::String& name)
{
    library.set (b, index, capture (name));
    bank = b;
    patchBank = b;
    patch = index;
    modified = false;
    if (onStateChanged)
        onStateChanged();
}

//==============================================================================
void PatchManager::recall (int b, int index)
{
    const auto& stored = library.get (b, index);
    if (! stored.has_value())
        return;

    auto req = std::make_shared<PendingRecall>();
    req->generation = ++generation;
    req->bank = b;
    req->index = index;
    req->patch = *stored;
    loading = index;
    bank = b;
    if (onStateChanged)
        onStateChanged();

    // Prima si contano tutti i plugin da caricare, poi si chiedono: la fabbrica puo' rispondere subito.
    struct Request { int station, slot; juce::String id, name; };
    std::vector<Request> requests;
    for (int s = 0; s < StageEngine::numStations; ++s)
    {
        const auto& sp = req->patch.stations[(size_t) s];
        if (sp.instrument.portableId.isNotEmpty())
            requests.push_back ({ s, 0, sp.instrument.portableId, sp.instrument.name });
        for (int fx = 0; fx < Station::maxEffects; ++fx)
            if (sp.effects[(size_t) fx].portableId.isNotEmpty())
                requests.push_back ({ s, fx + 1, sp.effects[(size_t) fx].portableId, sp.effects[(size_t) fx].name });
    }
    req->pending = (int) requests.size();

    if (req->pending == 0)
    {
        apply (*req);
        return;
    }

    for (const auto& r : requests)
    {
        auto deliver = [this, req, r, token = std::weak_ptr<bool> (alive)]
                       (std::unique_ptr<juce::AudioPluginInstance> inst, juce::String error)
        {
            if (token.expired())
                return;   // app in chiusura: l'istanza viene distrutta qui

            if (req->generation != generation)
            {
                addToPool (std::move (inst));   // richiamo superato: l'istanza resta pronta
                return;
            }

            if (inst == nullptr)
                req->stationErrors[(size_t) r.station].add (error.isNotEmpty() ? error : "Impossibile caricare " + r.name);
            else if (r.slot == 0)
                req->chains[(size_t) r.station].instrument = std::move (inst);
            else
                req->chains[(size_t) r.station].effects[(size_t) r.slot - 1] = std::move (inst);

            if (--req->pending == 0)
                apply (*req);
        };

        if (auto pooled = takeFromPool (r.id))
            deliver (std::move (pooled), {});
        else
            factory (r.id, engine.getSampleRate(), engine.getBlockSize(), deliver);
    }
}

void PatchManager::apply (PendingRecall& req)
{
    applying = true;
    listenToActive (false);
    errors.clear();

    for (int s = 0; s < StageEngine::numStations; ++s)
    {
        const auto& sp = req.patch.stations[(size_t) s];
        auto& chain = req.chains[(size_t) s];

        auto& failed = failedSlots[(size_t) s];
        failed[0] = (sp.instrument.portableId.isNotEmpty() && chain.instrument == nullptr) ? std::make_optional (sp.instrument) : std::nullopt;
        for (int fx = 0; fx < Station::maxEffects; ++fx)
            failed[(size_t) fx + 1] = (sp.effects[(size_t) fx].portableId.isNotEmpty() && chain.effects[(size_t) fx] == nullptr)
                                          ? std::make_optional (sp.effects[(size_t) fx]) : std::nullopt;

        auto restore = [] (juce::AudioPluginInstance* p, const PluginSlot& slot)
        {
            if (p != nullptr && slot.state.getSize() > 0)
                p->setStateInformation (slot.state.getData(), (int) slot.state.getSize());
        };
        restore (chain.instrument.get(), sp.instrument);
        for (int fx = 0; fx < Station::maxEffects; ++fx)
            restore (chain.effects[(size_t) fx].get(), sp.effects[(size_t) fx]);

        auto& station = engine.station (s);
        station.setChain (std::move (chain));
        station.setGainDb (sp.gainDb);
        station.setMuted (sp.muted);
        engine.setZone (s, sp.zone);

        const auto stationError = req.stationErrors[(size_t) s].joinIntoString ("; ");
        station.setLoadError (stationError);
        errors.addArray (req.stationErrors[(size_t) s]);
    }

    bank = req.bank;
    patchBank = req.bank;
    patch = req.index;
    loading = -1;
    listenToActive (true);
    modified = false;
    applying = false;

    if (onStateChanged)
        onStateChanged();
}

//==============================================================================
std::unique_ptr<juce::AudioPluginInstance> PatchManager::takeFromPool (const juce::String& id)
{
    for (auto it = pool.end(); it != pool.begin();)
    {
        --it;   // la piu' recente per prima
        if (makePortableId ((*it)->getPluginDescription()) == id)
        {
            auto p = std::move (*it);
            pool.erase (it);
            return p;
        }
    }
    return nullptr;
}

void PatchManager::addToPool (std::unique_ptr<juce::AudioPluginInstance> p)
{
    if (p == nullptr)
        return;
    pool.push_back (std::move (p));
    while ((int) pool.size() > maxPooled)
    {
        pool.front()->releaseResources();
        pool.erase (pool.begin());   // la piu' vecchia viene distrutta qui, sul message thread
    }
}

int PatchManager::pooledCount() const { return (int) pool.size(); }

void PatchManager::collectFinished()
{
    for (int s = 0; s < StageEngine::numStations; ++s)
        for (auto& inst : engine.station (s).takeFinishedInstances())
            addToPool (std::move (inst));
}

void PatchManager::timerCallback()
{
    collectFinished();
}

//==============================================================================
void PatchManager::listenToActive (bool listen)
{
    for (int s = 0; s < StageEngine::numStations; ++s)
    {
        auto& st = engine.station (s);
        auto apply = [this, listen] (juce::AudioPluginInstance* p)
        {
            if (p == nullptr) return;
            if (listen) p->addListener (this);
            else        p->removeListener (this);
        };
        apply (st.getInstrument());
        for (int fx = 0; fx < Station::maxEffects; ++fx)
            apply (st.getEffect (fx));
    }
}

void PatchManager::audioProcessorParameterChanged (juce::AudioProcessor*, int, float)
{
    if (! applying)
        modified = true;   // puo' arrivare da qualsiasi thread
}

void PatchManager::audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d)
{
    if (! applying && d.programChanged)
        modified = true;
}

int PatchManager::activeBank() const     { return bank; }
int PatchManager::activePatch() const    { return patch; }
int PatchManager::loadingPatch() const   { return loading; }
bool PatchManager::isLoading() const     { return loading >= 0; }
bool PatchManager::isModified() const    { return modified; }
void PatchManager::setActiveBank (int b) { bank = juce::jlimit (0, Library::numBanks - 1, b); if (onStateChanged) onStateChanged(); }
void PatchManager::markModified()        { modified = true; }
juce::StringArray PatchManager::lastErrors() const { return errors; }
int PatchManager::activePatchBank() const { return patchBank; }
