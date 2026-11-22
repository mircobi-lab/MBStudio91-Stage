#include "control/Push2Mapping.h"

class Push2MappingTests final : public juce::UnitTest
{
public:
    Push2MappingTests() : juce::UnitTest ("Push2Mapping", "Stage") {}

    static juce::MidiMessage pad (int note, int vel = 120) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) vel); }
    static juce::MidiMessage cc (int number, int value)    { return juce::MidiMessage::controllerEvent (1, number, value); }

    void expectAction (const juce::MidiMessage& m, SurfaceActionType type, int index)
    {
        const auto a = Push2Mapping::decode (m);
        expect (a.type == type, m.getDescription());
        expectEquals (a.index, index);
    }

    static int velocityAt (const std::vector<juce::MidiMessage>& msgs, bool isCc, int number)
    {
        int v = -1;
        for (const auto& m : msgs)
        {
            if (isCc && m.isController() && m.getControllerNumber() == number)   v = m.getControllerValue();
            if (! isCc && m.isNoteOn (true) && m.getNoteNumber() == number)      v = m.getVelocity();
        }
        return v;
    }

    void runTest() override
    {
        using T = SurfaceActionType;

        // Il manuale Ableton: pad 36 in basso a sinistra, 43 in basso a destra, 99 in alto a destra.
        // La patch 0 e' in alto a sinistra, come sullo schermo e sulla Launchpad.
        beginTest ("griglia dei pad");
        expectAction (pad (92), T::patch, 0);    // alto a sinistra
        expectAction (pad (99), T::patch, 7);    // alto a destra
        expectAction (pad (36), T::patch, 56);   // basso a sinistra
        expectAction (pad (43), T::patch, 63);   // basso a destra
        expectAction (pad (84), T::patch, 8);    // seconda riga dall'alto, prima colonna
        expectAction (juce::MidiMessage::noteOn (1, 92, (juce::uint8) 0), T::none, 0);
        expectAction (juce::MidiMessage::noteOff (1, 92), T::none, 0);
        expectAction (pad (35), T::none, 0);
        expectAction (pad (100), T::none, 0);

        beginTest ("pulsanti sopra il display: banchi");
        expectAction (cc (102, 127), T::bank, 0);
        expectAction (cc (109, 127), T::bank, 7);
        expectAction (cc (102, 0), T::none, 0);

        beginTest ("pulsanti sotto il display: mute e navigazione");
        expectAction (cc (20, 127), T::mute, 0);
        expectAction (cc (23, 127), T::mute, 3);
        expectAction (cc (24, 127), T::previous, 0);
        expectAction (cc (25, 127), T::next, 0);

        beginTest ("tap tempo e PANIC");
        expectAction (cc (3, 127), T::tap, 0);
        expectAction (cc (29, 127), T::panicDown, 0);
        expectAction (cc (29, 0), T::panicUp, 0);

        beginTest ("encoder: valore relativo in complemento a due");
        expectEquals (Push2Mapping::encoderDelta (cc (71, 1)), 1);
        expectEquals (Push2Mapping::encoderDelta (cc (71, 63)), 63);
        expectEquals (Push2Mapping::encoderDelta (cc (71, 127)), -1);
        expectEquals (Push2Mapping::encoderDelta (cc (71, 64)), -64);
        expectEquals (Push2Mapping::encoderDelta (cc (71, 0)), 0);
        expectEquals (Push2Mapping::encoderDelta (cc (50, 5)), 0);
        expectEquals (Push2Mapping::encoderIndex (cc (71, 1)), 0);
        expectEquals (Push2Mapping::encoderIndex (cc (78, 1)), 7);
        expectEquals (Push2Mapping::encoderIndex (cc (79, 1)), 8);
        expectEquals (Push2Mapping::encoderIndex (cc (14, 1)), 9);
        expectEquals (Push2Mapping::encoderIndex (cc (50, 1)), -1);

        beginTest ("encoder come azioni");
        {
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

            // encoder 5-8: non assegnati in Fase 4
            expect (Push2Mapping::decode (cc (75, 1)).type == T::none);
        }

        beginTest ("tavolozza: solo indici documentati da Ableton");
        using C = Push2Mapping::Colour;
        expectEquals (Push2Mapping::paletteIndex (C::off), 0);
        expectEquals (Push2Mapping::paletteIndex (C::white), 122);
        expectEquals (Push2Mapping::paletteIndex (C::lightGrey), 123);
        expectEquals (Push2Mapping::paletteIndex (C::darkGrey), 124);
        expectEquals (Push2Mapping::paletteIndex (C::blue), 125);
        expectEquals (Push2Mapping::paletteIndex (C::green), 126);
        expectEquals (Push2Mapping::paletteIndex (C::red), 127);

        beginTest ("LED dallo stato");
        {
            SurfaceState st;
            st.bank = 2;
            st.activePatch = 0;        // alto a sinistra -> nota 92
            st.loadingPatch = 7;       // alto a destra   -> nota 99
            st.occupied[0] = st.occupied[7] = st.occupied[56] = true;
            st.bankOccupied[2] = st.bankOccupied[5] = true;
            st.muted[1] = true;

            const auto msgs = Push2Mapping::ledMessages (st, nullptr);
            expectEquals (velocityAt (msgs, false, 92), 126);   // attiva: verde
            expectEquals (velocityAt (msgs, false, 99), 127);   // in caricamento: rosso
            expectEquals (velocityAt (msgs, false, 36), 125);   // salvata: blu
            expectEquals (velocityAt (msgs, false, 37), 0);     // vuota
            expectEquals (velocityAt (msgs, true, 104), 126);   // banco attivo C: verde
            expectEquals (velocityAt (msgs, true, 107), 125);   // banco F con patch: blu
            expectEquals (velocityAt (msgs, true, 102), 0);
            expectEquals (velocityAt (msgs, true, 21), 127);    // mute postazione 2: rosso
            expectEquals (velocityAt (msgs, true, 20), 124);    // non in mute: grigio scuro

            auto next = st;
            next.activePatch = 56;
            const auto diff = Push2Mapping::ledMessages (next, &st);
            expectEquals ((int) diff.size(), 2);
            expectEquals (velocityAt (diff, false, 36), 126);
            expectEquals (velocityAt (diff, false, 92), 125);
        }

        beginTest ("avvio e chiusura: User mode e Live mode");
        {
            const auto init = Push2Mapping::initMessages();
            expect (! init.empty());
            expect (init[0].isSysEx());
            const juce::uint8 wanted[] = { 0x00, 0x21, 0x1d, 0x01, 0x01, 0x0a, 0x01 };
            expectEquals (init[0].getSysExDataSize(), 7);
            expect (std::memcmp (init[0].getSysExData(), wanted, sizeof (wanted)) == 0);

            const auto bye = Push2Mapping::shutdownMessages();
            expect (! bye.empty());
            const auto& last = bye.back();
            expect (last.isSysEx());
            const juce::uint8 live[] = { 0x00, 0x21, 0x1d, 0x01, 0x01, 0x0a, 0x00 };
            expect (std::memcmp (last.getSysExData(), live, sizeof (live)) == 0);
            // prima del cambio modo, i LED vanno spenti
            expectEquals (velocityAt (bye, false, 92), 0);
        }
    }
};

static Push2MappingTests push2MappingTests;
