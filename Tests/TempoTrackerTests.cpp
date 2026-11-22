#include "engine/TempoTracker.h"
#include "engine/StageEngine.h"
#include "FakeInstrument.h"

class TempoTrackerTests final : public juce::UnitTest
{
public:
    TempoTrackerTests() : juce::UnitTest ("TempoTracker", "Stage") {}

    static void clocks (TempoTracker& t, int count, double start, double interval, double jitter = 0.0)
    {
        juce::Random r (42);
        for (int i = 0; i < count; ++i)
        {
            const double j = jitter > 0 ? (r.nextDouble() * 2.0 - 1.0) * jitter * interval : 0.0;
            t.handleClockMessage (juce::MidiMessage::midiClock(), start + i * interval + j);
        }
    }

    void runTest() override
    {
        beginTest ("BPM libero, limiti e trasporto sempre in riproduzione");
        {
            TempoTracker t;
            expectWithinAbsoluteError (t.getBpm(), 120.0, 1.0e-9);
            t.setBpm (98.0);
            expectWithinAbsoluteError (t.getBpm(), 98.0, 1.0e-9);
            t.setBpm (5.0);
            expectWithinAbsoluteError (t.getBpm(), 20.0, 1.0e-9);
            t.setBpm (999.0);
            expectWithinAbsoluteError (t.getBpm(), 300.0, 1.0e-9);
            expect (t.isPlaying());
        }

        beginTest ("tap tempo");
        {
            TempoTracker t;
            t.setBpm (90.0);
            for (int i = 0; i < 4; ++i) t.tap (10.0 + i * 0.5);
            expectWithinAbsoluteError (t.getBpm(), 120.0, 0.01);
            t.tap (13.5);   // pausa di 2 s: ricomincia, nessun salto
            expectWithinAbsoluteError (t.getBpm(), 120.0, 0.01);
            t.tap (13.9);
            expectWithinAbsoluteError (t.getBpm(), 150.0, 0.01);
        }

        beginTest ("MIDI clock esterno");
        {
            TempoTracker t;
            t.setSource (TempoTracker::Source::external);
            clocks (t, 25, 1.0, 1.0 / 48.0);
            expectWithinAbsoluteError (t.getBpm(), 120.0, 0.5);
        }

        beginTest ("MIDI clock con jitter del 10%");
        {
            TempoTracker t;
            t.setSource (TempoTracker::Source::external);
            clocks (t, 97, 1.0, 1.0 / 48.0, 0.1);
            expectWithinAbsoluteError (t.getBpm(), 120.0, 2.0);
        }

        beginTest ("Start, Stop, Continue e Song Position");
        {
            TempoTracker t;
            t.setSource (TempoTracker::Source::external);
            t.handleClockMessage (juce::MidiMessage::midiStop(), 1.0);
            expect (! t.isPlaying());
            t.handleClockMessage (juce::MidiMessage::songPositionPointer (16), 1.1);
            expectWithinAbsoluteError (t.position().getPpqPosition().orFallback (-1.0), 4.0, 1.0e-9);
            t.handleClockMessage (juce::MidiMessage::midiContinue(), 1.2);
            expect (t.isPlaying());
            t.handleClockMessage (juce::MidiMessage::midiStart(), 1.3);
            expect (t.isPlaying());
            expectWithinAbsoluteError (t.position().getPpqPosition().orFallback (-1.0), 0.0, 1.0e-9);
        }

        beginTest ("clock assente: BPM invariato e segnalato");
        {
            TempoTracker t;
            t.setSource (TempoTracker::Source::external);
            clocks (t, 25, 1.0, 1.0 / 48.0);
            const double before = t.getBpm();
            expect (! t.isClockLost (1.6));
            expect (t.isClockLost (4.0));
            expectWithinAbsoluteError (t.getBpm(), before, 1.0e-9);
        }

        beginTest ("la posizione avanza col tempo");
        {
            TempoTracker t;
            t.setBpm (120.0);
            const double p0 = t.position().getPpqPosition().orFallback (0.0);
            t.advance (48000, 48000.0);
            expectWithinAbsoluteError (t.position().getPpqPosition().orFallback (0.0) - p0, 2.0, 1.0e-6);
            expectWithinAbsoluteError (t.position().getBpm().orFallback (0.0), 120.0, 1.0e-9);
        }

        beginTest ("il motore passa il tempo ai plugin e legge il clock solo dall'ingresso scelto");
        {
            StageEngine e;
            e.prepare (48000.0, 256);
            auto f = std::make_unique<FakeInstrument>();
            auto* fp = f.get();
            e.station (0).setInstrument (std::move (f));
            expect (fp->getPlayHead() != nullptr);

            e.tempo().setSource (TempoTracker::Source::external);
            e.setClockSourceDevice ("drum");
            const double t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;
            for (int i = 0; i < 25; ++i)
            {
                auto m = juce::MidiMessage::midiClock();
                m.setTimeStamp (t0 + i / 40.0);   // 100 BPM
                e.routeIncoming ("drum", "Drum Machine", m);
                auto other = juce::MidiMessage::midiClock();
                other.setTimeStamp (t0 + i / 80.0);
                e.routeIncoming ("altro", "Altro", other);
            }
            expectWithinAbsoluteError (e.tempo().getBpm(), 100.0, 0.5);
        }
    }
};

static TempoTrackerTests tempoTrackerTests;
