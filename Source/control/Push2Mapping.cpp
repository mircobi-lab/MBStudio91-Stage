#include "Push2Mapping.h"
#include <map>

namespace Push2Mapping
{
    namespace
    {
        // Manuale Ableton: pad 36-99, nota = 36 + 8*riga + colonna con riga 0 in basso.
        constexpr int firstPad = 36, lastPad = 99;

        constexpr int firstBankCc = 102;    // pulsanti sopra il display
        constexpr int firstMuteCc = 20;     // pulsanti sotto il display
        constexpr int previousCc = 24, nextCc = 25;
        constexpr int tapCc = 3, panicCc = 29;

        constexpr int firstEncoderCc = 71, masterEncoderCc = 79, tempoEncoderCc = 14;

        // La patch 0 e' in alto a sinistra, come sullo schermo e sulla Launchpad.
        int padToPatch (int note)
        {
            const int offset = note - firstPad;
            return (7 - offset / 8) * 8 + offset % 8;
        }

        int patchToPad (int patch)
        {
            return firstPad + (7 - patch / 8) * 8 + patch % 8;
        }

        using LedMap = std::map<std::pair<bool, int>, int>;   // (true = CC, numero) -> indice tavolozza

        LedMap ledsFor (const SurfaceState& st)
        {
            LedMap leds;

            for (int i = 0; i < 64; ++i)
            {
                Colour c = Colour::off;
                if (i == st.loadingPatch)         c = Colour::red;
                else if (i == st.activePatch)     c = Colour::green;
                else if (st.occupied[(size_t) i]) c = Colour::blue;
                leds[{ false, patchToPad (i) }] = paletteIndex (c);
            }

            for (int b = 0; b < 8; ++b)
            {
                Colour c = b == st.bank ? Colour::green
                                        : (st.bankOccupied[(size_t) b] ? Colour::blue : Colour::off);
                leds[{ true, firstBankCc + b }] = paletteIndex (c);
            }

            for (int m = 0; m < 4; ++m)
                leds[{ true, firstMuteCc + m }] = paletteIndex (st.muted[(size_t) m] ? Colour::red : Colour::darkGrey);

            leds[{ true, previousCc }] = paletteIndex (Colour::darkGrey);
            leds[{ true, nextCc }]     = paletteIndex (Colour::darkGrey);
            leds[{ true, tapCc }]      = paletteIndex (st.tapLit ? Colour::green : Colour::darkGrey);
            leds[{ true, panicCc }]    = paletteIndex (Colour::red);
            return leds;
        }

        juce::MidiMessage ledMessage (bool isCc, int number, int value)
        {
            // Canale 1 in JUCE = canale 0 sul cavo: nessuna animazione (manuale Ableton).
            return isCc ? juce::MidiMessage::controllerEvent (1, number, value)
                        : juce::MidiMessage::noteOn (1, number, (juce::uint8) value);
        }

        juce::MidiMessage midiModeMessage (juce::uint8 mode)
        {
            const juce::uint8 data[] = { 0x00, 0x21, 0x1d, 0x01, 0x01, 0x0a, mode };
            return juce::MidiMessage::createSysExMessage (data, (int) sizeof (data));
        }
    }

    int paletteIndex (Colour c)
    {
        switch (c)
        {
            case Colour::white:     return 122;
            case Colour::lightGrey: return 123;
            case Colour::darkGrey:  return 124;
            case Colour::blue:      return 125;
            case Colour::green:     return 126;
            case Colour::red:       return 127;
            case Colour::off:       break;
        }
        return 0;
    }

    int encoderIndex (const juce::MidiMessage& m)
    {
        if (! m.isController())
            return -1;
        const int cc = m.getControllerNumber();
        if (cc >= firstEncoderCc && cc < firstEncoderCc + 8) return cc - firstEncoderCc;
        if (cc == masterEncoderCc)                           return 8;
        if (cc == tempoEncoderCc)                            return 9;
        return -1;
    }

    int encoderDelta (const juce::MidiMessage& m)
    {
        if (encoderIndex (m) < 0)
            return 0;
        const int v = m.getControllerValue();
        return v >= 64 ? v - 128 : v;   // relativo a 7 bit in complemento a due
    }

    SurfaceAction decode (const juce::MidiMessage& m)
    {
        if (m.isNoteOn() && m.getNoteNumber() >= firstPad && m.getNoteNumber() <= lastPad)
            return { SurfaceActionType::patch, padToPatch (m.getNoteNumber()), 0 };

        if (! m.isController())
            return {};

        const int enc = encoderIndex (m);
        if (enc >= 0)
        {
            const int delta = encoderDelta (m);
            if (delta == 0)
                return {};
            if (enc < 4)   return { SurfaceActionType::stationVolume, enc, delta };
            if (enc == 8)  return { SurfaceActionType::masterVolume, 0, delta };
            if (enc == 9)  return { SurfaceActionType::tempoDelta, 0, delta };
            return {};     // encoder 5-8: non assegnati
        }

        const int cc = m.getControllerNumber();
        const bool pressed = m.getControllerValue() > 0;

        if (cc == panicCc)
            return { pressed ? SurfaceActionType::panicDown : SurfaceActionType::panicUp, 0, 0 };

        if (! pressed)
            return {};

        if (cc >= firstBankCc && cc < firstBankCc + 8) return { SurfaceActionType::bank, cc - firstBankCc, 0 };
        if (cc >= firstMuteCc && cc < firstMuteCc + 4) return { SurfaceActionType::mute, cc - firstMuteCc, 0 };
        if (cc == previousCc)                          return { SurfaceActionType::previous, 0, 0 };
        if (cc == nextCc)                              return { SurfaceActionType::next, 0, 0 };
        if (cc == tapCc)                               return { SurfaceActionType::tap, 0, 0 };
        return {};
    }

    std::vector<juce::MidiMessage> ledMessages (const SurfaceState& st, const SurfaceState* previous)
    {
        const auto now = ledsFor (st);
        LedMap before;
        if (previous != nullptr)
            before = ledsFor (*previous);

        std::vector<juce::MidiMessage> out;
        for (const auto& [key, value] : now)
        {
            if (previous != nullptr)
                if (auto it = before.find (key); it != before.end() && it->second == value)
                    continue;
            out.push_back (ledMessage (key.first, key.second, value));
        }
        return out;
    }

    std::vector<juce::MidiMessage> initMessages()
    {
        return { midiModeMessage (0x01) };   // User mode: Stage non disturba Ableton Live
    }

    std::vector<juce::MidiMessage> shutdownMessages()
    {
        std::vector<juce::MidiMessage> out;
        SurfaceState dark;   // tutto spento
        for (const auto& [key, value] : ledsFor (dark))
            out.push_back (ledMessage (key.first, key.second, value == paletteIndex (Colour::darkGrey)
                                                               || value == paletteIndex (Colour::red) ? 0 : value));
        out.push_back (midiModeMessage (0x00));   // il Push 2 torna com'era per Ableton Live
        return out;
    }
}
