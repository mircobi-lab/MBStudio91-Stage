#include "SurfaceCoordinator.h"

namespace
{
    constexpr float dbPerClick = 0.5f;      // mezzo decibel a scatto del rotativo
    constexpr float minVolumeDb = -60.0f, maxVolumeDb = 6.0f;
}

SurfaceCoordinator::SurfaceCoordinator (StageEngine& e, Library& l, PatchManager& p, bool runTimer)
    : engine (e), library (l), patches (p)
{
    if (runTimer)
        startTimerHz (20);
}

SurfaceCoordinator::~SurfaceCoordinator()
{
    stopTimer();
    for (auto* s : surfaces)
        s->onAction = nullptr;
}

void SurfaceCoordinator::addSurface (ControlSurface& s)
{
    surfaces.push_back (&s);
    s.onAction = [this] (SurfaceAction a) { handleAction (a); };
}

SurfaceState SurfaceCoordinator::buildState() const
{
    SurfaceState st;
    st.bank = patches.activeBank();

    for (int i = 0; i < Library::patchesPerBank; ++i)
        st.occupied[(size_t) i] = library.get (st.bank, i).has_value();
    for (int b = 0; b < Library::numBanks; ++b)
        st.bankOccupied[(size_t) b] = library.bankHasPatches (b);

    if (patches.activePatchBank() == st.bank)
        st.activePatch = patches.activePatch();
    if (patches.isLoading())
        st.loadingPatch = patches.loadingPatch();

    for (int s = 0; s < StageEngine::numStations; ++s)
        st.muted[(size_t) s] = engine.station (s).isMuted();

    if (const auto& p = library.get (patches.activePatchBank(), patches.activePatch()))
        st.patchName = p->name;
    st.modified = patches.isModified();

    auto& tempo = engine.tempo();
    st.bpm = tempo.getBpm();
    const double ppq = tempo.position().getPpqPosition().orFallback (0.0);
    st.tapLit = tempo.isPlaying() && (ppq - std::floor (ppq)) < 0.15;   // lampeggia sul quarto
    return st;
}

void SurfaceCoordinator::handleAction (SurfaceAction a)
{
    switch (a.type)
    {
        case SurfaceActionType::patch:
            if (library.get (patches.activeBank(), a.index).has_value())
                patches.recall (patches.activeBank(), a.index);
            break;

        case SurfaceActionType::bank:
            patches.setActiveBank (a.index);
            break;

        case SurfaceActionType::previous:
        case SurfaceActionType::next:
            if (auto n = library.neighbour (patches.activePatchBank(), patches.activePatch(),
                                            a.type == SurfaceActionType::next ? 1 : -1))
                patches.recall (n->first, n->second);
            break;

        case SurfaceActionType::mute:
            if (juce::isPositiveAndBelow (a.index, StageEngine::numStations))
            {
                auto& st = engine.station (a.index);
                st.setMuted (! st.isMuted());
                patches.markModified();
            }
            break;

        case SurfaceActionType::tap:
            handleTap (juce::Time::getMillisecondCounterHiRes() * 0.001);
            break;

        case SurfaceActionType::panic:
            engine.panicAll();
            break;

        case SurfaceActionType::stationVolume:
            if (juce::isPositiveAndBelow (a.index, StageEngine::numStations))
            {
                auto& st = engine.station (a.index);
                st.setGainDb (juce::jlimit (minVolumeDb, maxVolumeDb, st.getGainDb() + a.delta * dbPerClick));
                patches.markModified();
            }
            break;

        case SurfaceActionType::masterVolume:
            engine.setMasterGainDb (juce::jlimit (minVolumeDb, maxVolumeDb,
                                                  engine.getMasterGainDb() + a.delta * dbPerClick));
            break;

        case SurfaceActionType::tempoDelta:
            // Col clock esterno il tempo lo detta la drum machine: il rotativo non tocca niente.
            if (engine.tempo().getSource() == TempoTracker::Source::internal)
                engine.tempo().setBpm (engine.tempo().getBpm() + a.delta);
            break;

        case SurfaceActionType::none:
        case SurfaceActionType::panicDown:
        case SurfaceActionType::panicUp:
            break;
    }
}

void SurfaceCoordinator::handleTap (double now)
{
    if (engine.tempo().getSource() == TempoTracker::Source::internal)
        engine.tempo().tap (now);
}

void SurfaceCoordinator::timerCallback()
{
    const auto st = buildState();
    for (auto* s : surfaces)
        s->showState (st);
}
