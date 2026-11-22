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
};

// Disegna la schermata 960x160 del Push 2
juce::Image renderScreen (const std::vector<ScreenSetup>& setups, int active,
                          const StyledText& setupChanger,
                          const juce::String& firmware, const Faces& faces);

// Disegna una scritta composta con la base del testo a "baseline"; restituisce la larghezza.
// Con draw = false misura soltanto.
float drawStyledText (juce::Graphics& g, const StyledText& parts, float x, float baseline,
                      float height, const Faces& faces, bool draw = true);
