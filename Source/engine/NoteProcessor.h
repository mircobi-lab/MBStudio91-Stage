#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <optional>

enum class VelocityCurve { lineare, morbida, dura, personalizzata };

// Impostazioni di tastiera di una postazione.
struct KeyZone
{
    int transpose = 0;                 // -48..+48 semitoni
    int lowNote = 0, highNote = 127;   // tasto fisico, prima della trasposizione
    VelocityCurve curve = VelocityCurve::lineare;
    float customGamma = 1.0f;          // 0.3..3.0, solo con la curva personalizzata
    bool faderControlsVolume = false;  // CC7 della tastiera = volume della postazione
    bool volumeLocked = false;         // LOCK: il volume non si muove piu', da nessuna parte

    juce::ValueTree toValueTree() const;
    static KeyZone fromValueTree (const juce::ValueTree&);
    bool operator== (const KeyZone&) const;
};

int applyVelocityCurve (int velocity1to127, VelocityCurve, float customGamma);
float faderValueToDb (int cc7value);

// Trasforma il MIDI di una tastiera per la sua postazione.
class NoteProcessor
{
public:
    NoteProcessor();

    void setZone (const KeyZone&);   // qualsiasi thread
    KeyZone getZone() const;

    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, std::optional<float>& faderDb);
    void reset();

private:
    mutable juce::SpinLock zoneLock;
    KeyZone zone, audioZone;
    std::array<std::array<juce::int8, 128>, 16> playedNote;   // [canale][tasto fisico] -> nota suonata, -1 = nessuna
};
