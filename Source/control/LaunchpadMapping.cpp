#include "LaunchpadMapping.h"
#include <map>

namespace LaunchpadMapping
{
    namespace
    {
        constexpr int firstTopCc = 104;   // tasti tondi in alto: CC 104..111
        constexpr int rightColumn = 8;    // tasti tondi a destra: colonna 8 del layout X-Y

        int gridNote (int index0to63) { return (index0to63 / 8) * 16 + (index0to63 % 8); }
        int rightNote (int row)       { return row * 16 + rightColumn; }

        // (true = CC, numero) -> velocity
        using LedMap = std::map<std::pair<bool, int>, int>;

        LedMap ledsFor (const SurfaceState& st)
        {
            LedMap leds;
            for (int i = 0; i < 64; ++i)
            {
                Led l = Led::off;
                if (i == st.loadingPatch)        l = Led::redFlash;
                else if (i == st.activePatch)    l = Led::green;
                else if (st.occupied[(size_t) i]) l = Led::amber;
                leds[{ false, gridNote (i) }] = velocityFor (l);
            }

            for (int b = 0; b < 8; ++b)
            {
                Led l = b == st.bank ? Led::green : (st.bankOccupied[(size_t) b] ? Led::amber : Led::off);
                leds[{ true, firstTopCc + b }] = velocityFor (l);
            }

            for (int m = 0; m < 4; ++m)
                leds[{ false, rightNote (m) }] = velocityFor (st.muted[(size_t) m] ? Led::red : Led::off);
            leds[{ false, rightNote (4) }] = velocityFor (Led::amber);                           // precedente
            leds[{ false, rightNote (5) }] = velocityFor (Led::amber);                           // successiva
            leds[{ false, rightNote (6) }] = velocityFor (st.tapLit ? Led::green : Led::off);    // TAP
            leds[{ false, rightNote (7) }] = velocityFor (Led::red);                             // PANIC
            return leds;
        }

        juce::MidiMessage ledMessage (bool isCc, int number, int velocity)
        {
            return isCc ? juce::MidiMessage::controllerEvent (1, number, velocity)
                        : juce::MidiMessage::noteOn (1, number, (juce::uint8) velocity);
        }
    }

    SurfaceAction decode (const juce::MidiMessage& m)
    {
        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc >= firstTopCc && cc < firstTopCc + 8 && m.getControllerValue() > 0)
                return { SurfaceActionType::bank, cc - firstTopCc };
            return {};
        }

        if (! (m.isNoteOn (true) || m.isNoteOff()))
            return {};

        const int note = m.getNoteNumber();
        const int row = note / 16, col = note % 16;
        const bool pressed = m.isNoteOn();   // velocity 0 conta come rilascio

        if (row > 7)
            return {};

        if (col == rightColumn && row == 7)
            return { pressed ? SurfaceActionType::panicDown : SurfaceActionType::panicUp, 0 };

        if (! pressed)
            return {};

        if (col < 8)
            return { SurfaceActionType::patch, row * 8 + col };

        if (col == rightColumn)
        {
            if (row < 4)  return { SurfaceActionType::mute, row };
            if (row == 4) return { SurfaceActionType::previous, 0 };
            if (row == 5) return { SurfaceActionType::next, 0 };
            if (row == 6) return { SurfaceActionType::tap, 0 };
        }
        return {};
    }

    int velocityFor (Led l)
    {
        // velocity = 16 * verde + rosso + flag (12 = normale, 8 = lampeggio)
        switch (l)
        {
            case Led::red:      return 15;
            case Led::green:    return 60;
            case Led::amber:    return 63;
            case Led::redFlash: return 11;
            case Led::off:      break;
        }
        return 12;
    }

    std::vector<juce::MidiMessage> ledMessages (const SurfaceState& st, const SurfaceState* previous)
    {
        const auto now = ledsFor (st);
        LedMap before;
        if (previous != nullptr)
            before = ledsFor (*previous);

        std::vector<juce::MidiMessage> out;
        for (const auto& [key, vel] : now)
        {
            if (previous != nullptr)
                if (auto it = before.find (key); it != before.end() && it->second == vel)
                    continue;
            out.push_back (ledMessage (key.first, key.second, vel));
        }
        return out;
    }

    std::vector<juce::MidiMessage> initMessages()
    {
        return { juce::MidiMessage::controllerEvent (1, 0, 0),     // reset
                 juce::MidiMessage::controllerEvent (1, 0, 1),     // layout X-Y
                 juce::MidiMessage::controllerEvent (1, 0, 40) };  // lampeggio automatico
    }
}
