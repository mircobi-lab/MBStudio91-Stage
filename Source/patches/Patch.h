#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <optional>
#include <utility>
#include "engine/NoteProcessor.h"

// Un plugin dentro una patch: ID portabile + stato salvato. portableId vuoto = slot vuoto.
struct PluginSlot
{
    juce::String portableId, name;
    juce::MemoryBlock state;
};

struct StationPatch
{
    PluginSlot instrument;
    std::array<PluginSlot, 3> effects;
    float gainDb = 0.0f;
    bool muted = false;
    KeyZone zone;
};

struct Patch
{
    juce::String name;
    std::array<StationPatch, 4> stations;

    juce::ValueTree toValueTree() const;
    static Patch fromValueTree (const juce::ValueTree&);
};

// 8 banchi (A-H) x 64 patch.
class Library
{
public:
    static constexpr int numBanks = 8, patchesPerBank = 64;

    const std::optional<Patch>& get (int bank, int index) const;
    void set (int bank, int index, Patch);
    void clear (int bank, int index);
    void rename (int bank, int index, const juce::String&);
    bool bankHasPatches (int bank) const;

    // La prossima patch salvata in avanti (+1) o indietro (-1), attraverso i banchi, ciclica.
    std::optional<std::pair<int, int>> neighbour (int bank, int index, int direction) const;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);
    bool saveTo (const juce::File&) const;
    bool loadFrom (const juce::File&);

    static juce::File dailyBackupFor (const juce::File& libraryFile, juce::Time);

private:
    static bool valid (int bank, int index);
    std::array<std::optional<Patch>, numBanks * patchesPerBank> patches;
};
