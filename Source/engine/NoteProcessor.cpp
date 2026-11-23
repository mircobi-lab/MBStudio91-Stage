#include "NoteProcessor.h"
#include <cmath>

namespace
{
    const juce::Identifier zoneId ("ZONE"), transposeId ("transpose"), lowId ("lowNote"), highId ("highNote"),
                           curveId ("curve"), gammaId ("customGamma"), faderId ("faderControlsVolume"),
                           lockId ("volumeLocked");

    float gammaFor (VelocityCurve c, float custom)
    {
        switch (c)
        {
            case VelocityCurve::morbida:        return 0.6f;
            case VelocityCurve::dura:           return 1.6f;
            case VelocityCurve::personalizzata: return juce::jlimit (0.3f, 3.0f, custom);
            case VelocityCurve::lineare:        break;
        }
        return 1.0f;
    }
}

//==============================================================================
juce::ValueTree KeyZone::toValueTree() const
{
    juce::ValueTree t (zoneId);
    t.setProperty (transposeId, transpose, nullptr);
    t.setProperty (lowId, lowNote, nullptr);
    t.setProperty (highId, highNote, nullptr);
    t.setProperty (curveId, (int) curve, nullptr);
    t.setProperty (gammaId, customGamma, nullptr);
    t.setProperty (faderId, faderControlsVolume, nullptr);
    t.setProperty (lockId, volumeLocked, nullptr);
    return t;
}

KeyZone KeyZone::fromValueTree (const juce::ValueTree& t)
{
    KeyZone z;
    if (! t.isValid())
        return z;
    z.transpose = juce::jlimit (-48, 48, (int) t.getProperty (transposeId, 0));
    z.lowNote   = juce::jlimit (0, 127, (int) t.getProperty (lowId, 0));
    z.highNote  = juce::jlimit (0, 127, (int) t.getProperty (highId, 127));
    z.curve     = (VelocityCurve) juce::jlimit (0, 3, (int) t.getProperty (curveId, 0));
    z.customGamma = juce::jlimit (0.3f, 3.0f, (float) t.getProperty (gammaId, 1.0f));
    z.faderControlsVolume = (bool) t.getProperty (faderId, false);
    z.volumeLocked = (bool) t.getProperty (lockId, false);
    return z;
}

bool KeyZone::operator== (const KeyZone& o) const
{
    return transpose == o.transpose && lowNote == o.lowNote && highNote == o.highNote
        && curve == o.curve && juce::approximatelyEqual (customGamma, o.customGamma)
        && faderControlsVolume == o.faderControlsVolume
        && volumeLocked == o.volumeLocked;
}

int applyVelocityCurve (int v, VelocityCurve c, float customGamma)
{
    v = juce::jlimit (1, 127, v);
    const double shaped = 127.0 * std::pow (v / 127.0, (double) gammaFor (c, customGamma));
    return juce::jlimit (1, 127, (int) std::lround (shaped));
}

float faderValueToDb (int value)
{
    if (value <= 0)
        return -60.0f;
    return juce::jmax (-60.0f, 40.0f * std::log10 ((float) juce::jmin (value, 127) / 127.0f));
}

//==============================================================================
NoteProcessor::NoteProcessor() { reset(); }

void NoteProcessor::setZone (const KeyZone& z)
{
    const juce::SpinLock::ScopedLockType sl (zoneLock);
    zone = z;
}

KeyZone NoteProcessor::getZone() const
{
    const juce::SpinLock::ScopedLockType sl (zoneLock);
    return zone;
}

void NoteProcessor::reset()
{
    for (auto& ch : playedNote)
        ch.fill (-1);
}

void NoteProcessor::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, std::optional<float>& faderDb)
{
    {
        const juce::SpinLock::ScopedTryLockType sl (zoneLock);
        if (sl.isLocked())
            audioZone = zone;   // se il lock e' occupato si usa la copia del blocco precedente
    }
    const auto& z = audioZone;
    faderDb.reset();

    for (const auto meta : in)
    {
        auto m = meta.getMessage();
        const int ch = m.getChannel();

        if (ch < 1 || ch > 16)
        {
            out.addEvent (m, meta.samplePosition);
            continue;
        }

        auto& map = playedNote[(size_t) (ch - 1)];

        if (m.isNoteOn())
        {
            const int key = m.getNoteNumber();
            const int played = key + z.transpose;
            if (key < z.lowNote || key > z.highNote || played < 0 || played > 127)
                continue;
            map[(size_t) key] = (juce::int8) played;
            out.addEvent (juce::MidiMessage::noteOn (ch, played,
                              (juce::uint8) applyVelocityCurve (m.getVelocity(), z.curve, z.customGamma)),
                          meta.samplePosition);
        }
        else if (m.isNoteOff())
        {
            const int key = m.getNoteNumber();
            const int played = map[(size_t) key];
            if (played < 0)
                continue;
            map[(size_t) key] = -1;
            out.addEvent (juce::MidiMessage::noteOff (ch, played, m.getVelocity()), meta.samplePosition);
        }
        else if (m.isAftertouch())
        {
            const int played = map[(size_t) m.getNoteNumber()];
            if (played >= 0)
                out.addEvent (juce::MidiMessage::aftertouchChange (ch, played, m.getAfterTouchValue()), meta.samplePosition);
        }
        else if (z.faderControlsVolume && ! z.volumeLocked && m.isControllerOfType (7))
        {
            faderDb = faderValueToDb (m.getControllerValue());
        }
        else
        {
            out.addEvent (m, meta.samplePosition);
        }
    }
}
