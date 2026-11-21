#include "NoteTracker.h"

namespace
{
    bool validChannel (int ch) { return ch >= 1 && ch <= 16; }
}

void NoteTracker::process (const juce::MidiBuffer& buffer)
{
    for (const auto meta : buffer)
    {
        const auto m = meta.getMessage();
        const int ch = m.getChannel();
        if (! validChannel (ch))
            continue;

        auto& notes = held[(size_t) (ch - 1)];

        if (m.isNoteOn())                     // velocity 0 e' gia' un note-off per JUCE
            notes.set ((size_t) m.getNoteNumber());
        else if (m.isNoteOff())
            notes.reset ((size_t) m.getNoteNumber());
        else if (m.isSustainPedalOn())
            sustain.set ((size_t) (ch - 1));
        else if (m.isSustainPedalOff())
            sustain.reset ((size_t) (ch - 1));
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            notes.reset();
    }
}

bool NoteTracker::isNoteHeld (int ch, int note) const
{
    return validChannel (ch) && note >= 0 && note < 128 && held[(size_t) (ch - 1)].test ((size_t) note);
}

bool NoteTracker::isSustainDown (int ch) const
{
    return validChannel (ch) && sustain.test ((size_t) (ch - 1));
}

int NoteTracker::heldNoteCount() const
{
    int n = 0;
    for (const auto& c : held)
        n += (int) c.count();
    return n;
}

void NoteTracker::reset()
{
    for (auto& c : held)
        c.reset();
    sustain.reset();
}
