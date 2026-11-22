#include "StationState.h"

namespace
{
const juce::Identifier stationsId ("POSTAZIONI"), stationId ("POSTAZIONE");
const juce::Identifier gainId ("volumeDb"), mutedId ("muta"), instrumentId ("strumento");
const juce::Identifier zoneTag ("ZONE");

juce::Identifier effectId (int slot)
{
    return juce::Identifier ("effetto" + juce::String (slot));
}
}

bool StationState::hasPlugins() const
{
    return ! allPluginIds().isEmpty();
}

juce::StringArray StationState::allPluginIds() const
{
    juce::StringArray ids;
    if (instrumentId.isNotEmpty())
        ids.add (instrumentId);
    for (const auto& e : effectIds)
        if (e.isNotEmpty())
            ids.add (e);
    return ids;
}

juce::ValueTree StationState::toValueTree() const
{
    juce::ValueTree t (stationId);
    t.setProperty (gainId, gainDb, nullptr);
    t.setProperty (mutedId, muted, nullptr);
    if (instrumentId.isNotEmpty())
        t.setProperty (::instrumentId, instrumentId, nullptr);
    for (int i = 0; i < maxEffects; ++i)
        if (effectIds[(size_t) i].isNotEmpty())
            t.setProperty (effectId (i), effectIds[(size_t) i], nullptr);
    t.appendChild (zone.toValueTree(), nullptr);
    return t;
}

StationState StationState::fromValueTree (const juce::ValueTree& t)
{
    StationState s;

    // Il vecchio file aveva la zona come figlio diretto di POSTAZIONI
    if (t.hasType (zoneTag))
    {
        s.zone = KeyZone::fromValueTree (t);
        return s;
    }

    s.gainDb = juce::jlimit (-60.0f, 12.0f, (float) t.getProperty (gainId, 0.0f));
    s.muted = (bool) t.getProperty (mutedId, false);
    s.instrumentId = t.getProperty (::instrumentId, juce::String()).toString();
    for (int i = 0; i < maxEffects; ++i)
        s.effectIds[(size_t) i] = t.getProperty (effectId (i), juce::String()).toString();
    s.zone = KeyZone::fromValueTree (t.getChildWithName (zoneTag));
    return s;
}

juce::ValueTree stationsToValueTree (const StationStates& states)
{
    juce::ValueTree tree (stationsId);
    for (const auto& s : states)
        tree.appendChild (s.toValueTree(), nullptr);
    return tree;
}

StationStates stationsFromValueTree (const juce::ValueTree& tree)
{
    StationStates states;
    for (int i = 0; i < StationState::numStations && i < tree.getNumChildren(); ++i)
        states[(size_t) i] = StationState::fromValueTree (tree.getChild (i));
    return states;
}
