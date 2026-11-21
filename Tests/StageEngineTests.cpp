#include "engine/StageEngine.h"
#include "FakeInstrument.h"

class StageEngineTests final : public juce::UnitTest
{
public:
    StageEngineTests() : juce::UnitTest ("StageEngine", "Stage") {}

    static constexpr int blockSize = 256;

    static juce::MidiBuffer noteOn()
    {
        juce::MidiBuffer b;
        b.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        return b;
    }

    // Inietta `midi` nella postazione 1 (indice 0) e rende `blocks` blocchi.
    static float render (StageEngine& e, const juce::MidiBuffer& midi, int blocks = 1)
    {
        if (! midi.isEmpty())
            e.injectMidiForTest (0, midi);
        return renderOnly (e, blocks);
    }

    static float renderOnly (StageEngine& e, int blocks = 1)
    {
        juce::AudioBuffer<float> out (2, blockSize);
        for (int i = 0; i < blocks; ++i)
            e.renderBlock (out);
        return out.getSample (1, blockSize - 1);
    }

    static juce::MidiMessage now (juce::MidiMessage m)
    {
        m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
        return m;
    }

    static FakeInstrument* fakeOn (StageEngine& e, int station)
    {
        auto f = std::make_unique<FakeInstrument>();
        auto* p = f.get();
        e.station (station).setInstrument (std::move (f));
        return p;
    }

    static StageEngine& withTwoFakes (StageEngine& e)
    {
        e.prepare (48000.0, blockSize);
        e.station (0).setInstrument (std::make_unique<FakeInstrument>());
        e.station (1).setInstrument (std::make_unique<FakeInstrument>());
        return e;
    }

