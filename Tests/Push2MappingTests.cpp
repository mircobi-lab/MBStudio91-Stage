#include "control/Push2Mapping.h"
#include <set>
#include "push2/Push2Protocol.h"

class Push2MappingTests final : public juce::UnitTest
{
public:
    Push2MappingTests() : juce::UnitTest ("Push2Mapping", "Stage") {}

    static juce::MidiMessage cc (int number, int value) { return juce::MidiMessage::controllerEvent (1, number, value); }

    void runTest() override
    {
        using namespace Push2Mapping;

        beginTest ("i 64 pad sono le 64 patch, la prima in alto a sinistra");
        {
            expectEquals (padForPatch (0), 92);    // in alto a sinistra
            expectEquals (padForPatch (7), 99);    // in alto a destra
            expectEquals (padForPatch (56), 36);   // in basso a sinistra
            expectEquals (padForPatch (63), 43);   // in basso a destra
            for (int i = 0; i < 64; ++i)
                expectEquals (patchForPad (padForPatch (i)), i);
            expectEquals (patchForPad (35), -1);
            expectEquals (patchForPad (100), -1);
        }

        beginTest ("premere un pad richiama la patch");
        {
            const auto a = decode (juce::MidiMessage::noteOn (1, padForPatch (12), (juce::uint8) 100));
            expect (a.type == SurfaceActionType::patch);
            expectEquals (a.index, 12);
            // il rilascio non deve richiamare niente
            expect (decode (juce::MidiMessage::noteOff (1, padForPatch (12))).type == SurfaceActionType::none);
        }

        beginTest ("i tasti sopra il display sono i banchi");
        {
            for (int b = 0; b < 8; ++b)
            {
                const auto a = decode (cc (push2::upperDisplayButtonCC + b, 127));
                expect (a.type == SurfaceActionType::bank);
                expectEquals (a.index, b);
            }
            expect (decode (cc (push2::upperDisplayButtonCC, 0)).type == SurfaceActionType::none);
        }

        beginTest ("colonna a destra: mute, precedente, successiva, tap, panic");
        {
            for (int m = 0; m < 4; ++m)
            {
                const auto a = decode (cc (push2::sceneButtonCC (m), 127));
                expect (a.type == SurfaceActionType::mute);
                expectEquals (a.index, m);
            }
            expect (decode (cc (push2::sceneButtonCC (4), 127)).type == SurfaceActionType::previous);
            expect (decode (cc (push2::sceneButtonCC (5), 127)).type == SurfaceActionType::next);
            expect (decode (cc (push2::sceneButtonCC (6), 127)).type == SurfaceActionType::tap);
            expect (decode (cc (push2::tapTempoCC, 127)).type == SurfaceActionType::tap);

            // PANIC manda giu' e su, cosi' il controller puo' distinguere la pressione lunga
            expect (decode (cc (push2::sceneButtonCC (7), 127)).type == SurfaceActionType::panicDown);
            expect (decode (cc (push2::sceneButtonCC (7), 0)).type == SurfaceActionType::panicUp);
        }

        beginTest ("i LED dicono patch attiva, in caricamento, occupate e mute");
        {
            SurfaceState st;
            st.bank = 2;
            st.activePatch = 5;
            st.loadingPatch = 9;
            st.occupied[3] = true;
            st.muted[1] = true;
            st.bankOccupied[2] = true;

            const auto msgs = ledMessages (st, nullptr);
            expect (! msgs.empty());

            // Si guardano i byte veri: un pad spento e' 0x90 nota 0, che JUCE non
            // considera "note on" ma che il Push legge come colore 0.
            auto valueOf = [&msgs] (bool isCc, int number) -> int
            {
                for (const auto& m : msgs)
                {
                    const auto* d = m.getRawData();
                    if (m.getRawDataSize() < 3)
                        continue;
                    const bool msgIsCc = (d[0] & 0xF0) == 0xB0;
                    if (msgIsCc == isCc && (int) d[1] == number)
                        return (int) d[2];
                }
                return -1;
            };

            expectEquals (valueOf (false, padForPatch (5)), paletteIndexFor (Led::active));
            expectEquals (valueOf (false, padForPatch (9)), paletteIndexFor (Led::loading));
            expectEquals (valueOf (false, padForPatch (3)), paletteIndexFor (Led::occupied));
            expectEquals (valueOf (false, padForPatch (0)), push2::colourBlack);
            expectEquals (valueOf (true, push2::upperDisplayButtonCC + 2), paletteIndexFor (Led::active));
            expectEquals (valueOf (true, push2::sceneButtonCC (1)), paletteIndexFor (Led::muted));
            expectEquals (valueOf (true, push2::sceneButtonCC (0)), push2::colourBlack);
        }

        beginTest ("senza cambiamenti non si manda niente");
        {
            SurfaceState st;
            st.activePatch = 1;
            expect (ledMessages (st, &st).empty());

            SurfaceState other = st;
            other.activePatch = 2;
            expect (! ledMessages (other, &st).empty());
        }

        beginTest ("la palette si definisce e si puo' richiedere");
        {
            const auto set = paletteMessages();
            expectEquals ((int) set.size(), numPaletteEntries + 1);   // le voci piu' il riapplica
            for (const auto& m : set)
                expect (m.isSysEx());

            expectEquals ((int) readPaletteMessages().size(), numPaletteEntries);

            // indici distinti e mai il nero
            std::set<int> seen;
            for (auto l : { Led::active, Led::occupied, Led::loading, Led::muted, Led::action, Led::tap })
            {
                const int i = paletteIndexFor (l);
                expect (i != push2::colourBlack);
                expect (seen.insert (i).second, "indice ripetuto");
            }
            expectEquals (paletteIndexFor (Led::off), push2::colourBlack);
        }

        beginTest ("spegnere tutto tocca pad, banchi e colonna");
        {
            const auto off = allOffMessages();
            expectEquals ((int) off.size(), 64 + 8 + 8);
            for (const auto& m : off)
            {
                expectEquals (m.getRawDataSize(), 3);
                expectEquals ((int) m.getRawData()[2], push2::colourBlack);
            }
        }
    }
};

static Push2MappingTests push2MappingTests;
