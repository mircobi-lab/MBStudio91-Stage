#include "engine/NoteProcessor.h"

class NoteProcessorTests final : public juce::UnitTest
{
public:
    NoteProcessorTests() : juce::UnitTest ("NoteProcessor", "Stage") {}

    static std::vector<juce::MidiMessage> run (NoteProcessor& p, std::initializer_list<juce::MidiMessage> msgs,
                                               std::optional<float>* faderOut = nullptr)
    {
        juce::MidiBuffer in, out;
        int pos = 0;
        for (const auto& m : msgs)
            in.addEvent (m, pos++);
        std::optional<float> fader;
        p.process (in, out, fader);
        if (faderOut != nullptr)
            *faderOut = fader;
        std::vector<juce::MidiMessage> result;
        for (const auto meta : out)
            result.push_back (meta.getMessage());
        return result;
    }

    static juce::MidiMessage on (int note, int vel = 100)  { return juce::MidiMessage::noteOn (1, note, (juce::uint8) vel); }
    static juce::MidiMessage off (int note)                { return juce::MidiMessage::noteOff (1, note); }

    void runTest() override
    {
        beginTest ("curve di velocity");
        expectEquals (applyVelocityCurve (64, VelocityCurve::lineare, 1.0f), 64);
        expectEquals (applyVelocityCurve (64, VelocityCurve::morbida, 1.0f), 84);
        expectEquals (applyVelocityCurve (64, VelocityCurve::dura, 1.0f), 42);
        expectEquals (applyVelocityCurve (1, VelocityCurve::dura, 1.0f), 1);
        for (auto c : { VelocityCurve::lineare, VelocityCurve::morbida, VelocityCurve::dura, VelocityCurve::personalizzata })
            expectEquals (applyVelocityCurve (127, c, 2.5f), 127);
        expectEquals (applyVelocityCurve (64, VelocityCurve::personalizzata, 1.0f), 64);

        beginTest ("fader del volume");
        expectWithinAbsoluteError (faderValueToDb (127), 0.0f, 1.0e-4f);
        expectWithinAbsoluteError (faderValueToDb (64), -11.9f, 0.1f);
        expectWithinAbsoluteError (faderValueToDb (0), -60.0f, 1.0e-4f);

        beginTest ("trasposizione");
        {
            NoteProcessor p;
            KeyZone z; z.transpose = 12;
            p.setZone (z);
            auto r = run (p, { on (60) });
            expect (r.size() == 1 && r[0].isNoteOn() && r[0].getNoteNumber() == 72);
        }

        beginTest ("estensione tasti sul tasto fisico");
        {
            NoteProcessor p;
            KeyZone z; z.lowNote = 36; z.highNote = 59;
            p.setZone (z);
            expect (run (p, { on (60) }).empty());
            expect (run (p, { off (60) }).empty());
            auto r = run (p, { on (59) });
            expect (r.size() == 1 && r[0].getNoteNumber() == 59);
        }

        beginTest ("il note-off spegne la nota realmente suonata anche se la trasposizione cambia");
        {
            NoteProcessor p;
            KeyZone z; z.transpose = 12;
            p.setZone (z);
            run (p, { on (60) });
            z.transpose = 5;
            p.setZone (z);
            auto r = run (p, { off (60) });
            expect (r.size() == 1 && r[0].isNoteOff() && r[0].getNoteNumber() == 72);
        }

        beginTest ("note trasposte fuori 0-127 scartate");
        {
            NoteProcessor p;
            KeyZone z; z.transpose = 12;
            p.setZone (z);
            expect (run (p, { on (120) }).empty());
            expect (run (p, { off (120) }).empty());
        }

        beginTest ("velocity applicata al note-on");
        {
            NoteProcessor p;
            KeyZone z; z.curve = VelocityCurve::dura;
            p.setZone (z);
            auto r = run (p, { on (60, 64) });
            expect (r.size() == 1 && r[0].getVelocity() == 42);
        }

        beginTest ("gli altri messaggi passano");
        {
            NoteProcessor p;
            auto r = run (p, { juce::MidiMessage::controllerEvent (1, 1, 90), juce::MidiMessage::pitchWheel (1, 9000) });
            expect (r.size() == 2 && r[0].isController() && r[1].isPitchWheel());
        }

        beginTest ("CC7 come fader del volume");
        {
            NoteProcessor p;
            KeyZone z; z.faderControlsVolume = true;
            p.setZone (z);
            std::optional<float> fader;
            auto r = run (p, { juce::MidiMessage::controllerEvent (1, 7, 127) }, &fader);
            expect (r.empty());
            expect (fader.has_value() && std::abs (*fader) < 1.0e-4f);

            z.faderControlsVolume = false;
            p.setZone (z);
            r = run (p, { juce::MidiMessage::controllerEvent (1, 7, 127) }, &fader);
            expect (r.size() == 1 && r[0].getControllerNumber() == 7);
            expect (! fader.has_value());
        }

        beginTest ("reset dimentica le note tenute");
        {
            NoteProcessor p;
            run (p, { on (60) });
            p.reset();
            expect (run (p, { off (60) }).empty());
        }

        beginTest ("KeyZone in ValueTree e ritorno");
        {
            KeyZone z;
            z.transpose = -7; z.lowNote = 21; z.highNote = 96;
            z.curve = VelocityCurve::personalizzata; z.customGamma = 0.45f; z.faderControlsVolume = true;
            expect (KeyZone::fromValueTree (z.toValueTree()) == z);
            expect (KeyZone::fromValueTree ({}) == KeyZone {});
        }
    }
};

static NoteProcessorTests noteProcessorTests;
