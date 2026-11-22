#include "plugins/PluginCatalog.h"

class PluginCatalogTests final : public juce::UnitTest
{
public:
    PluginCatalogTests() : juce::UnitTest ("PluginCatalog", "Stage") {}

    static juce::PluginDescription makeDesc (juce::String format, juce::String maker, juce::String name, int uid)
    {
        juce::PluginDescription d;
        d.pluginFormatName = format;
        d.manufacturerName = maker;
        d.name = name;
        d.uniqueId = uid;
        d.fileOrIdentifier = "/Library/Audio/Plug-Ins/VST3/" + name + ".vst3";
        d.isInstrument = true;
        return d;
    }

    void runTest() override
    {
        beginTest ("ID portabile");
        expectEquals (makePortableId (makeDesc ("VST3", "LennarDigital", "Sylenth1", 0x4c4e5331)),
                      juce::String ("VST3|LennarDigital|Sylenth1|4c4e5331"));

        beginTest ("le cartelle in piu' finiscono nel percorso di ricerca");
        {
            juce::VST3PluginFormat vst3;
            const auto standard = vst3.getDefaultLocationsToSearch();

            juce::TemporaryFile dirTmp;
            const auto dir = dirTmp.getFile();
            dir.createDirectory();
            const auto inesistente = dir.getSiblingFile ("non-esiste-davvero-12345");

            const auto path = PluginCatalog::searchPathFor (vst3, { dir.getFullPathName(),
                                                                    inesistente.getFullPathName() });
            juce::StringArray paths;
            for (int i = 0; i < path.getNumPaths(); ++i)
                paths.add (path[i].getFullPathName());

            expect (paths.contains (dir.getFullPathName()), "la cartella vera deve esserci");
            expect (! paths.contains (inesistente.getFullPathName()), "quella che non esiste no");
            expect (path.getNumPaths() >= standard.getNumPaths(), "le standard restano");

            // senza cartelle in piu' il percorso e' quello di sempre
            expectEquals (PluginCatalog::searchPathFor (vst3, {}).getNumPaths(), standard.getNumPaths());
        }

        beginTest ("elenca i file dei plugin senza aprirli");
        {
            juce::TemporaryFile listTmp2 (".xml"), pedalTmp2 (".txt");
            juce::TemporaryFile dirTmp;
            const auto dir = dirTmp.getFile();
            dir.createDirectory();

            // Finti bundle VST3 con la forma giusta ma senza codice dentro:
            // aprirli fallirebbe, elencarli no.
            for (const auto& name : { "Sylenth1", "Dune 2", "MuMU" })
            {
                const auto bundle = dir.getChildFile (juce::String (name) + ".vst3");
               #if JUCE_MAC
                auto binary = bundle.getChildFile ("Contents/MacOS").getChildFile (name);
               #elif JUCE_WINDOWS
                auto binary = bundle.getChildFile ("Contents/x86_64-win").getChildFile (juce::String (name) + ".vst3");
               #else
                auto binary = bundle.getChildFile ("Contents/x86_64-linux").getChildFile (juce::String (name) + ".so");
               #endif
                binary.getParentDirectory().createDirectory();
                binary.replaceWithText ("finto");
            }

            PluginCatalog cat (listTmp2.getFile(), pedalTmp2.getFile());
            cat.setExtraFolders ({ dir.getFullPathName() });

            const auto found = cat.findPluginFiles();
            juce::StringArray names;
            for (const auto& f : found)
                names.add (f.displayName);

            expect (names.contains ("Sylenth1"), "Sylenth1 nell'elenco");
            expect (names.contains ("Dune 2"), "Dune 2 nell'elenco");
            expect (names.contains ("MuMU"), "MuMU nell'elenco");

            // nessuno di questi e' stato aperto, quindi il catalogo resta vuoto
            expectEquals (cat.list().getNumTypes(), 0);

            for (const auto& f : found)
            {
                expect (f.key().contains ("|"), "la chiave e' formato|identificatore");
                expect (! f.alreadyKnown);
            }

            beginTest ("la scelta si imposta e si rilegge, senza doppioni");
            cat.setChosenPlugins ({ "VST3|/a", "VST3|/a", "", "VST3|/b" });
            expectEquals (cat.getChosenPlugins().size(), 2);
        }

        juce::TemporaryFile listTmp (".xml"), pedalTmp (".txt");

        beginTest ("salva e ricarica l'elenco dei plugin");
        {
            PluginCatalog cat (listTmp.getFile(), pedalTmp.getFile());
            cat.list().addType (makeDesc ("VST3", "LennarDigital", "Sylenth1", 0x4c4e5331));
            cat.list().addType (makeDesc ("VST3", "Synapse Audio", "DUNE 2", 0x53794432));
            cat.save();
        }
        {
            PluginCatalog cat (listTmp.getFile(), pedalTmp.getFile());
            cat.load();
            expectEquals (cat.list().getNumTypes(), 2);
            const auto syl = cat.findByPortableId ("VST3|LennarDigital|Sylenth1|4c4e5331");
            expect (syl.has_value() && syl->name == "Sylenth1");
            expect (cat.findByPortableId ("VST3|Synapse Audio|DUNE 2|53794432").has_value());
            expect (! cat.findByPortableId ("VST3|Nessuno|Niente|1").has_value());
        }

        beginTest ("i formati VST3 (e AU su macOS) sono registrati");
        {
            PluginCatalog cat (listTmp.getFile(), pedalTmp.getFile());
            juce::StringArray names;
            for (auto* f : cat.formats().getFormats())
                names.add (f->getName());
            expect (names.contains ("VST3"));
           #if JUCE_MAC
            expect (names.contains ("AudioUnit"));
           #endif
        }

        beginTest ("plugin con file inesistente: nessuna istanza e un errore");
        {
            PluginCatalog cat (listTmp.getFile(), pedalTmp.getFile());
            auto desc = makeDesc ("VST3", "Nessuno", "Fantasma", 1);
            desc.fileOrIdentifier = "/percorso/che/non/esiste/Fantasma.vst3";

            bool called = false;
            bool gotInstance = true;
            juce::String error;
            cat.createInstanceAsync (desc, 48000.0, 256,
                [&] (std::unique_ptr<juce::AudioPluginInstance> inst, juce::String err)
                {
                    called = true;
                    gotInstance = (inst != nullptr);
                    error = err;
                });

            for (int i = 0; i < 100 && ! called; ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

            expect (called);
            expect (! gotInstance);
            expect (error.isNotEmpty());
        }
    }
};

static PluginCatalogTests pluginCatalogTests;
