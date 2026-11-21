#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Fase 1: solo lo schema di colori MBStudio91 (telaio nero, scritte bianche) e il Bebas Neue.
// L'interfaccia hardware completa arriva in Fase 4.
class StageLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StageLookAndFeel();

    static const juce::Colour panicRed;
    static const juce::Colour ledGreen;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    static juce::Font titleFont (float height);

private:
    juce::Typeface::Ptr bebas;
};
