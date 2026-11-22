#include "control/Push2Mapping.h"
#include "control/SurfaceCoordinator.h"
#include "FakeInstrument.h"

// Gli otto rotativi sopra il display, quello del master e quello del tempo.
class Push2EncoderTests final : public juce::UnitTest
{
public:
    Push2EncoderTests() : juce::UnitTest ("Push2Encoder", "Stage") {}

    static juce::MidiMessage cc (int number, int value) { return juce::MidiMessage::controllerEvent (1, number, value); }

    void runTest() override
    {
        using T = SurfaceActionType;

        beginTest ("i rotativi diventano azioni, con il verso giusto");
        {
            // Il Push manda un valore relativo in complemento a due: 1-63 a destra, 127-64 a sinistra.
            auto a = Push2Mapping::decode (cc (71, 2));
            expect (a.type == T::stationVolume);
            expectEquals (a.index, 0);
            expectEquals (a.delta, 2);

            a = Push2Mapping::decode (cc (74, 127));
            expect (a.type == T::stationVolume);
            expectEquals (a.index, 3);
            expectEquals (a.delta, -1);

            a = Push2Mapping::decode (cc (79, 3));
            expect (a.type == T::masterVolume);
            expectEquals (a.delta, 3);

            a = Push2Mapping::decode (cc (14, 126));
            expect (a.type == T::tempoDelta);
            expectEquals (a.delta, -2);

            // rotativi 5-8: liberi per adesso
            expect (Push2Mapping::decode (cc (75, 1)).type == T::none);
            expect (Push2Mapping::decode (cc (78, 1)).type == T::none);
            // nessun movimento
            expect (Push2Mapping::decode (cc (71, 0)).type == T::none);
        }

        StageEngine e;
        e.prepare (48000.0, 256);
        Library lib;
        PatchManager pm (e, lib, [] (const juce::String&, double, int, PatchManager::LoadCallback cb)
                                 { cb (std::make_unique<FakeInstrument>(), {}); });
        SurfaceCoordinator c (e, lib, pm, false);

        beginTest ("il rotativo muove il volume della postazione, mezzo decibel a scatto");
        {
            e.station (1).setGainDb (0.0f);
            c.handleAction ({ T::stationVolume, 1, 4 });
            expectWithinAbsoluteError (e.station (1).getGainDb(), 2.0f, 0.01f);
            c.handleAction ({ T::stationVolume, 1, -10 });
            expectWithinAbsoluteError (e.station (1).getGainDb(), -3.0f, 0.01f);
            expect (pm.isModified());
        }

        beginTest ("il volume della postazione si ferma ai limiti");
        {
            e.station (2).setGainDb (0.0f);
            c.handleAction ({ T::stationVolume, 2, 100 });
            expectWithinAbsoluteError (e.station (2).getGainDb(), 6.0f, 0.01f);
            c.handleAction ({ T::stationVolume, 2, -500 });
            expectWithinAbsoluteError (e.station (2).getGainDb(), -60.0f, 0.01f);
            // una postazione che non esiste non deve far danni
            c.handleAction ({ T::stationVolume, 9, 4 });
        }

        beginTest ("il rotativo del master muove il volume generale");
        {
            e.setMasterGainDb (0.0f);
            c.handleAction ({ T::masterVolume, 0, 6 });
            expectWithinAbsoluteError (e.getMasterGainDb(), 3.0f, 0.01f);
            c.handleAction ({ T::masterVolume, 0, 200 });
            expectWithinAbsoluteError (e.getMasterGainDb(), 6.0f, 0.01f);
        }

        beginTest ("il rotativo del tempo muove il BPM solo in tempo libero");
        {
            e.tempo().setSource (TempoTracker::Source::internal);
            e.tempo().setBpm (120.0);
            c.handleAction ({ T::tempoDelta, 0, 5 });
            expectWithinAbsoluteError (e.tempo().getBpm(), 125.0, 0.01);
            c.handleAction ({ T::tempoDelta, 0, -10 });
            expectWithinAbsoluteError (e.tempo().getBpm(), 115.0, 0.01);

            // con il clock esterno il BPM lo decide la drum machine, non il rotativo
            e.tempo().setSource (TempoTracker::Source::external);
            c.handleAction ({ T::tempoDelta, 0, 20 });
            e.tempo().setSource (TempoTracker::Source::internal);
            expectWithinAbsoluteError (e.tempo().getBpm(), 115.0, 0.01);
        }
    }
};

static Push2EncoderTests push2EncoderTests;
