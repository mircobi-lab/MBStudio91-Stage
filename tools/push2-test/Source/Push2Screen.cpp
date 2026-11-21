#include "Push2Screen.h"
#include "Push2Protocol.h"

using Style = TextPart::Style;

namespace
{
const juce::Colour white = juce::Colours::white;

// Fonte: segnalazione di un utente su Gearspace, sulla Mini MK1 "LAUNCH" e' arancione
// (sulla MK2 bianco). Da verificare sul pannello.
const juce::Colour launchOrange { 0xffff8a00 };

TextPart part (const juce::String& t, Style s, float gap = 0.0f, juce::Colour c = white, float scale = 1.0f)
{
    TextPart p;
    p.text = t;
    p.style = s;
    p.gapBefore = gap;
    p.colour = c;
    p.scale = scale;
    return p;
}

TextPart mk (const juce::String& t)
{
    return part (t, Style::regular, 0.14f, juce::Colour (0xffb0b0b0), 0.5f);
}
} // namespace

namespace devices
{
// LAUNCH grassetto, KEY normale, MINI in negativo: come indicato da Mirco
StyledText launchkeyMiniMk1()
{
    return { part ("LAUNCH", Style::bold), part ("KEY", Style::regular),
             part ("MINI", Style::inverted, 0.16f), mk ("MK1") };
}

StyledText launchkeyMiniMk2()
{
    return { part ("LAUNCH", Style::bold), part ("KEY", Style::regular),
             part ("MINI", Style::inverted, 0.16f), mk ("MK2") };
}

// Stesso schema della famiglia Launch; il trattamento del "61" e' da verificare
StyledText launchkey61Mk2()
{
    return { part ("LAUNCH", Style::bold), part ("KEY", Style::regular),
             part ("61", Style::regular, 0.16f), mk ("MK2") };
}

// Da verificare sul pannello: per ora scritta normale
StyledText keystation88Mk3()
{
    return { part ("KEYSTATION", Style::regular), part ("88", Style::regular, 0.16f), mk ("MK3") };
}

StyledText launchpadMiniMk1()
{
    return { part ("LAUNCH", Style::bold, 0.0f, launchOrange), part ("PAD", Style::regular),
             part ("MINI", Style::inverted, 0.16f) };
}
} // namespace devices

StyledText pluginName (const juce::String& name, juce::Colour colour)
{
    const auto upper = name.toUpperCase();
    if (upper.isEmpty())
        return {};
    StyledText t { part (upper.substring (0, 1), Style::bold, 0.0f, colour) };
    if (upper.length() > 1)
        t.push_back (part (upper.substring (1), Style::regular, 0.0f, colour));
    return t;
}

StyledText logoStyleName (const juce::String& name, juce::Colour colour)
{
    StyledText t;
    const auto upper = name.toUpperCase();
    for (int i = 0; i < upper.length();)
    {
        const bool digit = juce::CharacterFunctions::isDigit (upper[i]);
        int j = i;
        while (j < upper.length() && juce::CharacterFunctions::isDigit (upper[j]) == digit)
            ++j;
        auto p = part (upper.substring (i, j), Style::regular, 0.0f, colour);
        p.face = digit ? TextPart::Face::logoLight : TextPart::Face::logoBold;
        t.push_back (p);
        i = j;
    }
    return t;
}

float drawStyledText (juce::Graphics& g, const StyledText& parts, float x, float baseline,
                      float height, const Faces& faces, bool draw)
{
    const float startX = x;

    for (const auto& p : parts)
    {
        const float h = height * p.scale;
        x += p.gapBefore * height;

        const auto& face = p.face == TextPart::Face::logoLight ? faces.logoLight
                         : p.face == TextPart::Face::logoBold  ? faces.logoBold
                                                               : faces.bebas;
        juce::Font font (juce::FontOptions().withTypeface (face).withHeight (h));
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, p.text, 0.0f, 0.0f);
        const float advance = ga.getBoundingBox (0, -1, true).getWidth();

        juce::Path path;
        ga.createPath (path);
        const auto ink = path.getBounds();

        // Il Bebas Neue libero ha un solo peso: il grassetto si ottiene ripassando il contorno
        // Comfortaa Bold e' meno spesso dello STUDIO del logo: lo ispessisco allo stesso modo
        const bool heavy = p.style == Style::bold || p.face == TextPart::Face::logoBold;
        const float boldStroke = h * (p.face == TextPart::Face::logoBold ? 0.075f : 0.05f);
        const float pad = h * 0.14f;

        float width = advance;
        if (heavy)
            width += boldStroke;
        if (p.style == Style::inverted)
            width += 2.0f * pad;

        if (draw)
        {
            const float textX = x + (p.style == Style::inverted ? pad : (heavy ? boldStroke * 0.5f : 0.0f));
            const auto placed = juce::AffineTransform::translation (textX, baseline);

            if (p.style == Style::inverted)
            {
                g.setColour (p.colour);
                g.fillRect (juce::Rectangle<float> (x, baseline + ink.getY() - pad,
                                                    width, ink.getHeight() + 2.0f * pad));
                g.setColour (juce::Colours::black);
                g.fillPath (path, placed);
            }
            else
            {
                g.setColour (p.colour);
                g.fillPath (path, placed);
                if (heavy)
                    g.strokePath (path, juce::PathStrokeType (boldStroke, p.face == TextPart::Face::logoBold
                                                                              ? juce::PathStrokeType::curved
                                                                              : juce::PathStrokeType::mitered,
                                                              juce::PathStrokeType::rounded), placed);
            }
        }

        x += width;
    }

    return x - startX;
}

