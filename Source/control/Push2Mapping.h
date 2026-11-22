#pragma once

#include "ControlSurface.h"
#include <vector>

// Ableton Push 2. Numeri presi dal manuale ufficiale "Push 2 MIDI and Display Interface"
// (github.com/Ableton/push-interface).
namespace Push2Mapping
{
    SurfaceAction decode (const juce::MidiMessage&);

    int encoderDelta (const juce::MidiMessage&);   // scatti: >0 a destra, <0 a sinistra, 0 se non e' un encoder
    int encoderIndex (const juce::MidiMessage&);   // 0-7 sopra il display, 8 master, 9 tempo, -1 altro

    // Solo indici della tavolozza documentata da Ableton: niente colori inventati.
    enum class Colour { off, white, lightGrey, darkGrey, blue, green, red };
    int paletteIndex (Colour);

    std::vector<juce::MidiMessage> ledMessages (const SurfaceState&, const SurfaceState* previous);
    std::vector<juce::MidiMessage> initMessages();
    std::vector<juce::MidiMessage> shutdownMessages();
}
