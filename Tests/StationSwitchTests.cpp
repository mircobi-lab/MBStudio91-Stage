#include "engine/Station.h"
#include "FakeInstrument.h"

class StationSwitchTests final : public juce::UnitTest
{
public:
    StationSwitchTests() : juce::UnitTest ("StationSwitch", "Stage") {}

    static constexpr double sr = 48000.0;
    static constexpr int bs = 256;

    static juce::MidiBuffer msg (std::initializer_list<juce::MidiMessage> ms)
    {
        juce::MidiBuffer b;
        for (const auto& m : ms) b.addEvent (m, 0);
        return b;
    }
    static juce::MidiMessage on (int n)  { return juce::MidiMessage::noteOn (1, n, (juce::uint8) 100); }
    static juce::MidiMessage off (int n) { return juce::MidiMessage::noteOff (1, n); }

    // Rende per `seconds` secondi; restituisce l'ultimo campione.
    static float runFor (Station& s, double seconds, juce::MidiBuffer first = {})
    {
        juce::AudioBuffer<float> out (2, bs);
        const int blocks = juce::jmax (1, (int) std::ceil (seconds * sr / bs));
        for (int i = 0; i < blocks; ++i)
        {
            out.clear();
            s.renderAdd (out, i == 0 ? first : juce::MidiBuffer {});
        }
        return out.getSample (0, bs - 1);
    }

    static Station::Chain chainWith (std::unique_ptr<juce::AudioPluginInstance> inst)
    {
        Station::Chain c;
        c.instrument = std::move (inst);
        return c;
    }

