#include "MidiPanic.h"

juce::MidiBuffer MidiPanic::makePanicBuffer (const NoteTracker& tracker, int samplePosition)
{
    juce::MidiBuffer out;

    for (int ch = 1; ch <= 16; ++ch)
        for (int note = 0; note < 128; ++note)
            if (tracker.isNoteHeld (ch, note))
                out.addEvent (juce::MidiMessage::noteOff (ch, note), samplePosition);

    for (int ch = 1; ch <= 16; ++ch)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), samplePosition);
        out.addEvent (juce::MidiMessage::allNotesOff (ch), samplePosition);
        out.addEvent (juce::MidiMessage::allSoundOff (ch), samplePosition);
    }

    return out;
}
