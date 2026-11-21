#pragma once

#include <juce_graphics/juce_graphics.h>

#include <vector>

// Un pezzo di scritta: normale, grassetto o in negativo (testo nero su riquadro pieno)
struct TextPart
{
    enum class Style { regular, bold, inverted };

    juce::String text;
    Style style = Style::regular;
    juce::Colour colour = juce::Colours::white;
    float scale = 1.0f;     // per le sigle piccole tipo MK2
    float gapBefore = 0.0f; // spazio prima del pezzo, in frazioni dell'altezza
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

// Nome del plugin: prima lettera in grassetto, il resto normale
StyledText pluginName (const juce::String& name, juce::Colour colour);

struct ScreenSetup
{
    juce::String plugin;
    StyledText device;
    juce::Colour colour;
};

// Disegna la schermata 960x160 del Push 2
juce::Image renderScreen (const std::vector<ScreenSetup>& setups, int active,
                          const StyledText& setupChanger,
                          const juce::String& firmware, juce::Typeface::Ptr typeface);

// Disegna una scritta composta con la base del testo a "baseline"; restituisce la larghezza.
// Con draw = false misura soltanto.
float drawStyledText (juce::Graphics& g, const StyledText& parts, float x, float baseline,
                      float height, juce::Typeface::Ptr typeface, bool draw = true);
