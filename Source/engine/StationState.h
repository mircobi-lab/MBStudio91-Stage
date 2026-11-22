#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include "NoteProcessor.h"

// Quello che una postazione deve ritrovare al prossimo avvio: la zona di tastiera,
// volume e mute, e quali plugin erano caricati (per ID portabile).
struct StationState
{
    static constexpr int maxEffects = 3;
    static constexpr int numStations = 4;

    KeyZone zone;
    float gainDb = 0.0f;
    bool muted = false;
    juce::String instrumentId;                        // vuoto = nessuno strumento
    std::array<juce::String, maxEffects> effectIds {};

    bool hasPlugins() const;
    juce::StringArray allPluginIds() const;           // solo quelli non vuoti

    juce::ValueTree toValueTree() const;
    static StationState fromValueTree (const juce::ValueTree&);
};

using StationStates = std::array<StationState, StationState::numStations>;

juce::ValueTree stationsToValueTree (const StationStates&);

// Legge il file di oggi e anche quello vecchio, che conteneva solo le zone.
StationStates stationsFromValueTree (const juce::ValueTree&);
