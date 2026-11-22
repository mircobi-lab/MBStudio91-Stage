#include "TempoTracker.h"
#include <cmath>

namespace
{
    constexpr double minBpm = 20.0, maxBpm = 300.0;
    constexpr int clocksPerQuarter = 24;
    constexpr double maxClockGapSeconds = 0.5;   // oltre: il clock e' ripartito, la media ricomincia
    constexpr int tapsToAverage = 4;
}

void TempoTracker::setSource (Source s)       { source = s; }
TempoTracker::Source TempoTracker::getSource() const { return source; }

void TempoTracker::setBpm (double bpm)
{
    internalBpm = juce::jlimit (minBpm, maxBpm, bpm);
}

void TempoTracker::tap (double now)
{
    const juce::ScopedLock sl (lock);
    if (! taps.isEmpty() && now - taps.getLast() >= tapResetSeconds)
        taps.clear();   // pausa lunga: si ricomincia senza cambiare il tempo
    taps.add (now);
    while (taps.size() > tapsToAverage + 1)
        taps.remove (0);

    if (taps.size() >= 2)
    {
        const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
        if (avg > 0.0)
            setBpm (60.0 / avg);
    }
}

void TempoTracker::handleClockMessage (const juce::MidiMessage& m, double now)
{
    if (m.isMidiStart())
    {
        ppq = 0.0;
        externalPlaying = true;
        return;
    }
    if (m.isMidiStop())     { externalPlaying = false; return; }
    if (m.isMidiContinue()) { externalPlaying = true; return; }
    if (m.isSongPositionPointer())
    {
        ppq = m.getSongPositionPointerMidiBeat() / 4.0;   // la posizione e' in sedicesimi
        return;
    }
    if (! m.isMidiClock())
        return;

    const juce::ScopedLock sl (lock);
    if (clockCount > 0 && now - clockTimes[(size_t) clockCount - 1] > maxClockGapSeconds)
        clockCount = 0;

    if (clockCount == (int) clockTimes.size())
    {
        std::move (clockTimes.begin() + 1, clockTimes.end(), clockTimes.begin());
        --clockCount;
    }
    clockTimes[(size_t) clockCount++] = now;
    lastClock = now;

    if (clockCount >= 3)
    {
        // Media su tutta la finestra: il jitter dei singoli impulsi si compensa.
        const double interval = (clockTimes[(size_t) clockCount - 1] - clockTimes[0]) / (clockCount - 1);
        if (interval > 0.0)
            externalBpm = juce::jlimit (minBpm, maxBpm, 60.0 / (interval * clocksPerQuarter));
    }
}

double TempoTracker::getBpm() const
{
    return source == Source::external ? externalBpm.load() : internalBpm.load();
}

bool TempoTracker::isPlaying() const
{
    return source == Source::internal || externalPlaying.load();
}

bool TempoTracker::isClockLost (double now) const
{
    const double last = lastClock.load();
    return last <= 0.0 || now - last > clockLostSeconds;
}

void TempoTracker::advance (int numSamples, double sampleRate)
{
    if (sampleRate <= 0.0 || ! isPlaying())
        return;
    ppq = ppq.load() + numSamples / sampleRate * getBpm() / 60.0;
}

juce::AudioPlayHead::PositionInfo TempoTracker::position() const
{
    juce::AudioPlayHead::PositionInfo p;
    const double q = ppq.load();
    p.setBpm (getBpm());
    p.setPpqPosition (q);
    p.setPpqPositionOfLastBarStart (std::floor (q / 4.0) * 4.0);
    p.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
    p.setIsPlaying (isPlaying());
    return p;
}
