#include "Push2Mapping.h"
#include "push2/Push2Protocol.h"

#include <map>

namespace Push2Mapping
{
    namespace
    {
        // I nostri colori, nell'ordine di Led
        struct Rgb { juce::uint8 r, g, b; };
        constexpr Rgb colours[numPaletteEntries]
        {
            { 0x3d, 0x8b, 0xff },   // active: blu MBStudio
            { 0x3a, 0x3a, 0x3a },   // occupied: grigio spento
            { 0xff, 0xb0, 0x20 },   // loading: ambra
            { 0xff, 0x3b, 0x30 },   // muted: rosso
            { 0xf3, 0xf3, 0xf1 },   // action: bianco
            { 0x3e, 0xe0, 0x5c },   // tap: verde
        };

        using LedMap = std::map<std::pair<bool, int>, int>;   // (isCc, numero) -> indice di palette

        // Colonna a destra dei pad: 0 in alto. Le prime quattro sono i mute.
        int rightColumnCC (int rowFromTop) { return push2::sceneButtonCC (rowFromTop); }

        LedMap ledsFor (const SurfaceState& st)
        {
            LedMap leds;

            for (int i = 0; i < 64; ++i)
            {
                Led l = Led::off;
                if (i == st.loadingPatch)         l = Led::loading;
                else if (i == st.activePatch)     l = Led::active;
                else if (st.occupied[(size_t) i]) l = Led::occupied;
                leds[{ false, padForPatch (i) }] = paletteIndexFor (l);
            }

            for (int b = 0; b < 8; ++b)
            {
                const Led l = b == st.bank ? Led::active
                            : st.bankOccupied[(size_t) b] ? Led::occupied : Led::off;
                leds[{ true, push2::upperDisplayButtonCC + b }] = paletteIndexFor (l);
            }

            for (int m = 0; m < 4; ++m)
                leds[{ true, rightColumnCC (m) }] = paletteIndexFor (st.muted[(size_t) m] ? Led::muted : Led::off);

            leds[{ true, rightColumnCC (4) }] = paletteIndexFor (Led::action);                       // precedente
            leds[{ true, rightColumnCC (5) }] = paletteIndexFor (Led::action);                       // successiva
            leds[{ true, rightColumnCC (6) }] = paletteIndexFor (st.tapLit ? Led::tap : Led::off);   // TAP
            leds[{ true, rightColumnCC (7) }] = paletteIndexFor (Led::muted);                        // PANIC
            return leds;
        }

        juce::MidiMessage ledMessage (bool isCc, int number, int value)
        {
            return isCc ? juce::MidiMessage::controllerEvent (1, number, value)
                        : juce::MidiMessage::noteOn (1, number, (juce::uint8) value);
        }

        juce::MidiMessage sysex (const push2::Bytes& b)
        {
            return juce::MidiMessage::createSysExMessage (b.data() + 1, (int) b.size() - 2);
        }
    }

    int paletteIndexFor (Led l)
    {
        switch (l)
        {
            case Led::active:   return firstPaletteIndex;
            case Led::occupied: return firstPaletteIndex + 1;
            case Led::loading:  return firstPaletteIndex + 2;
            case Led::muted:    return firstPaletteIndex + 3;
            case Led::action:   return firstPaletteIndex + 4;
            case Led::tap:      return firstPaletteIndex + 5;
            case Led::off:      break;
        }
        return push2::colourBlack;
    }

    std::vector<juce::MidiMessage> paletteMessages()
    {
        std::vector<juce::MidiMessage> out;
        for (int i = 0; i < numPaletteEntries; ++i)
        {
            const auto& c = colours[i];
            const auto white = (juce::uint8) ((c.r * 2 + c.g * 5 + c.b) / 8);
            out.push_back (sysex (push2::setPaletteEntry (firstPaletteIndex + i, c.r, c.g, c.b, white)));
        }
        out.push_back (sysex (push2::reapplyPalette()));
        return out;
    }

    std::vector<juce::MidiMessage> readPaletteMessages()
    {
        std::vector<juce::MidiMessage> out;
        for (int i = 0; i < numPaletteEntries; ++i)
            out.push_back (sysex ({ 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x04,
                                    (std::uint8_t) (firstPaletteIndex + i), 0xF7 }));
        return out;
    }

    int padForPatch (int index0to63)
    {
        const int i = juce::jlimit (0, 63, index0to63);
        return push2::padNote (i / 8, i % 8);         // patch 0 = pad in alto a sinistra
    }

    int patchForPad (int note)
    {
        if (auto pos = push2::padPosition (note))
            return pos->first * 8 + pos->second;
        return -1;
    }

    // I rotativi mandano un valore relativo in complemento a due: 0 vuol dire fermo.
    SurfaceAction encoderAction (int cc, int value)
    {
        const int delta = push2::encoderDelta (value);
        if (delta == 0)
            return {};

        if (cc >= push2::trackEncoderCC && cc < push2::trackEncoderCC + push2::numTrackEncoders)
        {
            const int index = cc - push2::trackEncoderCC;
            // I primi quattro sulle postazioni; gli altri restano liberi per una fase successiva.
            return index < 4 ? SurfaceAction { SurfaceActionType::stationVolume, index, delta }
                             : SurfaceAction {};
        }

        if (cc == push2::masterEncoderCC) return { SurfaceActionType::masterVolume, 0, delta };
        if (cc == push2::tempoEncoderCC)  return { SurfaceActionType::tempoDelta, 0, delta };
        return {};
    }

    SurfaceAction decode (const juce::MidiMessage& m)
    {
        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const bool pressed = m.getControllerValue() > 0;

            if (const auto enc = encoderAction (cc, m.getControllerValue()); enc.type != SurfaceActionType::none)
                return enc;

            if (cc == push2::sceneButtonCC (7))
                return { pressed ? SurfaceActionType::panicDown : SurfaceActionType::panicUp, 0 };

            if (! pressed)
                return {};

            if (cc >= push2::upperDisplayButtonCC && cc < push2::upperDisplayButtonCC + 8)
                return { SurfaceActionType::bank, cc - push2::upperDisplayButtonCC };

            for (int row = 0; row < 7; ++row)
                if (cc == push2::sceneButtonCC (row))
                {
                    if (row < 4)  return { SurfaceActionType::mute, row };
                    if (row == 4) return { SurfaceActionType::previous, 0 };
                    if (row == 5) return { SurfaceActionType::next, 0 };
                    return { SurfaceActionType::tap, 0 };
                }

            if (cc == push2::tapTempoCC)
                return { SurfaceActionType::tap, 0 };

            return {};
        }

        if (m.isNoteOn())
            if (const int patch = patchForPad (m.getNoteNumber()); patch >= 0)
                return { SurfaceActionType::patch, patch };

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

    std::vector<juce::MidiMessage> allOffMessages()
    {
        std::vector<juce::MidiMessage> out;
        for (int note = 36; note <= 99; ++note)
            out.push_back (juce::MidiMessage::noteOn (1, note, (juce::uint8) push2::colourBlack));
        for (int b = 0; b < 8; ++b)
            out.push_back (juce::MidiMessage::controllerEvent (1, push2::upperDisplayButtonCC + b, push2::colourBlack));
        for (int row = 0; row < 8; ++row)
            out.push_back (juce::MidiMessage::controllerEvent (1, push2::sceneButtonCC (row), push2::colourBlack));
        return out;
    }
}
