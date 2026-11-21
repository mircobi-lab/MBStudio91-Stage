#include "engine/Station.h"
#include "FakeInstrument.h"

class StationTests final : public juce::UnitTest
{
public:
    StationTests() : juce::UnitTest ("Station", "Stage") {}

    static constexpr int blockSize = 256;

    static juce::MidiBuffer noteOn (int note = 60)
    {
        juce::MidiBuffer b;
        b.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        return b;
    }

    // Esegue n blocchi e restituisce l'ultimo campione del canale 0 dell'ultimo blocco.
    static float run (Station& s, juce::MidiBuffer first, int blocks = 1, float preset = 0.0f)
    {
        juce::AudioBuffer<float> out (2, blockSize);
        for (int i = 0; i < blocks; ++i)
        {
            out.clear();
            if (std::abs (preset) > 0.0f)
                for (int ch = 0; ch < 2; ++ch)
                    juce::FloatVectorOperations::fill (out.getWritePointer (ch), preset, blockSize);
            s.renderAdd (out, i == 0 ? first : juce::MidiBuffer {});
        }
        return out.getSample (0, blockSize - 1);
    }

    void runTest() override
    {
        beginTest ("senza strumento il buffer resta invariato");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            expectWithinAbsoluteError (run (s, noteOn(), 1, 0.3f), 0.3f, 1.0e-6f);
        }

        beginTest ("lo strumento suona e si somma al buffer");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            s.setInstrument (std::make_unique<FakeInstrument>());
            expectWithinAbsoluteError (run (s, noteOn()), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (run (s, noteOn(), 1, 0.25f), 0.75f, 1.0e-4f);
        }

        beginTest ("effetto nello slot 0; slot 3 rifiutato");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            s.setInstrument (std::make_unique<FakeInstrument>());
            expect (s.setEffect (0, std::make_unique<FakeGainEffect> (0.5f)));
            expect (! s.setEffect (3, std::make_unique<FakeGainEffect> (0.5f)));
            expect (s.getEffect (0) != nullptr);
            expectWithinAbsoluteError (run (s, noteOn()), 0.25f, 1.0e-4f);
        }

        beginTest ("mute porta l'uscita a zero con una rampa");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            s.setInstrument (std::make_unique<FakeInstrument>());
            run (s, noteOn());
            s.setMuted (true);
            expect (s.isMuted());
            expectWithinAbsoluteError (run (s, {}, 10), 0.0f, 1.0e-4f);
        }

        beginTest ("volume in dB");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            s.setInstrument (std::make_unique<FakeInstrument>());
            s.setGainDb (-6.0206f);
            expectWithinAbsoluteError (run (s, noteOn(), 10), 0.25f, 1.0e-3f);
        }

        beginTest ("panic con nota e sustain tenuti");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            auto fake = std::make_unique<FakeInstrument>();
            auto* f = fake.get();
            s.setInstrument (std::move (fake));
            juce::MidiBuffer held = noteOn (60);
            held.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            run (s, held);
            s.requestPanic();
            run (s, {});
            expect (f->receivedNoteOff (60));
            expect (f->receivedController (123));
            expect (f->receivedController (120));
            expectWithinAbsoluteError (run (s, {}), 0.0f, 1.0e-6f);
        }

        beginTest ("strumento non caricato: niente suono e messaggio di errore");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            s.setInstrument (nullptr);
            s.setLoadError ("File non trovato");
            expectEquals (s.getLoadError(), juce::String ("File non trovato"));
            expectWithinAbsoluteError (run (s, noteOn()), 0.0f, 1.0e-6f);
        }

        beginTest ("cambio di frequenza di campionamento ripara il plugin");
        {
            Station s;
            s.prepare (44100.0, 512);
            auto fake = std::make_unique<FakeInstrument>();
            auto* f = fake.get();
            s.setInstrument (std::move (fake));
            expectEquals (f->lastSampleRate, 44100.0);
            s.prepare (48000.0, blockSize);
            expectEquals (f->lastSampleRate, 48000.0);
            expectWithinAbsoluteError (run (s, noteOn()), 0.5f, 1.0e-4f);
        }

        beginTest ("sostituire lo strumento avvisa prima di rimuovere il vecchio");
        {
            Station s;
            s.prepare (48000.0, blockSize);
            auto first = std::make_unique<FakeInstrument>();
            auto* firstPtr = first.get();
            s.setInstrument (std::move (first));
            juce::AudioPluginInstance* removed = nullptr;
            s.onBeforeInstanceRemoved = [&] (juce::AudioPluginInstance& p) { removed = &p; };
            s.setInstrument (std::make_unique<FakeInstrument>());
            expect (removed == firstPtr);
            expect (s.getInstrument() != nullptr && s.getInstrument() != firstPtr);
        }
    }
};

static StationTests stationTests;
