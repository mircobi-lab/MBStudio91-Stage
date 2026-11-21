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

    static float render (StageEngine& e, const juce::MidiBuffer& midi, int blocks = 1)
    {
        juce::AudioBuffer<float> out (2, blockSize);
        for (int i = 0; i < blocks; ++i)
            e.renderBlock (out, i == 0 ? midi : juce::MidiBuffer {});
        return out.getSample (1, blockSize - 1);
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
        beginTest ("il MIDI delle tastiere va alla postazione 1");
        {
            StageEngine e;
            withTwoFakes (e);
            expectEquals (StageEngine::numStations, 4);
            expectWithinAbsoluteError (render (e, noteOn()), 0.5f, 1.0e-4f);
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
            e.renderBlock (out, {});
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
