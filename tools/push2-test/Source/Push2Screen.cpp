#include "Push2Screen.h"
#include "Push2Protocol.h"

using Style = TextPart::Style;

namespace
{
const juce::Colour white = juce::Colours::white;

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

// Riduce la maschera del logo alla misura voluta facendo la media dei pixel: a questa
// altezza il ridimensionamento normale sgranerebbe le lettere.
juce::Image shrinkMask (const juce::Image& src, int w, int h)
{
    juce::Image out (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    const juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (out, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < out.getHeight(); ++y)
    {
        const int y0 = y * in.height / out.getHeight();
        const int y1 = juce::jmax (y0 + 1, (y + 1) * in.height / out.getHeight());

        for (int x = 0; x < out.getWidth(); ++x)
        {
            const int x0 = x * in.width / out.getWidth();
            const int x1 = juce::jmax (x0 + 1, (x + 1) * in.width / out.getWidth());

            int sum = 0, n = 0;
            for (int sy = y0; sy < y1; ++sy)
                for (int sx = x0; sx < x1; ++sx, ++n)
                    sum += in.getPixelColour (sx, sy).getAlpha();

            dst.setPixelColour (x, y, juce::Colours::white.withAlpha ((juce::uint8) (sum / juce::jmax (1, n))));
        }
    }
    return out;
}
} // namespace

namespace devices
{
// Dalle foto del pannello mandate da Mirco: LAUNCH grassetto, KEY sottile,
// il modello in un riquadro bianco con la scritta nera. Il MK non e' stampato
// sul pannello, lo aggiungo piccolo per distinguere le sue tre tastiere.
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

StyledText launchkey61Mk2()
{
    return { part ("LAUNCH", Style::bold), part ("KEY", Style::regular),
             part ("61", Style::inverted, 0.16f), mk ("MK2") };
}

// Dalla foto del pannello: KEYSTATION grassetto attaccato a 88 sottile,
// poi MK3 nel riquadro chiaro con la scritta scura
StyledText keystation88Mk3()
{
    return { part ("KEYSTATION", Style::bold), part ("88", Style::regular),
             part ("MK3", Style::inverted, 0.16f, white, 0.62f) };
}

// Da verificare su una foto del pannello: per ora segue lo schema delle Launchkey
StyledText launchpadMiniMk1()
{
    return { part ("LAUNCH", Style::bold), part ("PAD", Style::regular),
             part ("MINI", Style::inverted, 0.16f) };
}
} // namespace devices

StyledText cellName (const juce::String& name, juce::Colour colour)
{
    return { part (name.toUpperCase(), Style::regular, 0.0f, colour) };
}

StyledText logoStyleName (const juce::String& name, juce::Colour colour)
{
    const auto upper = name.toUpperCase();
    if (upper.isEmpty())
        return {};

    auto first = part (upper.substring (0, 1), Style::regular, 0.0f, colour);
    first.face = TextPart::Face::logoBold;
    StyledText t { first };

    if (upper.length() > 1)
    {
        auto rest = part (upper.substring (1), Style::regular, 0.0f, colour);
        rest.face = TextPart::Face::logoLight;
        t.push_back (rest);
    }
    return t;
}

StyledText brandName (const juce::String& name, juce::Colour colour, TextPart::Face face)
{
    // Lo script va lasciato com'e' scritto: tutto maiuscolo sarebbe illeggibile
    auto p = part (face == TextPart::Face::script ? name : name.toUpperCase(),
                   Style::regular, 0.0f, colour);
    p.face = face;
    // lo script ha maiuscole piu' piccole a parita' di corpo: lo ingrandisco
    if (face == TextPart::Face::script)
        p.scale = 1.18f;
    return { p };
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
                         : p.face == TextPart::Face::script    ? faces.script
                         : p.face == TextPart::Face::square    ? faces.square
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
        const float padX = h * 0.20f, padY = h * 0.14f;

        float width = advance;
        if (heavy)
            width += boldStroke;
        if (p.style == Style::inverted)
            width += 2.0f * padX;

        if (draw)
        {
            const float textX = x + (p.style == Style::inverted ? padX : (heavy ? boldStroke * 0.5f : 0.0f));
            const auto placed = juce::AffineTransform::translation (textX, baseline);

            if (p.style == Style::inverted)
            {
                g.setColour (p.colour);
                const juce::Rectangle<float> box (x, baseline + ink.getY() - padY,
                                                  width, ink.getHeight() + 2.0f * padY);
                g.fillRoundedRectangle (box, h * 0.16f);
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

        const auto label = cellName (s.plugin, on ? juce::Colours::black : s.colour.withMultipliedBrightness (0.85f));
        const float h = 24.0f;
        const float w = drawStyledText (g, label, 0, 0, h, faces, false);
        drawStyledText (g, label, cell.getCentreX() - w * 0.5f, cell.getCentreY() + h * 0.36f, h, faces);
    }

    // Al centro: plugin e periferica
    if (active >= 0 && active < (int) setups.size())
    {
        const auto& s = setups[(size_t) active];
        const auto name = s.nameFace == TextPart::Face::logoBold
                            ? logoStyleName (s.plugin, s.colour)
                            : brandName (s.plugin, s.colour, s.nameFace);
        const float deviceH = 34.0f, baseline = 104.0f;
        // lo script sporge a destra per via dell'inclinazione: gli lascio piu' spazio
        const float gap = s.nameFace == TextPart::Face::script ? 46.0f : 30.0f;
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

    // In basso: logo MBSTUDIO91, firmware, chi cambia i setup
    juce::Font small (juce::FontOptions().withTypeface (faces.bebas).withHeight (22.0f));
    const float baseline = 150.0f;
    float footX = 12.0f;

    if (faces.logo.isValid())
    {
        // Il logo e' alto come le maiuscole del testo che lo segue
        juce::GlyphArrangement cap;
        cap.addLineOfText (small, "M", 0.0f, 0.0f);
        juce::Path capPath;
        cap.createPath (capPath);

        const int logoH = juce::roundToInt (capPath.getBounds().getHeight());
        const int logoW = juce::roundToInt ((float) logoH * (float) faces.logo.getWidth()
                                                          / (float) faces.logo.getHeight());

        g.setColour (juce::Colour (0xfff3f3f1).withAlpha (0.96f)); // stampato
        g.drawImageAt (shrinkMask (faces.logo, logoW, logoH), (int) footX,
                       juce::roundToInt (baseline) - logoH, true);
        footX += (float) logoW + 10.0f;
    }

    juce::String footer = "STAGE";
    if (firmware.isNotEmpty())
        footer << "   -   PUSH 2 FW " << firmware;
    g.setColour (juce::Colour (0xff9a9a9a));
    g.setFont (small);
    g.drawText (footer, (int) footX, 128, 600, 28, juce::Justification::centredLeft);

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
