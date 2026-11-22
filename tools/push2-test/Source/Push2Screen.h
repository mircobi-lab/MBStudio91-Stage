#pragma once

#include <juce_graphics/juce_graphics.h>

#include <vector>

// Un pezzo di scritta: normale, grassetto o in negativo (testo nero su riquadro pieno)
struct TextPart
{
    enum class Style { regular, bold, inverted };
    enum class Face { bebas, logoLight, logoBold, script, square };

    juce::String text;
    Style style = Style::regular;
    juce::Colour colour = juce::Colours::white;
    float scale = 1.0f;     // per le sigle piccole tipo MK2
    float gapBefore = 0.0f; // spazio prima del pezzo, in frazioni dell'altezza
    Face face = Face::bebas;
};

struct Faces
{
    juce::Typeface::Ptr bebas, logoLight, logoBold;
    juce::Typeface::Ptr script, square; // somiglianti ai marchi Sylenth1 e DUNE
    juce::Image logo; // maschera bianca del marchio MBSTUDIO91
};

using StyledText = std::vector<TextPart>;

// Le periferiche di Mirco, scritte come sul pannello
namespace devices
{
StyledText launchkeyMiniMk1();
StyledText launchkeyMiniMk2();
StyledText launchkey61Mk2();
StyledText keystation88Mk3();
StyledText launchpadMiniMk1();
} // namespace devices

// Nome del plugin nelle caselle in alto: tutto testo normale
StyledText cellName (const juce::String& name, juce::Colour colour);

// Nome al centro, stile logo MBSTUDIO91: solo l'iniziale spessa, il resto sottile
StyledText logoStyleName (const juce::String& name, juce::Colour colour);

// Nome al centro in un carattere somigliante al marchio del plugin
StyledText brandName (const juce::String& name, juce::Colour colour, TextPart::Face face);

struct ScreenSetup
{
    juce::String plugin;
    StyledText device;
    juce::Colour colour;
    // Come scrivere il nome al centro: logoBold = stile MBSTUDIO91,
    // script o square = carattere somigliante al marchio del plugin
    TextPart::Face nameFace = TextPart::Face::logoBold;
    float volume = 0.75f; // 0..1, dal rotativo sopra la casella
    bool locked = false;  // volume bloccato: il rotativo non lo muove piu'
    bool muted = false;   // dal tasto sotto il display
    int transpose = 0;    // ottave, dai tasti Octave
};

// Tutto quello che non appartiene a un singolo setup
struct ScreenState
{
    int active = 0;
    int touched = -1;         // rotativo tenuto col dito, -1 nessuno
    float master = 1.0f;
    bool masterMuted = false;
    int bpm = 120;
    bool metronome = false;
    bool beat = false;        // lampeggio a tempo
    int scene = -1;           // ultima scena richiamata
    bool shift = false;
    float strip = 0.5f;       // 0..1, posizione della striscia
    bool stripTouched = false;
    bool stripMod = false;    // false = pitch bend, true = mod wheel
    bool polyAftertouch = false;
    int aftertouch = 0;       // 0..127
    bool sustain = false;     // pedale 1
    int pedal2 = -1;          // -1 mai visto, altrimenti 0..127
    bool pedal2Continuous = false;
    juce::String firmware;
};

// Disegna la schermata 960x160 del Push 2
juce::Image renderScreen (const std::vector<ScreenSetup>& setups, const ScreenState& st,
                          const StyledText& setupChanger, const Faces& faces);

// Disegna una scritta composta con la base del testo a "baseline"; restituisce la larghezza.
// Con draw = false misura soltanto.
float drawStyledText (juce::Graphics& g, const StyledText& parts, float x, float baseline,
                      float height, const Faces& faces, bool draw = true);
