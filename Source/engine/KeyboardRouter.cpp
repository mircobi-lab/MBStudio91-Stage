#include "KeyboardRouter.h"

namespace
{
    bool validStation (int s) { return juce::isPositiveAndBelow (s, KeyboardRouter::numStations); }
}

void KeyboardRouter::assign (int station, const juce::String& deviceId, const juce::String& deviceName)
{
    if (! validStation (station) || deviceId.isEmpty())
        return;

    const juce::ScopedLock sl (lock);
    for (auto& a : assignments)                       // una tastiera suona una sola postazione
        if (a.has_value() && a->deviceId == deviceId)
            a.reset();
    assignments[(size_t) station] = Assignment { deviceId, deviceName };
}

void KeyboardRouter::clear (int station)
{
    if (! validStation (station))
        return;
    const juce::ScopedLock sl (lock);
    assignments[(size_t) station].reset();
}

int KeyboardRouter::stationFor (const juce::String& deviceId) const
{
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < assignments.size(); ++i)
        if (assignments[i].has_value() && assignments[i]->deviceId == deviceId)
            return (int) i;
    return -1;
}

std::optional<KeyboardRouter::Assignment> KeyboardRouter::assignmentFor (int station) const
{
    if (! validStation (station))
        return std::nullopt;
    const juce::ScopedLock sl (lock);
    return assignments[(size_t) station];
}

void KeyboardRouter::startLearn (int station)
{
    const juce::ScopedLock sl (lock);
    learning = validStation (station) ? station : -1;
}

void KeyboardRouter::cancelLearn()
{
    const juce::ScopedLock sl (lock);
    learning = -1;
}

int KeyboardRouter::learningStation() const
{
    const juce::ScopedLock sl (lock);
    return learning;
}

int KeyboardRouter::handleLearn (const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage& m)
{
    int station = -1;
    {
        const juce::ScopedLock sl (lock);
        if (learning < 0 || ! m.isNoteOn())
            return -1;
        station = learning;
        learning = -1;
    }
    assign (station, deviceId, deviceName);
    return station;
}

int KeyboardRouter::suggestStationForDeviceName (const juce::String& name)
{
    if (name.containsIgnoreCase ("DAW") || name.containsIgnoreCase ("InControl"))
        return -1;
    if (name.containsIgnoreCase ("Keystation"))
        return 0;
    if (name.containsIgnoreCase ("Launchkey 61"))
        return 1;
    return -1;   // le due Launchkey Mini hanno nomi simili: si assegnano con ASSEGNA
}

std::unique_ptr<juce::XmlElement> KeyboardRouter::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("TASTIERE");
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < assignments.size(); ++i)
        if (assignments[i].has_value())
        {
            auto* e = xml->createNewChildElement ("TASTIERA");
            e->setAttribute ("postazione", (int) i);
            e->setAttribute ("id", assignments[i]->deviceId);
            e->setAttribute ("nome", assignments[i]->deviceName);
        }
    return xml;
}

void KeyboardRouter::fromXml (const juce::XmlElement& xml)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& a : assignments)
            a.reset();
    }
    for (const auto* e : xml.getChildWithTagNameIterator ("TASTIERA"))
        assign (e->getIntAttribute ("postazione", -1), e->getStringAttribute ("id"), e->getStringAttribute ("nome"));
}
