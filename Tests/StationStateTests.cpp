#include "engine/StationState.h"

class StationStateTests final : public juce::UnitTest
{
public:
    StationStateTests() : juce::UnitTest ("StationState", "Stage") {}

    void runTest() override
    {
        beginTest ("giro completo di una postazione");
        {
            StationState s;
            s.gainDb = -4.5f;
            s.muted = true;
            s.instrumentId = "VST3|LennarDigital|Sylenth1|4c4e5331";
            s.effectIds[0] = "VST3|MBStudio91|LA-91|11223344";
            s.effectIds[2] = "AudioUnit|Apple|AUDelay|64656c79";
            s.zone.transpose = -12;
            s.zone.lowNote = 36;
            s.zone.highNote = 84;

            const auto back = StationState::fromValueTree (s.toValueTree());
            expectEquals (back.gainDb, s.gainDb);
            expect (back.muted);
            expectEquals (back.instrumentId, s.instrumentId);
            expectEquals (back.effectIds[0], s.effectIds[0]);
            expectEquals (back.effectIds[1], juce::String());
            expectEquals (back.effectIds[2], s.effectIds[2]);
            expect (back.zone == s.zone);
            expect (back.hasPlugins());
            expectEquals (back.allPluginIds().size(), 3);
        }

        beginTest ("postazione vuota");
        {
            const StationState s;
            const auto back = StationState::fromValueTree (s.toValueTree());
            expect (! back.hasPlugins());
            expect (back.allPluginIds().isEmpty());
            expect (! back.muted);
        }

        beginTest ("le quattro postazioni restano al loro posto");
        {
            StationStates states;
            for (int i = 0; i < StationState::numStations; ++i)
            {
                states[(size_t) i].instrumentId = "VST3|MBStudio91|Strumento" + juce::String (i) + "|0";
                states[(size_t) i].zone.transpose = i;
            }
            const auto back = stationsFromValueTree (stationsToValueTree (states));
            for (int i = 0; i < StationState::numStations; ++i)
            {
                expectEquals (back[(size_t) i].instrumentId, states[(size_t) i].instrumentId);
                expectEquals (back[(size_t) i].zone.transpose, i);
            }
        }

        beginTest ("legge il vecchio file con le sole zone");
        {
            juce::ValueTree old ("POSTAZIONI");
            for (int i = 0; i < 4; ++i)
            {
                KeyZone z;
                z.transpose = 12 - i;
                old.appendChild (z.toValueTree(), nullptr);
            }
            const auto back = stationsFromValueTree (old);
            for (int i = 0; i < 4; ++i)
            {
                expectEquals (back[(size_t) i].zone.transpose, 12 - i);
                expect (back[(size_t) i].instrumentId.isEmpty());
            }
        }

        beginTest ("un file vuoto non fa danni");
        {
            const auto back = stationsFromValueTree ({});
            for (const auto& s : back)
                expect (! s.hasPlugins());
        }
    }
};

static StationStateTests stationStateTests;
