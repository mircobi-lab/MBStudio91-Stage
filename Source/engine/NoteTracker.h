#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <bitset>

// Tiene traccia delle note e del pedale di sustain tenuti, per canale (1..16).
class NoteTracker
{
public:
    void process (const juce::MidiBuffer&);
    bool isNoteHeld (int channel1to16, int note) const;
    bool isSustainDown (int channel1to16) const;
    int heldNoteCount() const;
    void reset();

private:
    std::array<std::bitset<128>, 16> held {};
    std::bitset<16> sustain {};
};
