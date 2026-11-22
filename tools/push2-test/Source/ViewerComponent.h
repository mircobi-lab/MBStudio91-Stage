#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FrameServer.h"

// Mostra il display del Push 2 ricevuto dalla rete, dentro un telaio in stile MBStudio.
class ViewerComponent final : public juce::Component, private juce::Timer
{
public:
    ViewerComponent();

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::CriticalSection lock;
    juce::Image frame;
    bool dirty = false;
    int shownFrames = -1;

    juce::Typeface::Ptr bebas;
    juce::Image logo;
    std::unique_ptr<FrameServer> server;
};
