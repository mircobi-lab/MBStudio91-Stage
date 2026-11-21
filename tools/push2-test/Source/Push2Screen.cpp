#include "Push2Screen.h"
#include "Push2Protocol.h"

juce::Image renderScreen (const std::vector<ScreenSetup>& setups, int active,
                          const juce::String& firmware, juce::Typeface::Ptr typeface)
{
    juce::Image img (juce::Image::RGB, push2::displayWidth, push2::displayHeight, true);
    juce::Graphics g (img);
    auto font = [&typeface] (float h) { return juce::Font (juce::FontOptions().withTypeface (typeface).withHeight (h)); };

    g.fillAll (juce::Colours::black);

    // In alto: una casella per tasto sopra il display (120 px l'una)
    const int cellWidth = push2::displayWidth / 8;
    for (int i = 0; i < (int) setups.size() && i < 8; ++i)
    {
        juce::Rectangle<int> cell (i * cellWidth + 4, 4, cellWidth - 8, 30);
        const auto& s = setups[(size_t) i];
        if (i == active)
        {
            g.setColour (s.colour);
            g.fillRect (cell);
            g.setColour (juce::Colours::black);
        }
        else
        {
            g.setColour (s.colour.withMultipliedBrightness (0.6f));
            g.drawRect (cell, 1);
        }
        g.setFont (font (24.0f));
        g.drawText (s.name.toUpperCase(), cell, juce::Justification::centred);
    }

    if (active >= 0 && active < (int) setups.size())
    {
        const auto& s = setups[(size_t) active];
        g.setColour (s.colour);
        g.setFont (font (78.0f));
        g.drawText (s.name.toUpperCase(), 0, 38, push2::displayWidth, 82, juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff9a9a9a));
    g.setFont (font (22.0f));
    juce::String footer = "MBSTUDIO91 STAGE  -  TEST PUSH 2";
    if (firmware.isNotEmpty())
        footer << "   -   FIRMWARE " << firmware;
    g.drawText (footer, 12, 128, 936, 28, juce::Justification::centredLeft);
    g.drawText ("SETUP " + juce::String (active + 1) + " / " + juce::String ((int) setups.size()),
                12, 128, 936, 28, juce::Justification::centredRight);
    return img;
}
