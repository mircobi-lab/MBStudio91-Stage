#pragma once

#include "ControlSurface.h"
#include <vector>

// Ableton Push 2 come selettore di patch. Numeri presi da doc/Push2-map.json di Ableton
// (vedi Source/push2/Push2Protocol.h): pad note 36-99, tasti sopra il display CC 102-109,
// colonna a destra dei pad CC 43 in alto fino a 36 in basso.
namespace Push2Mapping
{
    SurfaceAction decode (const juce::MidiMessage&);

    // Voci di palette che ci riserviamo: il Push non ha colori fissi garantiti oltre
    // nero, bianco e rosso, quindi ce li definiamo e li rimettiamo a posto all'uscita.
    enum class Led { off, active, occupied, loading, muted, action, tap };
    constexpr int firstPaletteIndex = 1;
    constexpr int numPaletteEntries = 6;

    int paletteIndexFor (Led);
    std::vector<juce::MidiMessage> paletteMessages();   // definisce i colori e li applica
    std::vector<juce::MidiMessage> readPaletteMessages();  // chiede le voci originali

    int patchForPad (int note);       // -1 se il pad non e' uno dei 64
    int padForPatch (int index0to63);

    std::vector<juce::MidiMessage> ledMessages (const SurfaceState&, const SurfaceState* previous);
    std::vector<juce::MidiMessage> allOffMessages();
}
