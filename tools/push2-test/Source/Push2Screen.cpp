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
// Lucchetto minuscolo: corpo pieno e staffa ad arco
void drawPadlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    const float bodyH = r.getHeight() * 0.55f;
    juce::Rectangle<float> body (r.getX(), r.getBottom() - bodyH, r.getWidth(), bodyH);
    g.setColour (c);
    g.fillRoundedRectangle (body, r.getWidth() * 0.18f);

    juce::Path shackle;
    const float sw = r.getWidth() * 0.56f;
    shackle.addCentredArc (r.getCentreX(), body.getY(), sw * 0.5f, r.getHeight() * 0.3f,
                           0.0f, -juce::MathConstants<float>::halfPi,
                           juce::MathConstants<float>::halfPi, true);
    g.strokePath (shackle, juce::PathStrokeType (juce::jmax (1.0f, r.getWidth() * 0.18f)));
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

juce::Image renderScreen (const std::vector<ScreenSetup>& setups, const ScreenState& st,
                          const StyledText& setupChanger, const Faces& faces)
{
    juce::Image img (juce::Image::RGB, push2::displayWidth, push2::displayHeight, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colours::black);

    const juce::Colour dim { 0xff7a7a7a }, bright { 0xfff3f3f1 };

    // In alto: una casella per ogni rotativo e per il tasto sotto di esso.
    // Nome del setup, barra del volume, lucchetto se bloccato, barrato se in mute.
    const int cellWidth = push2::displayWidth / 8;
    for (int i = 0; i < (int) setups.size() && i < 8; ++i)
    {
        juce::Rectangle<float> cell ((float) (i * cellWidth + 4), 3.0f, (float) (cellWidth - 8), 36.0f);
        const auto& s = setups[(size_t) i];
        const bool on = (i == st.active);
        const bool hand = (i == st.touched);

        if (on)
        {
            g.setColour (s.colour.withMultipliedBrightness (s.muted ? 0.45f : 1.0f));
            g.fillRect (cell);
        }
        else
        {
            g.setColour (s.colour.withMultipliedBrightness (hand ? 1.0f : s.muted ? 0.3f : 0.6f));
            g.drawRect (cell, hand ? 2.0f : 1.0f);
        }

        auto ink = on ? juce::Colours::black : s.colour.withMultipliedBrightness (0.85f);
        if (s.muted)
            ink = ink.withAlpha (0.5f);

        const auto label = cellName (s.plugin, ink);
        const float h = 22.0f;
        const float w = drawStyledText (g, label, 0, 0, h, faces, false);
        const float lx = cell.getCentreX() - w * 0.5f;
        drawStyledText (g, label, lx, cell.getY() + 20.0f, h, faces);
        if (s.muted) // riga sopra il nome: muto
        {
            g.setColour (ink);
            g.fillRect (lx, cell.getY() + 13.0f, w, 1.5f);
        }

        const float lockW = 7.0f;
        juce::Rectangle<float> bar (cell.getX() + 5.0f, cell.getBottom() - 9.0f,
                                    cell.getWidth() - 10.0f - lockW - 4.0f, 5.0f);
        g.setColour (ink.withMultipliedAlpha (0.25f));
        g.fillRect (bar);
        g.setColour (ink);
        g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, s.volume)));

        if (s.locked)
            drawPadlock (g, { bar.getRight() + 4.0f, bar.getBottom() - 9.0f, lockW, 9.0f }, ink);
    }

    // Al centro: di norma plugin e periferica; tenendo un rotativo, quel volume
    const int middle = st.touched >= 0 && st.touched < (int) setups.size() ? st.touched : st.active;
    if (middle >= 0 && middle < (int) setups.size())
    {
        const auto& s = setups[(size_t) middle];
        const auto name = s.nameFace == TextPart::Face::logoBold
                            ? logoStyleName (s.plugin, s.colour)
                            : brandName (s.plugin, s.colour, s.nameFace);
        const float deviceH = 30.0f, baseline = 92.0f;
        const float gap = s.nameFace == TextPart::Face::script ? 42.0f : 28.0f;
        float nameH = 56.0f;

        StyledText right;
        if (st.touched >= 0)
        {
            right = { part (juce::String (juce::roundToInt (s.volume * 100.0f)) + "%", Style::bold) };
            if (s.locked)
                right.push_back (part ("BLOCCATO", Style::inverted, 0.3f, white, 0.5f));
        }
        else
        {
            right = s.device;
            if (s.transpose != 0)
                right.push_back (part ((s.transpose > 0 ? "+" : "") + juce::String (s.transpose) + " OTT",
                                       Style::inverted, 0.3f, white, 0.5f));
        }

        const float devW = right.empty() ? 0.0f : drawStyledText (g, right, 0, 0, deviceH, faces, false);
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
            g.fillRect (x + gap * 0.5f - 1.0f, baseline - 44.0f, 2.0f, 44.0f);
            drawStyledText (g, right, x + gap, baseline, deviceH, faces);
        }
    }

    // Riga di stato: quello che non appartiene a un singolo setup
    {
        StyledText row;
        auto label = [&] (const juce::String& t, float gap = 0.62f)
        { row.push_back (part (t, Style::regular, row.empty() ? 0.0f : gap, dim)); };
        auto value = [&] (const juce::String& t)
        { row.push_back (part (t, Style::bold, 0.16f, bright)); };
        auto badge = [&] (const juce::String& t)
        { row.push_back (part (t, Style::inverted, 0.62f, bright)); };

        label ("BPM");
        value (juce::String (st.bpm));
        if (st.metronome)
            row.push_back (part (st.beat ? "**" : "--", Style::bold, 0.2f,
                                 st.beat ? bright : dim.withAlpha (0.5f)));

        label ("MASTER");
        if (st.masterMuted)
            badge ("MUTO");
        else
            value (juce::String (juce::roundToInt (st.master * 100.0f)) + "%");

        label ("SCENA");
        value (st.scene >= 0 ? juce::String (st.scene + 1) : juce::String ("-"));

        label (st.stripMod ? "MOD" : "PITCH");
        const int sv = juce::roundToInt (st.strip * 127.0f);
        row.push_back (part (st.stripMod ? juce::String (sv)
                                         : (st.strip >= 0.5f ? "+" : "") + juce::String (juce::roundToInt ((st.strip - 0.5f) * 200.0f)),
                             Style::bold, 0.16f, st.stripTouched ? bright : dim));

        label (st.polyAftertouch ? "AT POLY" : "AT CH");
        value (juce::String (st.aftertouch));

        if (st.sustain)
            badge ("SUSTAIN");
        if (st.pedal2 > 0)
            badge (st.pedal2Continuous ? "PEDALE " + juce::String (juce::roundToInt (st.pedal2 * 100.0f / 127.0f)) + "%"
                                       : juce::String ("PEDALE 2"));
        if (st.shift)
            badge ("SHIFT");

        drawStyledText (g, row, 12.0f, 124.0f, 17.0f, faces);
    }

    // In basso: logo MBSTUDIO91, firmware, chi cambia i setup
    juce::Font small (juce::FontOptions().withTypeface (faces.bebas).withHeight (20.0f));
    const float baseline = 152.0f;
    float footX = 12.0f;

    if (faces.logo.isValid())
    {
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
    if (st.firmware.isNotEmpty())
        footer << "   -   PUSH 2 FW " << st.firmware;
    g.setColour (juce::Colour (0xff9a9a9a));
    g.setFont (small);
    g.drawText (footer, (int) footX, 134, 520, 24, juce::Justification::centredLeft);

    const float changerH = 20.0f;
    const float changerW = drawStyledText (g, setupChanger, 0, 0, changerH, faces, false);
    const float changerX = 948.0f - changerW;
    drawStyledText (g, setupChanger, changerX, baseline, changerH, faces);

    g.setColour (juce::Colour (0xff9a9a9a));
    g.setFont (small);
    g.drawText ("SETUP " + juce::String (st.active + 1) + "/" + juce::String ((int) setups.size()) + "  DA",
                (int) changerX - 200, 134, 190, 24, juce::Justification::centredRight);
    return img;
}
