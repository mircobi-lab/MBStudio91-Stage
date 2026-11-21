#include "patches/Patch.h"

class PatchTests final : public juce::UnitTest
{
public:
    PatchTests() : juce::UnitTest ("Patch", "Stage") {}

    static Patch samplePatch (const juce::String& name)
    {
        Patch p;
        p.name = name;
        auto& s = p.stations[1];
        s.instrument.portableId = "VST3|LennarDigital|Sylenth1|4c4e5331";
        s.instrument.name = "Sylenth1";
        const unsigned char bytes[] = { 0x00, 0xff, 0x10, 0x00, 0x7f };
        s.instrument.state.append (bytes, sizeof (bytes));
        s.effects[2].portableId = "VST3|X|Delay|1";
        s.effects[2].name = "Delay";
        s.gainDb = -4.5f;
        s.muted = true;
        s.zone.transpose = 12;
        s.zone.curve = VelocityCurve::morbida;
        return p;
    }

    void runTest() override
    {
        beginTest ("patch in ValueTree e ritorno");
        {
            const auto p = samplePatch ("Fusion Rhodes");
            const auto q = Patch::fromValueTree (p.toValueTree());
            expectEquals (q.name, juce::String ("Fusion Rhodes"));
            const auto& s = q.stations[1];
            expectEquals (s.instrument.portableId, juce::String ("VST3|LennarDigital|Sylenth1|4c4e5331"));
            expect (s.instrument.state == p.stations[1].instrument.state);
            expectEquals (s.effects[2].name, juce::String ("Delay"));
            expect (s.effects[0].portableId.isEmpty());
            expectWithinAbsoluteError (s.gainDb, -4.5f, 1.0e-4f);
            expect (s.muted);
            expect (s.zone == p.stations[1].zone);
            expect (q.stations[0].instrument.portableId.isEmpty());
        }

        beginTest ("libreria: set, get, clear, rename");
        {
            Library lib;
            expect (! lib.get (0, 0).has_value());
            lib.set (2, 10, samplePatch ("Uno"));
            expect (lib.get (2, 10).has_value());
            expect (lib.bankHasPatches (2));
            expect (! lib.bankHasPatches (3));
            lib.rename (2, 10, "Due");
            expectEquals (lib.get (2, 10)->name, juce::String ("Due"));
            lib.clear (2, 10);
            expect (! lib.get (2, 10).has_value());
            expect (! lib.get (99, 99).has_value());   // fuori range: vuoto, nessun crash
        }

        beginTest ("patch vicine: attraverso i banchi e ciclico");
        {
            Library lib;
            expect (! lib.neighbour (0, 0, 1).has_value());
            lib.set (0, 5, samplePatch ("a"));
            lib.set (1, 0, samplePatch ("b"));
            lib.set (7, 63, samplePatch ("c"));
            expect (lib.neighbour (0, 5, 1) == std::make_optional (std::make_pair (1, 0)));
            expect (lib.neighbour (1, 0, 1) == std::make_optional (std::make_pair (7, 63)));
            expect (lib.neighbour (7, 63, 1) == std::make_optional (std::make_pair (0, 5)));
            expect (lib.neighbour (0, 5, -1) == std::make_optional (std::make_pair (7, 63)));
            expect (lib.neighbour (0, 0, 1) == std::make_optional (std::make_pair (0, 5)));
        }

        beginTest ("salva e ricarica da file");
        {
            juce::TemporaryFile tmp (".xml");
            Library a;
            a.set (0, 0, samplePatch ("Piano"));
            a.set (7, 63, samplePatch ("Fine"));
            expect (a.saveTo (tmp.getFile()));
            Library b;
            expect (b.loadFrom (tmp.getFile()));
            expectEquals (b.get (0, 0)->name, juce::String ("Piano"));
            expectEquals (b.get (7, 63)->name, juce::String ("Fine"));
            expect (b.get (7, 63)->stations[1].instrument.state == a.get (7, 63)->stations[1].instrument.state);
            expect (! b.get (3, 3).has_value());
        }

        beginTest ("libreria illeggibile: non si carica e viene messa da parte");
        {
            const auto dir = juce::File::createTempFile ("lib");
            dir.createDirectory();
            const auto f = dir.getChildFile ("libreria.xml");
            f.replaceWithText ("<LIBRERIA><PATCH name='rotto'");   // XML troncato
            Library lib;
            expect (! lib.loadFrom (f));
            const auto moved = Library::setAsideUnreadable (f);
            expect (moved.existsAsFile());
            expect (! f.existsAsFile());
            expect (moved.getFileName().startsWith ("libreria-danneggiata-"));

            const auto other = dir.getChildFile ("altro.xml");
            other.replaceWithText ("<ALTRO/>");
            expect (! lib.loadFrom (other));   // XML valido ma non e' una libreria
            dir.deleteRecursively();
        }

        beginTest ("copia giornaliera");
        {
            const auto f = juce::File ("/tmp/x/libreria.xml");
            const auto backup = Library::dailyBackupFor (f, juce::Time (2026, 10, 21, 9, 30));   // mese 0-based: novembre
            expectEquals (backup.getFileName(), juce::String ("libreria-2026-11-21.xml"));
            expect (backup.getParentDirectory() == f.getParentDirectory());
        }
    }
};

static PatchTests patchTests;
