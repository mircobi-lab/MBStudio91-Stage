#include "engine/KeyboardRouter.h"
#include "settings/AppPaths.h"

class KeyboardRouterTests final : public juce::UnitTest
{
public:
    KeyboardRouterTests() : juce::UnitTest ("KeyboardRouter", "Stage") {}

    void runTest() override
    {
        beginTest ("assegnazione");
        {
            KeyboardRouter r;
            expectEquals (r.stationFor ("ks88"), -1);
            r.assign (0, "ks88", "Keystation 88 MK3");
            expectEquals (r.stationFor ("ks88"), 0);
            expect (r.assignmentFor (0).has_value() && r.assignmentFor (0)->deviceName == "Keystation 88 MK3");
        }

        beginTest ("una tastiera riassegnata lascia la postazione precedente");
        {
            KeyboardRouter r;
            r.assign (0, "ks88", "Keystation 88 MK3");
            r.assign (2, "ks88", "Keystation 88 MK3");
            expectEquals (r.stationFor ("ks88"), 2);
            expect (! r.assignmentFor (0).has_value());
        }

        beginTest ("una postazione riassegnata libera la tastiera precedente");
        {
            KeyboardRouter r;
            r.assign (2, "mini1", "Launchkey Mini");
            r.assign (2, "mini2", "Launchkey Mini MK2");
            expectEquals (r.stationFor ("mini1"), -1);
            expectEquals (r.stationFor ("mini2"), 2);
            r.clear (2);
            expectEquals (r.stationFor ("mini2"), -1);
        }

        beginTest ("apprendimento: premi un tasto");
        {
            KeyboardRouter r;
            expectEquals (r.learningStation(), -1);
            r.startLearn (1);
            expectEquals (r.learningStation(), 1);
            expectEquals (r.handleLearn ("lk61", "Launchkey 61", juce::MidiMessage::controllerEvent (1, 21, 10)), -1);
            expectEquals (r.handleLearn ("lk61", "Launchkey 61", juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90)), 1);
            expectEquals (r.learningStation(), -1);
            expectEquals (r.stationFor ("lk61"), 1);
            expectEquals (r.handleLearn ("x", "X", juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90)), -1);

            r.startLearn (3);
            r.cancelLearn();
            expectEquals (r.learningStation(), -1);
        }

        beginTest ("l'apprendimento scade dopo 10 secondi");
        {
            KeyboardRouter r;
            juce::uint32 fakeNow = 1000;
            r.clock = [&fakeNow] { return fakeNow; };
            r.startLearn (2);
            fakeNow += 9000;
            expectEquals (r.learningStation(), 2);
            fakeNow += 2000;
            expectEquals (r.learningStation(), -1);
            expectEquals (r.handleLearn ("x", "X", juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90)), -1);
            expectEquals (r.stationFor ("x"), -1);
        }

        beginTest ("suggerimento per nome");
        expectEquals (KeyboardRouter::suggestStationForDeviceName ("Keystation 88 MK3"), 0);
        expectEquals (KeyboardRouter::suggestStationForDeviceName ("Launchkey 61"), 1);
        expectEquals (KeyboardRouter::suggestStationForDeviceName ("Launchkey 61 LK 61 InControl"), -1);
        expectEquals (KeyboardRouter::suggestStationForDeviceName ("Launchkey 61 DAW"), -1);
        expectEquals (KeyboardRouter::suggestStationForDeviceName ("Launchkey Mini"), -1);
        expectEquals (KeyboardRouter::suggestStationForDeviceName ("Launchpad Mini"), -1);

        beginTest ("XML e ritorno");
        {
            KeyboardRouter a;
            a.assign (0, "ks88", "Keystation 88 MK3");
            a.assign (3, "mini1", "Launchkey Mini");
            KeyboardRouter b;
            b.fromXml (*a.toXml());
            expectEquals (b.stationFor ("ks88"), 0);
            expectEquals (b.stationFor ("mini1"), 3);
            expect (b.assignmentFor (3)->deviceName == "Launchkey Mini");
            expect (! b.assignmentFor (1).has_value());
        }

        beginTest ("l'intestazione dice che periferica e'");
        {
            using K = KeyboardRouter;
            auto label = [] (const char* n) { return K::kindLabel (K::kindForDeviceName (n)); };

            expectEquals (label ("Keystation 88 MK3"), juce::String ("KEYB"));
            expectEquals (label ("Launchkey 49 MK3 MIDI"), juce::String ("KEYB"));
            expectEquals (label ("Ableton Push 2 Live Port"), juce::String ("SURF"));
            expectEquals (label ("Launchpad Mini"), juce::String ("SURF"));
            expectEquals (label ("Scarlett 2i4 USB"), juce::String ("MIDI"));
            expectEquals (label (""), juce::String ("MIDI"));
        }

        beginTest ("nuovi file di impostazioni");
        expectEquals (AppPaths::keyboardsFile().getFileName(), juce::String ("tastiere.xml"));
        expectEquals (AppPaths::stationsFile().getFileName(), juce::String ("postazioni.xml"));
        expect (AppPaths::keyboardsFile().getParentDirectory() == AppPaths::dataDir());
    }
};

static KeyboardRouterTests keyboardRouterTests;
