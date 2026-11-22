#include "ViewerComponent.h"
#include "Push2Protocol.h"

#include "BinaryData.h"

namespace
{
const juce::Colour chassisTop { 0xff151617 }, chassisBottom { 0xff0a0a0b };
const juce::Colour recess { 0xff050506 }, recessEdge { 0xff26282b };
const juce::Colour print { 0xfff3f3f1 }, printDim { 0xff9b9da2 };
const juce::Colour ledGreen { 0xff3ee05c }, ledAmber { 0xffffb020 }, ledRed { 0xffff3b30 };

constexpr int scale = 1;
constexpr int margin = 26;
constexpr int barHeight = 34;
}

ViewerComponent::ViewerComponent()
{
    bebas = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf,
                                                     BinaryData::BebasNeueRegular_ttfSize);
    logo = juce::ImageCache::getFromMemory (BinaryData::mbstudio91wordmark_png,
                                            BinaryData::mbstudio91wordmark_pngSize);

    server = std::make_unique<FrameServer> ([this] (juce::Image img)
    {
        const juce::ScopedLock sl (lock);
        frame = std::move (img);
        dirty = true;
    });

    setSize (push2::displayWidth * scale + 2 * margin,
             push2::displayHeight * scale + 2 * margin + barHeight);
    startTimerHz (30);
}

void ViewerComponent::timerCallback()
{
    bool repaintNeeded = false;
    {
        const juce::ScopedLock sl (lock);
        repaintNeeded = dirty;
        dirty = false;
    }
    if (server->getFrameCount() != shownFrames)
    {
        shownFrames = server->getFrameCount();
        repaintNeeded = true;
    }
    if (repaintNeeded)
        repaint();
}

void ViewerComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setGradientFill ({ chassisTop, 0.0f, 0.0f, chassisBottom, 0.0f, area.getHeight(), false });
    g.fillAll();

    // Il display, incassato come sul pannello vero
    juce::Rectangle<int> screen (margin, margin, push2::displayWidth * scale, push2::displayHeight * scale);
    g.setColour (recess);
    g.fillRect (screen.expanded (6));
    g.setColour (recessEdge);
    g.drawRect (screen.expanded (6), 1);

    juce::Image shown;
    {
        const juce::ScopedLock sl (lock);
        shown = frame;
    }

    if (shown.isValid())
    {
        g.drawImage (shown, screen.toFloat());
    }
    else
    {
        g.setColour (juce::Colours::black);
        g.fillRect (screen);
        g.setColour (printDim);
        g.setFont (juce::Font (juce::FontOptions().withTypeface (bebas).withHeight (28.0f)));
        g.drawText (server->isListening() ? "IN ATTESA DI UN FRAME SULLA PORTA "
                                                + juce::String (server->getPort())
                                          : "PORTA NON DISPONIBILE",
                    screen, juce::Justification::centred);
    }

    // Barra di stato: LED, conteggio dei frame, marchio impresso
    auto bar = getLocalBounds().removeFromBottom (barHeight).reduced (margin, 0).toFloat();
    const bool client = server->hasClient();
    const auto led = ! server->isListening() ? ledRed : client ? ledGreen : ledAmber;

    g.setColour (led);
    g.fillEllipse (bar.getX(), bar.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (led.withAlpha (0.35f));
    g.fillEllipse (bar.getX() - 3.0f, bar.getCentreY() - 7.0f, 14.0f, 14.0f);

    juce::String text = ! server->isListening()
                            ? "PORTA " + juce::String (server->getPort()) + " OCCUPATA"
                            : client ? "COLLEGATO   " + juce::String (server->getFrameCount()) + " FRAME"
                                     : "IN ASCOLTO SU 127.0.0.1:" + juce::String (server->getPort());
    if (auto err = server->getLastError(); err.isNotEmpty())
        text << "   -   " << err.toUpperCase();

    g.setColour (printDim);
    g.setFont (juce::Font (juce::FontOptions().withTypeface (bebas).withHeight (20.0f)));
    g.drawText (text, bar.withTrimmedLeft (20.0f), juce::Justification::centredLeft);

    // marchio impresso in basso a destra, discreto
    if (logo.isValid())
    {
        const int h = 13;
        const int w = juce::roundToInt ((float) h * (float) logo.getWidth() / (float) logo.getHeight());
        juce::Rectangle<int> r ((int) bar.getRight() - w, (int) bar.getCentreY() - h / 2, w, h);
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.drawImage (logo, r.translated (0, -1).toFloat(), juce::RectanglePlacement::stretchToFit, true);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawImage (logo, r.translated (0, 1).toFloat(), juce::RectanglePlacement::stretchToFit, true);
        g.setColour (print.withAlpha (0.10f));
        g.drawImage (logo, r.toFloat(), juce::RectanglePlacement::stretchToFit, true);
    }
}