    void runTest() override
    {
        beginTest ("una tastiera assegnata suona solo la sua postazione");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            expectEquals (StageEngine::numStations, 4);
            auto* f0 = fakeOn (e, 0);
            auto* f2 = fakeOn (e, 2);
            e.keyboards().assign (2, "lk-mini2", "Launchkey Mini MK2");
            e.routeIncoming ("lk-mini2", "Launchkey Mini MK2", now (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
            expectWithinAbsoluteError (renderOnly (e), 0.5f, 1.0e-4f);
            expect (f2->notes.count (60) == 1);
            expect (f0->notes.empty());
            expect (e.lastActivityMs (2) > 0);
        }

        beginTest ("un dispositivo non assegnato non suona nulla");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            fakeOn (e, 0);
            e.routeIncoming ("ignoto", "Launchkey 61 DAW", now (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
            expectWithinAbsoluteError (renderOnly (e), 0.0f, 1.0e-6f);
        }

        beginTest ("la zona della postazione trasforma le note");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            auto* f = fakeOn (e, 0);
            KeyZone z; z.transpose = 12;
            e.setZone (0, z);
            expect (e.getZone (0) == z);
            e.keyboards().assign (0, "ks88", "Keystation 88 MK3");
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
            renderOnly (e);
            expect (f->notes.count (72) == 1);
        }

        beginTest ("il fader della Keystation regola il volume della postazione 1");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            auto* f = fakeOn (e, 0);
            expect (e.getZone (0).faderControlsVolume);
            expect (! e.getZone (1).faderControlsVolume);
            e.keyboards().assign (0, "ks88", "Keystation 88 MK3");
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::controllerEvent (1, 7, 64)));
            renderOnly (e);
            expectWithinAbsoluteError (e.station (0).getGainDb(), -11.9f, 0.1f);
            expect (! f->receivedController (7));
        }

        beginTest ("apprendimento: il primo tasto assegna la tastiera e non suona");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            auto* f = fakeOn (e, 3);
            int finished = -1;
            e.onLearnFinished = [&finished] (int s) { finished = s; };
            e.keyboards().startLearn (3);
            e.routeIncoming ("dev-x", "Launchkey Mini", now (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
            expectEquals (e.keyboards().stationFor ("dev-x"), 3);
            renderOnly (e);
            expect (f->notes.empty());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            expectEquals (finished, 3);
        }

        beginTest ("riassegnare una tastiera che sta suonando libera la vecchia postazione");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            auto* f0 = fakeOn (e, 0);
            fakeOn (e, 2);
            e.keyboards().assign (0, "ks88", "Keystation 88 MK3");
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::controllerEvent (1, 64, 127)));
            renderOnly (e);
            expect (f0->notes.count (60) == 1);
            e.keyboards().startLearn (2);
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100)));
            renderOnly (e);
            expect (f0->notes.empty(), "la postazione 1 non deve restare con note appese");
        }

        beginTest ("una tastiera scollegata libera la sua postazione");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            auto* f = fakeOn (e, 1);
            e.keyboards().assign (1, "lk61", "Launchkey 61");
            e.routeIncoming ("lk61", "Launchkey 61", now (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100)));
            renderOnly (e);
            expect (f->notes.count (64) == 1);
            e.deviceDisconnected ("lk61");
            renderOnly (e);
            expect (f->notes.empty());
            expectEquals (e.keyboards().stationFor ("lk61"), 1);   // l'assegnazione resta per quando torna
        }

        beginTest ("PANIC annulla l'apprendimento in corso");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            e.keyboards().startLearn (3);
            e.panicAll();
            expectEquals (e.keyboards().learningStation(), -1);
        }

        beginTest ("PANIC dopo un cambio di trasposizione con nota tenuta");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            auto* f = fakeOn (e, 0);
            e.keyboards().assign (0, "ks88", "Keystation 88 MK3");
            KeyZone z; z.transpose = 7;
            e.setZone (0, z);
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)));
            renderOnly (e);
            z.transpose = -5;
            e.setZone (0, z);
            e.panicAll();
            renderOnly (e);
            expect (f->notes.empty());
            expectWithinAbsoluteError (renderOnly (e), 0.0f, 1.0e-6f);
            // dopo il panic il note-off del tasto non deve produrre note fantasma
            e.routeIncoming ("ks88", "Keystation 88 MK3", now (juce::MidiMessage::noteOff (1, 60)));
            renderOnly (e);
            expect (! f->receivedNoteOff (55));
        }

        beginTest ("le postazioni si sommano");
        {
            StageEngine e;
            withTwoFakes (e);
            e.injectMidiForTest (1, noteOn());
            expectWithinAbsoluteError (render (e, noteOn()), 1.0f, 1.0e-4f);
        }

        beginTest ("il buffer d'uscita viene azzerato prima del mix");
        {
            StageEngine e;
            e.prepare (48000.0, blockSize);
            juce::AudioBuffer<float> out (2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (out.getWritePointer (ch), 0.7f, blockSize);
            e.renderBlock (out);
            expectWithinAbsoluteError (out.getMagnitude (0, blockSize), 0.0f, 1.0e-6f);
        }

        beginTest ("volume master");
        {
            StageEngine e;
            withTwoFakes (e);
            e.setMasterGainDb (-6.0206f);
            expectWithinAbsoluteError (render (e, noteOn(), 10), 0.25f, 1.0e-3f);
        }

        beginTest ("panicAll ferma tutte le postazioni");
        {
            StageEngine e;
            withTwoFakes (e);
            e.injectMidiForTest (1, noteOn());
            render (e, noteOn());
            e.panicAll();
            render (e, {});
            expectWithinAbsoluteError (render (e, {}), 0.0f, 1.0e-6f);
        }

        beginTest ("ingressi MIDI: chi sparisce si chiude, chi c'e' si apre");
        {
            const juce::StringArray enabled { "keystation", "launchkey61", "minimk1" };
            juce::Array<juce::MidiDeviceInfo> available;
            available.add ({ "Keystation 88 MK3", "keystation" });
            available.add ({ "Launchkey Mini MK2", "minimk2" });
            available.add ({ "Launchpad Mini", "launchpad" });

            const auto changes = StageEngine::planMidiInputChanges (enabled, available);
            expect (changes.toDisable == juce::StringArray { "launchkey61", "minimk1" });
            expect (changes.toOpen == juce::StringArray { "keystation", "minimk2" });
        }

        beginTest ("i dispositivi Launchpad non suonano le postazioni");
        {
            expect (StageEngine::isReservedControlDevice ("Launchpad Mini"));
            expect (StageEngine::isReservedControlDevice ("Launchpad Mini 4"));
            expect (! StageEngine::isReservedControlDevice ("Launchkey 61"));
            expect (! StageEngine::isReservedControlDevice ("Keystation 88 MK3"));
        }
    }
};

static StageEngineTests stageEngineTests;
