#include "StageLookAndFeel.h"
#include "BinaryData.h"

const juce::Colour StageLookAndFeel::panicRed { 0xffd8261c };
const juce::Colour StageLookAndFeel::ledGreen { 0xff3ddc4a };

StageLookAndFeel::StageLookAndFeel()
    : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme {
          0xff000000,   // windowBackground
          0xff0b0b0b,   // widgetBackground
          0xff151515,   // menuBackground
          0xff5a5a5a,   // outline
          0xffffffff,   // defaultText
          0xff1e1e1e,   // defaultFill
          0xff000000,   // highlightedText
          0xffffffff,   // highlightedFill
          0xffffffff }) // menuText
{
    bebas = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf,
                                                     BinaryData::BebasNeueRegular_ttfSize);
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::white);
    setColour (juce::Slider::thumbColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8a1c14));
}

juce::Typeface::Ptr StageLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (bebas != nullptr)
        return bebas;
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font StageLookAndFeel::titleFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}

//==============================================================================
MBStudioBrand::MBStudioBrand()
{
    const auto mask = juce::ImageCache::getFromMemory (BinaryData::mbstudio91_wordmark_png,
                                                       BinaryData::mbstudio91_wordmark_pngSize);
    printed = tint (mask, juce::Colour (0xf3f3f1).withAlpha (0.96f));
    light   = tint (mask, juce::Colours::white.withAlpha (0.13f));
    shadow  = tint (mask, juce::Colours::black.withAlpha (0.90f));
    body    = tint (mask, juce::Colour (0xff09090a));
}

juce::Image MBStudioBrand::tint (const juce::Image& mask, juce::Colour c)
{
    if (! mask.isValid())
        return {};

    juce::Image out (juce::Image::ARGB, mask.getWidth(), mask.getHeight(), true);
    const juce::Image::BitmapData in (mask, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (out, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < in.height; ++y)
        for (int x = 0; x < in.width; ++x)
            dst.setPixelColour (x, y, c.withMultipliedAlpha (in.getPixelColour (x, y).getFloatAlpha()));

    return out;
}

int MBStudioBrand::widthFor (int height) const
{
    if (! printed.isValid() || printed.getHeight() == 0)
        return 0;
    return juce::roundToInt ((float) height * (float) printed.getWidth() / (float) printed.getHeight());
}

void MBStudioBrand::drawMark (juce::Graphics& g, const juce::Image& im,
                              juce::Rectangle<int> area, float opacity) const
{
    if (! im.isValid())
        return;
    g.setOpacity (opacity);
    g.drawImage (im, area.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    g.setOpacity (1.0f);
}

void MBStudioBrand::paintPrinted (juce::Graphics& g, juce::Rectangle<int> area,
                                  const juce::String& product) const
{
    const int h = juce::jmin (area.getHeight(), 30);
    auto row = area.withSizeKeepingCentre (area.getWidth(), h);
    auto markArea = row.removeFromLeft (widthFor (h));
    drawMark (g, printed, markArea, 1.0f);

    if (product.isEmpty())
        return;

    row.removeFromLeft (10);
    const auto font = StageLookAndFeel::titleFont ((float) h * 0.74f);
    const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, product)) + 18;
    auto badge = row.removeFromLeft (juce::jmin (w, row.getWidth())).reduced (0, 1);

    g.setColour (juce::Colour (0xff34363a));
    g.drawRoundedRectangle (badge.toFloat(), 2.0f, 1.0f);
    g.setColour (juce::Colour (0xfff3f3f1));
    g.setFont (font);
    g.drawText (product, badge, juce::Justification::centred);
}

void MBStudioBrand::paintDebossed (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int h = juce::jmin (area.getHeight(), 18);
    const int w = widthFor (h);
    auto markArea = juce::Rectangle<int> (area.getRight() - w, area.getCentreY() - h / 2, w, h);

    drawMark (g, light,  markArea.translated (0, 1), 1.0f);
    drawMark (g, shadow, markArea.translated (0, -1), 1.0f);
    drawMark (g, body,   markArea, 0.55f);
}
