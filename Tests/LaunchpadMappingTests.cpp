#include "control/LaunchpadMapping.h"

class LaunchpadMappingTests final : public juce::UnitTest
{
public:
    LaunchpadMappingTests() : juce::UnitTest ("LaunchpadMapping", "Stage") {}

    static juce::MidiMessage press (int note)   { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 127); }
    static juce::MidiMessage release (int note) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) 0); }

    void expectAction (const juce::MidiMessage& m, SurfaceActionType type, int index)
    {
        const auto a = LaunchpadMapping::decode (m);
        expect (a.type == type, m.getDescription());
        expectEquals (a.index, index);
    }

    static int velocityAt (const std::vector<juce::MidiMessage>& msgs, bool isCc, int number)
    {
        int v = -1;
        for (const auto& m : msgs)
        {
            if (isCc && m.isController() && m.getControllerNumber() == number)     v = m.getControllerValue();
            if (! isCc && m.isNoteOn (true) && m.getNoteNumber() == number)          v = m.getVelocity();
        }
        return v;
    }

    void runTest() override
    {
        using T = SurfaceActionType;

        beginTest ("tasti della griglia");
        expectAction (press (0x00), T::patch, 0);
        expectAction (press (0x07), T::patch, 7);
        expectAction (press (0x10), T::patch, 8);
        expectAction (press (0x77), T::patch, 63);
        expectAction (release (0x00), T::none, 0);

        beginTest ("tasti tondi a destra");
        expectAction (press (8), T::mute, 0);
        expectAction (press (24), T::mute, 1);
        expectAction (press (40), T::mute, 2);
        expectAction (press (56), T::mute, 3);
        expectAction (press (72), T::previous, 0);
        expectAction (press (88), T::next, 0);
        expectAction (press (104), T::tap, 0);
        expectAction (press (120), T::panicDown, 0);
        expectAction (release (120), T::panicUp, 0);
        expectAction (juce::MidiMessage::noteOff (1, 120), T::panicUp, 0);

        beginTest ("tasti tondi in alto: banchi");
        expectAction (juce::MidiMessage::controllerEvent (1, 104, 127), T::bank, 0);
        expectAction (juce::MidiMessage::controllerEvent (1, 111, 127), T::bank, 7);
        expectAction (juce::MidiMessage::controllerEvent (1, 104, 0), T::none, 0);

        beginTest ("colori");
        expectEquals (LaunchpadMapping::velocityFor (LaunchpadMapping::Led::off), 12);
        expectEquals (LaunchpadMapping::velocityFor (LaunchpadMapping::Led::red), 15);
        expectEquals (LaunchpadMapping::velocityFor (LaunchpadMapping::Led::green), 60);
        expectEquals (LaunchpadMapping::velocityFor (LaunchpadMapping::Led::amber), 63);
        expectEquals (LaunchpadMapping::velocityFor (LaunchpadMapping::Led::redFlash), 11);

        beginTest ("LED dallo stato");
        {
            SurfaceState st;
            st.bank = 2;
            st.activePatch = 9;
            st.loadingPatch = 10;
            st.occupied[3] = st.occupied[9] = st.occupied[10] = true;
            st.bankOccupied[2] = st.bankOccupied[5] = true;
            st.muted[1] = true;
            const auto msgs = LaunchpadMapping::ledMessages (st, nullptr);
            expectEquals (velocityAt (msgs, false, 0x03), 63);        // salvata
            expectEquals (velocityAt (msgs, false, 0x11), 60);        // attiva (9 = riga 1, col 1)
            expectEquals (velocityAt (msgs, false, 0x12), 11);        // in caricamento
            expectEquals (velocityAt (msgs, false, 0x00), 12);        // vuota
            expectEquals (velocityAt (msgs, true, 106), 60);          // banco attivo C
            expectEquals (velocityAt (msgs, true, 109), 63);          // banco F con patch
            expectEquals (velocityAt (msgs, true, 104), 12);
            expectEquals (velocityAt (msgs, false, 24), 15);          // mute postazione 2
            expectEquals (velocityAt (msgs, false, 8), 12);
            expectEquals ((int) msgs.size(), 64 + 8 + 8);

            auto next = st;
            next.activePatch = 3;
            const auto diff = LaunchpadMapping::ledMessages (next, &st);
            expectEquals ((int) diff.size(), 2);                      // 9 torna ambra, 3 diventa verde
            expectEquals (velocityAt (diff, false, 0x03), 60);
            expectEquals (velocityAt (diff, false, 0x11), 63);
        }

        beginTest ("inizializzazione");
        {
            const auto init = LaunchpadMapping::initMessages();
            expect (! init.empty());
            expect (init[0].isController() && init[0].getControllerNumber() == 0 && init[0].getControllerValue() == 0);
            expectEquals (velocityAt (init, true, 0), 40);   // l'ultimo CC0 attiva il lampeggio automatico
        }
    }
};

static LaunchpadMappingTests launchpadMappingTests;

#include "control/LaunchpadController.h"

class LaunchpadControllerTests final : public juce::UnitTest
{
public:
    LaunchpadControllerTests() : juce::UnitTest ("LaunchpadController", "Stage") {}

    void runTest() override
    {
        beginTest ("PANIC solo dopo 500 ms di pressione");
        {
            LaunchpadController c (false);   // senza aprire dispositivi
            int panics = 0, patches = 0;
            c.onAction = [&] (SurfaceAction a)
            {
                if (a.type == SurfaceActionType::panic) ++panics;
                if (a.type == SurfaceActionType::patch) ++patches;
            };
            c.handleAction ({ SurfaceActionType::panicDown, 0 }, 1000);
            c.tick (1400);
            expectEquals (panics, 0);
            c.tick (1510);
            expectEquals (panics, 1);
            c.tick (1700);
            expectEquals (panics, 1);
            c.handleAction ({ SurfaceActionType::panicUp, 0 }, 1800);

            c.handleAction ({ SurfaceActionType::panicDown, 0 }, 2000);
            c.handleAction ({ SurfaceActionType::panicUp, 0 }, 2200);
            c.tick (2600);
            expectEquals (panics, 1);

            c.handleAction ({ SurfaceActionType::patch, 5 }, 3000);
            expectEquals (patches, 1);
        }
    }
};

static LaunchpadControllerTests launchpadControllerTests;
