#include "patches/PatchManager.h"
#include "FakeInstrument.h"

class PatchManagerTests final : public juce::UnitTest
{
public:
    PatchManagerTests() : juce::UnitTest ("PatchManager", "Stage") {}

    static constexpr int bs = 256;

    struct Factory
    {
        int created = 0;
        bool deferred = false;
        std::vector<std::function<void()>> pending;

        PatchManager::InstanceFactory make()
        {
            return [this] (const juce::String& id, double, int, PatchManager::LoadCallback cb)
            {
                auto run = [this, id, cb]
                {
                    if (id.contains ("FakeInstrument"))     { ++created; cb (std::make_unique<FakeInstrument>(), {}); }
                    else if (id.contains ("FakeGainEffect")) { ++created; cb (std::make_unique<FakeGainEffect> (0.5f), {}); }
                    else                                    cb (nullptr, "Impossibile caricare " + id);
                };
                if (deferred) pending.push_back (run);
                else          run();
            };
        }

        void flush() { auto p = std::move (pending); pending.clear(); for (auto& f : p) f(); }
    };

    static FakeInstrument* fakeAt (StageEngine& e, int station)
    {
        return dynamic_cast<FakeInstrument*> (e.station (station).getInstrument());
    }

    static void renderSeconds (StageEngine& e, double seconds)
    {
        juce::AudioBuffer<float> out (2, bs);
        for (int i = 0; i < (int) (seconds * 48000.0 / bs) + 1; ++i)
            e.renderBlock (out);
    }

    void runTest() override
    {
        beginTest ("cattura, salva e richiama ripristina tutto");
        {
            StageEngine e; e.prepare (48000.0, bs);
            Library lib; Factory f;
            PatchManager pm (e, lib, f.make());

            auto inst = std::make_unique<FakeInstrument>();
            inst->stateText = "suono A";
            e.station (1).setInstrument (std::move (inst));
            e.station (1).setGainDb (-3.0f);
            e.station (1).setMuted (true);
            KeyZone z; z.transpose = 5; z.curve = VelocityCurve::dura;
            e.setZone (1, z);

            pm.saveCurrentTo (0, 0, "Fusion");
            expect (lib.get (0, 0).has_value() && lib.get (0, 0)->name == "Fusion");
            expect (lib.get (0, 0)->stations[1].instrument.portableId.contains ("FakeInstrument"));

            fakeAt (e, 1)->stateText = "suono B";
            e.setZone (1, {});
            e.station (1).setGainDb (0.0f);
            e.station (1).setMuted (false);

            pm.recall (0, 0);
            auto* now = fakeAt (e, 1);
            expect (now != nullptr && now->stateText == "suono A");
            expect (e.getZone (1) == z);
            expectWithinAbsoluteError (e.station (1).getGainDb(), -3.0f, 0.01f);
            expect (e.station (1).isMuted());
            expectEquals (pm.activeBank(), 0);
            expectEquals (pm.activePatch(), 0);
            expect (! pm.isLoading());
            expect (! pm.isModified());
        }

        beginTest ("plugin mancante: la postazione resta vuota, le altre si caricano");
        {
            StageEngine e; e.prepare (48000.0, bs);
            Library lib; Factory f;
            PatchManager pm (e, lib, f.make());

            Patch p; p.name = "Con fantasma";
            p.stations[0].instrument.portableId = "VST3|Nessuno|Fantasma|1";
            p.stations[0].instrument.name = "Fantasma";
            p.stations[2].instrument.portableId = "Fake||FakeInstrument|0";
            lib.set (1, 5, p);

            pm.recall (1, 5);
            expect (e.station (0).getInstrument() == nullptr);
            expect (e.station (0).getLoadError().contains ("Fantasma"));
            expect (fakeAt (e, 2) != nullptr);
            expect (! pm.lastErrors().isEmpty());
        }

        beginTest ("due richiami di fila: vince l'ultimo");
        {
            StageEngine e; e.prepare (48000.0, bs);
            Library lib; Factory f; f.deferred = true;
            PatchManager pm (e, lib, f.make());

            Patch a; a.name = "A"; a.stations[0].instrument.portableId = "Fake||FakeInstrument|0";
            a.stations[0].instrument.state.append ("primo", 5);
            Patch b = a; b.name = "B"; b.stations[0].instrument.state.reset(); b.stations[0].instrument.state.append ("secondo", 7);
            lib.set (0, 0, a);
            lib.set (0, 1, b);

            pm.recall (0, 0);
            pm.recall (0, 1);
            expect (pm.isLoading());
            expectEquals (pm.loadingPatch(), 1);
            f.flush();
            expect (! pm.isLoading());
            expectEquals (pm.activePatch(), 1);
            auto* now = fakeAt (e, 0);
            expect (now != nullptr && now->stateText == "secondo");
            expectEquals (pm.pooledCount(), 1);   // l'istanza del richiamo superato e' nel pool
        }

        beginTest ("il pool evita di ricaricare un plugin gia' usato");
        {
            StageEngine e; e.prepare (48000.0, bs);
            Library lib; Factory f;
            PatchManager pm (e, lib, f.make());

            Patch a; a.name = "A"; a.stations[0].instrument.portableId = "Fake||FakeInstrument|0";
            Patch b; b.name = "B"; b.stations[0].instrument.portableId = "Fake||FakeInstrument|0";
            lib.set (0, 0, a);
            lib.set (0, 1, b);

            pm.recall (0, 0);
            pm.recall (0, 1);
            expectEquals (f.created, 2);
            renderSeconds (e, 3.3);          // la catena di A finisce la coda
            pm.collectFinished();
            expectEquals (pm.pooledCount(), 1);
            pm.recall (0, 0);
            expectEquals (f.created, 2);     // preso dal pool
            expectEquals (pm.pooledCount(), 0);
        }

        beginTest ("patch modificata");
        {
            StageEngine e; e.prepare (48000.0, bs);
            Library lib; Factory f;
            PatchManager pm (e, lib, f.make());
            Patch a; a.name = "A"; a.stations[0].instrument.portableId = "Fake||FakeInstrument|0";
            lib.set (0, 0, a);

            pm.recall (0, 0);
            expect (! pm.isModified());
            fakeAt (e, 0)->cutoff->setValueNotifyingHost (0.9f);
            expect (pm.isModified());
            pm.saveCurrentTo (0, 0, "A");
            expect (! pm.isModified());
            pm.markModified();
            expect (pm.isModified());
            pm.recall (0, 0);
            expect (! pm.isModified());
        }
    }
};

static PatchManagerTests patchManagerTests;
