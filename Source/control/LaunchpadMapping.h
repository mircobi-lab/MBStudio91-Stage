#pragma once

#include "ControlSurface.h"
#include <vector>

// Protocollo "classico" della Launchpad Mini MK1 (layout X-Y, LED rosso/verde).
namespace LaunchpadMapping
{
    SurfaceAction decode (const juce::MidiMessage&);

    enum class Led { off, red, green, amber, redFlash };
    int velocityFor (Led);

    std::vector<juce::MidiMessage> ledMessages (const SurfaceState&, const SurfaceState* previous);
    std::vector<juce::MidiMessage> initMessages();
}
