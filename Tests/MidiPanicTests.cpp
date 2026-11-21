#include <juce_audio_basics/juce_audio_basics.h>
#include "engine/MidiPanic.h"
#include "engine/NoteTracker.h"

class MidiPanicTests final : public juce::UnitTest
{
public:
    MidiPanicTests() : juce::UnitTest ("MidiPanic", "Stage") {}

    void runTest() override
    {
        NoteTracker tracker;
        juce::MidiBuffer in;
        in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        tracker.process (in);

        const auto panic = MidiPanic::makePanicBuffer (tracker);

        std::vector<juce::MidiMessage> msgs;
        for (const auto meta : panic)
            msgs.push_back (meta.getMessage());

        beginTest ("note-off per ogni nota tenuta, prima dei CC");
        int firstCcIndex = -1, noteOffCount = 0, lastNoteOffIndex = -1;
        for (int i = 0; i < (int) msgs.size(); ++i)
        {
            if (msgs[(size_t) i].isNoteOff())
            {
                ++noteOffCount;
                lastNoteOffIndex = i;
                expectEquals (msgs[(size_t) i].getChannel(), 1);
                expect (msgs[(size_t) i].getNoteNumber() == 60 || msgs[(size_t) i].getNoteNumber() == 64);
            }
            else if (msgs[(size_t) i].isController() && firstCcIndex < 0)
            {
                firstCcIndex = i;
            }
        }
        expectEquals (noteOffCount, 2);
        expect (lastNoteOffIndex < firstCcIndex);

        beginTest ("CC64=0, CC123, CC120 su tutti i 16 canali, in quest'ordine");
        for (int ch = 1; ch <= 16; ++ch)
        {
            std::vector<int> ccs;
            for (const auto& m : msgs)
                if (m.isController() && m.getChannel() == ch)
                    ccs.push_back (m.getControllerNumber() * 1000 + m.getControllerValue());
            expect (ccs == std::vector<int> { 64000, 123000, 120000 }, "canale " + juce::String (ch));
        }

        beginTest ("dopo il panic nessuna nota e nessun sustain restano tenuti");
        tracker.process (panic);
        expectEquals (tracker.heldNoteCount(), 0);
        expect (! tracker.isSustainDown (1));

        beginTest ("posizione nel buffer");
        const auto later = MidiPanic::makePanicBuffer (NoteTracker {}, 37);
        for (const auto meta : later)
            expectEquals (meta.samplePosition, 37);
        expectEquals (later.getNumEvents(), 48);
    }
};

static MidiPanicTests midiPanicTests;
