#include "Push2SurfaceScreen.h"
#include "Push2Protocol.h"

namespace
{
const juce::Colour print { 0xfff3f3f1 }, printDim { 0xff8e9094 };
const juce::Colour ledBlue { 0xff3d8bff }, ledAmber { 0xffffb020 }, ledRed { 0xffff3b30 };

juce::Font font (juce::Typeface::Ptr face, float height)
{
    return juce::Font (juce::FontOptions().withTypeface (face).withHeight (height));
}
}

juce::Image renderSurfaceScreen (const SurfaceState& st, juce::Typeface::Ptr face)
{
    juce::Image img (juce::Image::RGB, push2::displayWidth, push2::displayHeight, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colours::black);

    // In alto: una casella per banco, allineata ai tasti sopra il display
    const int cellWidth = push2::displayWidth / 8;
    for (int b = 0; b < 8; ++b)
    {
        juce::Rectangle<float> cell ((float) (b * cellWidth + 6), 4.0f, (float) (cellWidth - 12), 26.0f);
        const bool here = b == st.bank;
        const juce::String letter (juce::String::charToString ((juce::juce_wchar) ('A' + b)));

        if (here)
        {
            g.setColour (ledBlue);
            g.fillRect (cell);
            g.setColour (juce::Colours::black);
        }
        else
        {
            g.setColour (st.bankOccupied[(size_t) b] ? printDim : juce::Colour (0xff2a2c2f));
            g.drawRect (cell, 1.0f);
            g.setColour (st.bankOccupied[(size_t) b] ? print : juce::Colour (0xff55585c));
        }
        g.setFont (font (face, 20.0f));
        g.drawText (letter, cell, juce::Justification::centred);
    }

    // Al centro: il nome della patch, con il pallino se e' modificata
    const bool loading = st.loadingPatch >= 0;
    juce::String name = st.patchName.isNotEmpty() ? st.patchName.toUpperCase()
                                                  : juce::String ("NESSUNA PATCH");
    if (loading)
        name = "CARICO...";

    g.setColour (loading ? ledAmber : (st.patchName.isNotEmpty() ? print : printDim));
    g.setFont (font (face, 64.0f));
    g.drawText (name, 24, 38, push2::displayWidth - 48, 66, juce::Justification::centredLeft);

    if (st.modified && ! loading)
    {
        g.setColour (ledAmber);
        g.fillEllipse ((float) push2::displayWidth - 46.0f, 60.0f, 14.0f, 14.0f);
        g.setFont (font (face, 18.0f));
        g.drawText ("MODIFICATA", push2::displayWidth - 230, 78, 200, 20, juce::Justification::centredRight);
    }

    // Riga bassa: patch, tempo, mute delle quattro postazioni
    g.setFont (font (face, 22.0f));
    g.setColour (printDim);
    g.drawText ("PATCH", 24, 116, 60, 24, juce::Justification::centredLeft);
    g.setColour (print);
    g.drawText (st.activePatch >= 0 ? juce::String (st.activePatch + 1) : juce::String ("-"),
                86, 116, 46, 24, juce::Justification::centredLeft);

    g.setColour (printDim);
    g.drawText ("BPM", 150, 116, 44, 24, juce::Justification::centredLeft);
    g.setColour (st.tapLit ? ledBlue : print);
    g.drawText (juce::String (juce::roundToInt (st.bpm)), 196, 116, 60, 24, juce::Justification::centredLeft);

    for (int m = 0; m < 4; ++m)
    {
        juce::Rectangle<float> box (300.0f + (float) m * 86.0f, 118.0f, 78.0f, 22.0f);
        const bool off = st.muted[(size_t) m];
        g.setColour (off ? ledRed : juce::Colour (0xff2a2c2f));
        off ? g.fillRect (box) : g.drawRect (box, 1.0f);
        g.setColour (off ? juce::Colours::black : printDim);
        g.setFont (font (face, 18.0f));
        g.drawText ("POST " + juce::String (m + 1), box, juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff6c6e72));
    g.setFont (font (face, 20.0f));
    g.drawText ("MBSTUDIO91 STAGE", push2::displayWidth - 260, 116, 236, 24, juce::Justification::centredRight);
    return img;
}
