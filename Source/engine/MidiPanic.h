#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "NoteTracker.h"

namespace MidiPanic
{
    // Note-off per ogni nota tenuta, poi su tutti i 16 canali: CC64=0, CC123, CC120.
    juce::MidiBuffer makePanicBuffer (const NoteTracker&, int samplePosition = 0);
}
