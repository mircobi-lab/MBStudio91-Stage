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