juce::Image renderScreen (const std::vector<ScreenSetup>& setups, int active,
                          const StyledText& setupChanger,
                          const juce::String& firmware, const Faces& faces)
{
    juce::Image img (juce::Image::RGB, push2::displayWidth, push2::displayHeight, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colours::black);

    // In alto: una casella per tasto sopra il display (120 px l'una)
    const int cellWidth = push2::displayWidth / 8;
    for (int i = 0; i < (int) setups.size() && i < 8; ++i)
    {
        juce::Rectangle<float> cell ((float) (i * cellWidth + 4), 4.0f, (float) (cellWidth - 8), 30.0f);
        const auto& s = setups[(size_t) i];
        const bool on = (i == active);

        if (on)
        {
            g.setColour (s.colour);
            g.fillRect (cell);
        }
        else
        {
            g.setColour (s.colour.withMultipliedBrightness (0.6f));
            g.drawRect (cell, 1.0f);
        }

        const auto label = pluginName (s.plugin, on ? juce::Colours::black : s.colour.withMultipliedBrightness (0.85f));
        const float h = 24.0f;
        const float w = drawStyledText (g, label, 0, 0, h, faces, false);
        drawStyledText (g, label, cell.getCentreX() - w * 0.5f, cell.getCentreY() + h * 0.36f, h, faces);
    }

    // Al centro: plugin e periferica
    if (active >= 0 && active < (int) setups.size())
    {
        const auto& s = setups[(size_t) active];
        const auto name = logoStyleName (s.plugin, s.colour);
        const float deviceH = 34.0f, gap = 30.0f, baseline = 104.0f;
        float nameH = 66.0f;

        const float devW = s.device.empty() ? 0.0f : drawStyledText (g, s.device, 0, 0, deviceH, faces, false);
        float nameW = drawStyledText (g, name, 0, 0, nameH, faces, false);
        const float room = 920.0f - (devW > 0 ? gap + devW : 0.0f);
        if (nameW > room) // nomi lunghi: rimpicciolisce solo il nome
        {
            nameH *= room / nameW;
            nameW = room;
        }
        const float total = nameW + (devW > 0 ? gap + devW : 0.0f);
        float x = (push2::displayWidth - total) * 0.5f;

        x += drawStyledText (g, name, x, baseline, nameH, faces);
        if (devW > 0)
        {
            g.setColour (juce::Colour (0xff444444));
            g.fillRect (x + gap * 0.5f - 1.0f, baseline - 52.0f, 2.0f, 52.0f);
            drawStyledText (g, s.device, x + gap, baseline, deviceH, faces);
        }
    }

    // In basso: firma, firmware, chi cambia i setup
    juce::Font small (juce::FontOptions().withTypeface (faces.bebas).withHeight (22.0f));
    g.setColour (juce::Colour (0xff9a9a9a));
    g.setFont (small);
    juce::String footer = "MBSTUDIO91 STAGE";
    if (firmware.isNotEmpty())
        footer << "   -   PUSH 2 FW " << firmware;
    g.drawText (footer, 12, 128, 600, 28, juce::Justification::centredLeft);

    const float changerH = 22.0f;
    const float changerW = drawStyledText (g, setupChanger, 0, 0, changerH, faces, false);
    const float changerX = 948.0f - changerW;
    drawStyledText (g, setupChanger, changerX, 150.0f, changerH, faces);

    g.setColour (juce::Colour (0xff9a9a9a));
    g.setFont (small);
    g.drawText ("SETUP " + juce::String (active + 1) + "/" + juce::String ((int) setups.size()) + "  DA",
                (int) changerX - 200, 128, 190, 28, juce::Justification::centredRight);
    return img;
}
