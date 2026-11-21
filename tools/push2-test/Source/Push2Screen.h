#pragma once

#include <juce_graphics/juce_graphics.h>

#include <vector>

struct ScreenSetup
{
    juce::String name;
    juce::Colour colour;
};

// Disegna la schermata 960x160 del Push 2: 8 caselle sotto i tasti in alto,
// il nome del setup attivo al centro, la firma MBStudio91 in basso.
juce::Image renderScreen (const std::vector<ScreenSetup>& setups, int active,
                          const juce::String& firmware, juce::Typeface::Ptr typeface);
