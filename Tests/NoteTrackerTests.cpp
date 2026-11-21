#include <juce_audio_basics/juce_audio_basics.h>
#include "engine/NoteTracker.h"

class NoteTrackerTests final : public juce::UnitTest
{
public:
    NoteTrackerTests() : juce::UnitTest ("NoteTracker", "Stage") {}

    static juce::MidiBuffer one (const juce::MidiMessage& m)
    {
        juce::MidiBuffer b;
        b.addEvent (m, 0);
        return b;
    }

    void runTest() override
    {
        beginTest ("note-on e note-off");
        NoteTracker t;
        t.process (one (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
        expect (t.isNoteHeld (1, 60));
        expectEquals (t.heldNoteCount(), 1);
        t.process (one (juce::MidiMessage::noteOff (1, 60)));
        expect (! t.isNoteHeld (1, 60));
        expectEquals (t.heldNoteCount(), 0);

        beginTest ("note-on con velocity 0 vale come note-off");
        t.process (one (juce::MidiMessage::noteOn (3, 64, (juce::uint8) 90)));
        expect (t.isNoteHeld (3, 64));
        t.process (one (juce::MidiMessage::noteOn (3, 64, (juce::uint8) 0)));
        expect (! t.isNoteHeld (3, 64));

        beginTest ("pedale di sustain (CC64)");
        t.process (one (juce::MidiMessage::controllerEvent (2, 64, 127)));
        expect (t.isSustainDown (2));
        expect (! t.isSustainDown (1));
        t.process (one (juce::MidiMessage::controllerEvent (2, 64, 0)));
        expect (! t.isSustainDown (2));

        beginTest ("CC123 azzera le note del canale");
        t.process (one (juce::MidiMessage::noteOn (5, 40, (juce::uint8) 80)));
        t.process (one (juce::MidiMessage::controllerEvent (5, 64, 127)));
        t.process (one (juce::MidiMessage::allNotesOff (5)));
        expect (! t.isNoteHeld (5, 40));

        beginTest ("reset");
        t.process (one (juce::MidiMessage::noteOn (16, 127, (juce::uint8) 1)));
        t.process (one (juce::MidiMessage::controllerEvent (16, 64, 100)));
        t.reset();
        expectEquals (t.heldNoteCount(), 0);
        expect (! t.isSustainDown (16));
    }
};

static NoteTrackerTests noteTrackerTests;
