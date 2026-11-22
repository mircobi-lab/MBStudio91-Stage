#include "control/SurfaceCoordinator.h"
#include "FakeInstrument.h"

class SurfaceCoordinatorTests final : public juce::UnitTest
{
public:
    SurfaceCoordinatorTests() : juce::UnitTest ("SurfaceCoordinator", "Stage") {}

    void runTest() override
    {
        StageEngine e; e.prepare (48000.0, 256);
        Library lib;
        PatchManager pm (e, lib, [] (const juce::String&, double, int, PatchManager::LoadCallback cb)
                                 { cb (std::make_unique<FakeInstrument>(), {}); });
        Patch p; p.name = "Uno"; p.stations[0].instrument.portableId = "Fake||FakeInstrument|0";
        lib.set (0, 3, p);
        Patch q = p; q.name = "Due";
        lib.set (1, 0, q);

        SurfaceCoordinator c (e, lib, pm, false);

        beginTest ("stato per le superfici");
        {
            auto st = c.buildState();
            expectEquals (st.bank, 0);
            expect (st.occupied[3] && ! st.occupied[4]);
            expect (st.bankOccupied[0] && st.bankOccupied[1] && ! st.bankOccupied[2]);
            expectEquals (st.activePatch, -1);
        }

        beginTest ("pad: richiama la patch; pad vuoto: niente");
        {
            c.handleAction ({ SurfaceActionType::patch, 3 });
            auto st = c.buildState();
            expectEquals (st.activePatch, 3);
            expectEquals (st.patchName, juce::String ("Uno"));
            c.handleAction ({ SurfaceActionType::patch, 10 });
            expectEquals (c.buildState().activePatch, 3);
        }

        beginTest ("banco: cambia la griglia mostrata, la patch attiva resta");
        {
            c.handleAction ({ SurfaceActionType::bank, 1 });
            auto st = c.buildState();
            expectEquals (st.bank, 1);
            expectEquals (st.activePatch, -1);    // la patch attiva e' nel banco A
            expect (st.occupied[0]);
            expectEquals (pm.activePatch(), 3);
        }

        beginTest ("successiva e precedente attraverso i banchi");
        {
            c.handleAction ({ SurfaceActionType::next, 0 });
            expectEquals (pm.activePatchBank(), 1);
            expectEquals (pm.activePatch(), 0);
            c.handleAction ({ SurfaceActionType::previous, 0 });
            expectEquals (pm.activePatchBank(), 0);
            expectEquals (pm.activePatch(), 3);
        }

        beginTest ("mute di una postazione");
        {
            c.handleAction ({ SurfaceActionType::mute, 2 });
            expect (e.station (2).isMuted());
            expect (c.buildState().muted[2]);
            expect (pm.isModified());
            c.handleAction ({ SurfaceActionType::mute, 2 });
            expect (! e.station (2).isMuted());
        }

        beginTest ("tap solo in tempo libero");
        {
            c.handleTap (10.0);
            c.handleTap (10.6);
            expectWithinAbsoluteError (e.tempo().getBpm(), 100.0, 0.01);
            e.tempo().setSource (TempoTracker::Source::external);
            c.handleTap (11.0);
            c.handleTap (11.3);
            e.tempo().setSource (TempoTracker::Source::internal);
            expectWithinAbsoluteError (e.tempo().getBpm(), 100.0, 0.01);
        }
    }
};

static SurfaceCoordinatorTests surfaceCoordinatorTests;