    void runTest() override
    {
        beginTest ("la nota tenuta prima del cambio finisce sulla vecchia catena");
        {
            Station s; s.prepare (sr, bs);
            auto a = std::make_unique<FakeInstrument>(); auto* pa = a.get();
            auto b = std::make_unique<FakeInstrument>(); auto* pb = b.get();
            s.setInstrument (std::move (a));
            runFor (s, 0.01, msg ({ on (60) }));
            s.setChain (chainWith (std::move (b)));
            expect (s.getInstrument() == pb);
            expect (s.hasTailChain());
            runFor (s, 0.01, msg ({ off (60), on (62) }));
            expect (pa->receivedNoteOff (60));
            expect (pa->notes.empty());
            expect (pb->notes.count (62) == 1);
            expect (! pb->receivedNoteOff (60));
            for (const auto& m : pa->received)
                expect (! (m.isNoteOn() && m.getNoteNumber() == 62));
        }

        beginTest ("con il pedale giu' la coda non scade");
        {
            Station s; s.prepare (sr, bs);
            auto a = std::make_unique<RingingInstrument>();
            s.setInstrument (std::move (a));
            runFor (s, 0.01, msg ({ on (60), juce::MidiMessage::controllerEvent (1, 64, 127) }));
            s.setChain (chainWith (std::make_unique<FakeInstrument>()));
            runFor (s, 4.0, msg ({ off (60) }));
            expect (s.hasTailChain());
            expect (s.takeFinishedInstances().empty());
            runFor (s, 3.2, msg ({ juce::MidiMessage::controllerEvent (1, 64, 0) }));
            expectEquals ((int) s.takeFinishedInstances().size(), 1);
            expect (! s.hasTailChain());
        }

        beginTest ("la coda suona 3 secondi, poi sfuma e si libera");
        {
            Station s; s.prepare (sr, bs);
            s.setInstrument (std::make_unique<RingingInstrument>());
            runFor (s, 0.01, msg ({ on (60), off (60) }));
            s.setChain (chainWith (std::make_unique<FakeInstrument>()));
            expectWithinAbsoluteError (runFor (s, 2.9), 0.5f, 1.0e-3f);
            expectWithinAbsoluteError (runFor (s, 0.25), 0.0f, 1.0e-6f);
            auto finished = s.takeFinishedInstances();
            expectEquals ((int) finished.size(), 1);
            expect (finished.size() == 1 && finished[0]->getName() == "RingingInstrument");
        }

        beginTest ("un secondo cambio chiude la coda precedente in 20 ms");
        {
            Station s; s.prepare (sr, bs);
            s.setInstrument (std::make_unique<RingingInstrument>());
            runFor (s, 0.01, msg ({ on (60), off (60) }));
            s.setChain (chainWith (std::make_unique<RingingInstrument>()));
            runFor (s, 0.01, msg ({ on (64), off (64) }));
            s.setChain (chainWith (std::make_unique<FakeInstrument>()));
            runFor (s, 0.03);
            expectEquals ((int) s.takeFinishedInstances().size(), 1);
            expect (s.hasTailChain());
        }

        beginTest ("il panic silenzia catena attiva e coda");
        {
            Station s; s.prepare (sr, bs);
            s.setInstrument (std::make_unique<RingingInstrument>());
            runFor (s, 0.01, msg ({ on (60) }));
            s.setChain (chainWith (std::make_unique<RingingInstrument>()));
            runFor (s, 0.01, msg ({ on (62) }));
            s.requestPanic();
            runFor (s, 0.05);
            expectWithinAbsoluteError (runFor (s, 0.01), 0.0f, 1.0e-6f);
        }

        beginTest ("il MIDI arrivato mentre la postazione e' occupata non va perso");
        {
            Station s; s.prepare (sr, bs);
            auto a = std::make_unique<FakeInstrument>(); auto* pa = a.get();
            s.setInstrument (std::move (a));
            runFor (s, 0.01, msg ({ on (60) }));
            s.runWithLockHeldForTest ([&] { runFor (s, 0.001, msg ({ off (60) })); });
            runFor (s, 0.01);
            expect (pa->receivedNoteOff (60), "il note-off va consegnato al blocco successivo");
            expect (pa->notes.empty());
        }

        beginTest ("blocchi audio piu' grandi di 4096 campioni con una coda in sfumatura");
        {
            Station s; s.prepare (sr, bs);
            s.setInstrument (std::make_unique<RingingInstrument>());
            runFor (s, 0.01, msg ({ on (60), off (60) }));
            s.setChain (chainWith (std::make_unique<FakeInstrument>()));
            juce::AudioBuffer<float> big (2, 5000);
            for (int i = 0; i < 40; ++i)
            {
                big.clear();
                s.renderAdd (big, {});
            }
            expect (big.getMagnitude (0, 5000) < 1.0e-6f);
            expectEquals ((int) s.takeFinishedInstances().size(), 1);
        }

        beginTest ("mod wheel, espressione e aftertouch arrivano anche al suono in coda");
        {
            Station s; s.prepare (sr, bs);
            auto a = std::make_unique<FakeInstrument>(); auto* pa = a.get();
            auto b = std::make_unique<FakeInstrument>(); auto* pb = b.get();
            s.setInstrument (std::move (a));
            runFor (s, 0.01, msg ({ on (60) }));
            s.setChain (chainWith (std::move (b)));
            runFor (s, 0.01, msg ({ juce::MidiMessage::controllerEvent (1, 1, 90),
                                    juce::MidiMessage::controllerEvent (1, 11, 40),
                                    juce::MidiMessage::aftertouchChange (1, 60, 70),
                                    juce::MidiMessage::channelPressureChange (1, 50) }));
            expect (pa->receivedController (1));
            expect (pa->receivedController (11));
            bool polyOnOld = false, polyOnNew = false, pressureOnOld = false;
            for (const auto& m : pa->received) { polyOnOld |= m.isAftertouch(); pressureOnOld |= m.isChannelPressure(); }
            for (const auto& m : pb->received) polyOnNew |= m.isAftertouch();
            expect (polyOnOld && ! polyOnNew);
            expect (pressureOnOld);
            expect (pb->receivedController (1));
        }

        beginTest ("il pedale premuto dopo il cambio non tiene in vita una coda senza note");
        {
            Station s; s.prepare (sr, bs);
            auto a = std::make_unique<FakeInstrument>(); auto* pa = a.get();
            s.setInstrument (std::move (a));
            runFor (s, 0.01, msg ({ on (60), off (60) }));
            s.setChain (chainWith (std::make_unique<FakeInstrument>()));
            runFor (s, 0.01, msg ({ juce::MidiMessage::controllerEvent (1, 64, 127) }));
            expect (! pa->receivedController (64));
        }

        beginTest ("le istanze della nuova catena sono preparate");
        {
            Station s; s.prepare (sr, bs);
            auto b = std::make_unique<FakeInstrument>(); auto* pb = b.get();
            s.setChain (chainWith (std::move (b)));
            expectEquals (pb->lastSampleRate, sr);
        }
    }
};

static StationSwitchTests stationSwitchTests;
