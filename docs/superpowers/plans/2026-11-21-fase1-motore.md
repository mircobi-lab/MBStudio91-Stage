# MBStudio91 Stage — Fase 1: motore — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un'app macOS (JUCE) che apre l'uscita audio, scansiona i plugin VST3/AU in modo sicuro, carica uno strumento più effetti sulla postazione 1, lo suona da qualsiasi tastiera MIDI, apre l'editor del plugin e ha un PANIC affidabile, compilata e testata su GitHub Actions.

**Architecture:** Motore audio (`StageEngine`) con 4 oggetti `Station` mescolati in un callback audio; in Fase 1 l'interfaccia espone solo la postazione 1, ma il motore ne contiene già 4. La scansione dei plugin gira in un processo figlio. La logica pura (panic, note tenute, ID portabile, mixaggio) è coperta da test unitari JUCE eseguiti in CI.

**Tech Stack:** C++17, JUCE 8 (via CMake FetchContent), CMake ≥ 3.22, GitHub Actions (runner macOS), test con `juce::UnitTest` + CTest.

**Spec:** `docs/superpowers/specs/2026-11-21-mbstudio91-stage-design.md` (sezioni 2, 3, 5.1, 5.6, 6, 7 fase 1)

## Global Constraints

- Nome app: `MBStudio91 Stage`; bundle id `com.mbstudio91.stage`; repo `mircobi-lab/MBStudio91-Stage`.
- Formati ospitati: VST3 e AU. App standalone, non plugin.
- Build di rilascio: `x86_64`, `CMAKE_OSX_DEPLOYMENT_TARGET=12.0`.
- Compilazione solo su GitHub Actions; artifact `.zip` dell'app scaricabile; Release automatica sui tag `v*`.
- Cartella dati: `~/Music/MBStudio91 Stage/` (impostazioni macchina, elenco plugin, log scansione).
- Postazione: strumento + massimo **3** effetti + volume; il motore ha **4** postazioni.
- PANIC sempre visibile nell'interfaccia.
- App non firmata: istruzioni di primo avvio nel README in italiano.
- Stile: telaio nero, scritte bianche, Bebas Neue (OFL) incorporato; l'interfaccia completa è Fase 4, in Fase 1 basta lo schema di colori.
- Il caricamento di un plugin non deve mai avvenire sul thread audio.

## Review Focus

1. **Plugin che non si carica** (file spostato, plugin non valido): la postazione mostra l'errore, il motore continua a suonare e le altre postazioni non cambiano. → test in Task 3 e Task 4.
2. **PANIC con il pedale di sustain premuto**: dopo il PANIC nessuna nota deve restare, anche se il pedale è ancora giù. → test in Task 2.
3. **Cambio di frequenza di campionamento o buffer con un plugin caricato**: il plugin viene ripreparato e suona alla nuova frequenza. → test in Task 3.
4. **Sostituzione di uno strumento con il suo editor aperto**: la finestra si chiude prima che l'istanza venga distrutta (nessun crash). → verifica in Task 5.
5. **Tastiera MIDI scollegata e ricollegata mentre l'app è aperta**: l'ingresso torna attivo senza riavviare. → verifica manuale in Task 6.

---

## File Structure

```
CMakeLists.txt                     app + target di test, FetchContent JUCE
.github/workflows/build.yml        build x86_64, test, zip, release su tag
README.md                          italiano: download, primo avvio, uso Fase 1
Assets/Fonts/BebasNeue-Regular.ttf + OFL.txt
Source/Main.cpp                    JUCEApplication, finestra, avvio scanner figlio
Source/engine/NoteTracker.h/.cpp   note e sustain tenuti per canale (logica pura)
Source/engine/MidiPanic.h/.cpp     costruzione del MidiBuffer di panic (logica pura)
Source/engine/Station.h/.cpp       una postazione: strumento, ≤3 effetti, volume, mute
Source/engine/StageEngine.h/.cpp   device audio/MIDI, 4 postazioni, mix, PANIC globale
Source/plugins/PluginCatalog.h/.cpp KnownPluginList, scansione out-of-process, ID portabile
Source/plugins/ScannerProcess.h/.cpp processo figlio di scansione (ChildProcessWorker)
Source/settings/AppPaths.h/.cpp    percorsi in ~/Music/MBStudio91 Stage/
Source/ui/MainComponent.h/.cpp     interfaccia minima Fase 1
Source/ui/StageLookAndFeel.h/.cpp  colori neri/bianchi, Bebas Neue
Tests/TestMain.cpp                 esegue tutti i juce::UnitTest, exit code ≠ 0 se fallisce
Tests/FakeInstrument.h             AudioProcessor finto per i test
Tests/*Tests.cpp                   un file per unità
```

---

### Task 1: Scheletro del progetto e CI

**Files:**
- Create: `CMakeLists.txt`, `.github/workflows/build.yml`, `Source/Main.cpp`, `Source/settings/AppPaths.h/.cpp`, `Tests/TestMain.cpp`, `Tests/AppPathsTests.cpp`, `.gitignore`, `README.md` (bozza)

**Interfaces:**
- Produces: target CMake `MBStudio91Stage` (app) e `StageTests` (console); libreria interna `StageCore` (static) con tutti i sorgenti tranne `Main.cpp` e `ui/`, linkata da entrambi.
- Produces: `namespace AppPaths { juce::File dataDir(); juce::File pluginListFile(); juce::File deadMansPedalFile(); juce::File machineSettingsFile(); }` — rispettivamente `~/Music/MBStudio91 Stage/`, `…/plugins.xml`, `…/scan-in-corso.txt`, `…/macchina.settings`. `dataDir()` crea la cartella se manca.

- [ ] **Step 1:** Creare il repo `mircobi-lab/MBStudio91-Stage` (privato salvo diversa indicazione), copiare `docs/` con spec e piano.
- [ ] **Step 2:** `CMakeLists.txt`: FetchContent di JUCE fissato all'ultimo tag `8.0.x` stabile (verificare il tag su GitHub al momento e scriverlo esplicitamente), `juce_add_gui_app` con `PLUGINHOST_VST3=1`, `PLUGINHOST_AU=1`, `JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1`, `MICROPHONE_PERMISSION_ENABLED FALSE`, `NEEDS_CURL FALSE`, `NEEDS_WEB_BROWSER FALSE`. Moduli: `juce_audio_utils`, `juce_audio_processors`, `juce_gui_extra`, `juce_dsp`.
- [ ] **Step 3: Write the failing test** `AppPathsTests`: `expect(AppPaths::dataDir().getFileName() == "MBStudio91 Stage")`, `expect(AppPaths::dataDir().getParentDirectory().getFileName() == "Music")`, `expect(AppPaths::pluginListFile().getParentDirectory() == AppPaths::dataDir())`, `expect(AppPaths::dataDir().isDirectory())`.
- [ ] **Step 4:** Workflow `build.yml`, due job su `macos-14`:
  - `test`: configura nativo (arm64), compila `StageTests`, esegue `ctest --output-on-failure`.
  - `app`: configura con `-DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_BUILD_TYPE=Release`, compila `MBStudio91Stage`, verifica con `lipo -archs` che il binario sia `x86_64`, `ditto -c -k --keepParent` → `MBStudio91-Stage-macOS-Intel.zip`, `actions/upload-artifact`; se `github.ref` è un tag `v*`, `softprops/action-gh-release` con lo zip.
  - Trigger: push su `main`, pull request, tag `v*`, `workflow_dispatch`.
- [ ] **Step 5:** Push; il job `test` deve FALLIRE (AppPaths non implementato).
- [ ] **Step 6:** Implementare `AppPaths` con `juce::File::getSpecialLocation(userMusicDirectory)`; `Main.cpp` apre una finestra vuota nera intitolata "MBStudio91 Stage".
- [ ] **Step 7:** Push; Expected: entrambi i job verdi, artifact zip presente, `lipo` stampa `x86_64`.
- [ ] **Step 8: Commit** `feat: scheletro progetto, AppPaths e CI GitHub Actions`

### Task 2: NoteTracker e MidiPanic

**Files:**
- Create: `Source/engine/NoteTracker.h/.cpp`, `Source/engine/MidiPanic.h/.cpp`, `Tests/NoteTrackerTests.cpp`, `Tests/MidiPanicTests.cpp`

**Interfaces:**
- Produces: `class NoteTracker { void process(const juce::MidiBuffer&); bool isNoteHeld(int channel1to16, int note) const; bool isSustainDown(int channel1to16) const; int heldNoteCount() const; void reset(); }` — note-on con velocity 0 conta come note-off; CC64 ≥ 64 = sustain giù.
- Produces: `juce::MidiBuffer MidiPanic::makePanicBuffer(const NoteTracker&, int samplePosition = 0)` — per ogni nota tenuta un note-off; poi per **tutti i 16 canali**: CC64=0, CC123=0 (All Notes Off), CC120=0 (All Sound Off), in quest'ordine.

- [ ] **Step 1: Write the failing tests**
  - `NoteTracker`: note-on ch1 n60 → `isNoteHeld(1,60)`; note-off → false; note-on vel 0 → false; CC64=127 ch2 → `isSustainDown(2)`; CC64=0 → false; `reset()` → `heldNoteCount()==0`.
  - `MidiPanic` con ch1 n60 e n64 tenuti e sustain giù su ch1: il buffer contiene note-off per 60 e 64 su ch1; contiene CC64=0, CC123, CC120 per ognuno dei canali 1–16; i note-off vengono prima dei CC (posizione nell'iterazione).
  - Dopo `tracker.process(panicBuffer)`: `heldNoteCount()==0` e `isSustainDown(1)==false` (Review Focus 2).
- [ ] **Step 2:** Push/CI: Expected FAIL (simboli mancanti).
- [ ] **Step 3:** Implementare; il tracker gestisce anche CC123/CC120 azzerando il canale.
- [ ] **Step 4:** CI: Expected PASS.
- [ ] **Step 5: Commit** `feat: NoteTracker e costruzione del MIDI panic`

### Task 3: Station

**Files:**
- Create: `Source/engine/Station.h/.cpp`, `Tests/FakeInstrument.h`, `Tests/StationTests.cpp`

**Interfaces:**
- Consumes: `NoteTracker`, `MidiPanic::makePanicBuffer` (Task 2).
- Produces:
  ```cpp
  class Station {
  public:
      static constexpr int maxEffects = 3;
      void prepare(double sampleRate, int maxBlockSize);
      // chiamati dal message thread
      void setInstrument(std::unique_ptr<juce::AudioPluginInstance>);   // nullptr = svuota
      bool setEffect(int slot0to2, std::unique_ptr<juce::AudioPluginInstance>); // false se slot fuori range
      juce::AudioPluginInstance* getInstrument() const;
      juce::AudioPluginInstance* getEffect(int slot) const;
      void setLoadError(juce::String);  juce::String getLoadError() const;
      void setGainDb(float);  void setMuted(bool);  bool isMuted() const;
      void requestPanic();   // thread-safe, eseguito al prossimo blocco audio
      std::function<void(juce::AudioPluginInstance&)> onBeforeInstanceRemoved; // per chiudere l'editor (Task 5)
      // thread audio: ADDIZIONA il proprio output in `out`
      void renderAdd(juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiIn);
  };
  ```
- Thread safety in Fase 1: le sostituzioni prendono un `juce::SpinLock`; `renderAdd` usa `ScopedTryLock` e, se il lock è occupato, salta il blocco (silenzio per un blocco). Le istanze rimosse vengono distrutte sul message thread, mai sul thread audio. Il doppio slot A/B arriva in Fase 3.
- Il guadagno è applicato con `juce::SmoothedValue` (rampa 20 ms) per evitare clic; mute = guadagno 0 con la stessa rampa.

- [ ] **Step 1:** `FakeInstrument` (`juce::AudioPluginInstance` minimale): mentre almeno una nota è tenuta scrive `0.5f` su tutti i canali, altrimenti 0; registra i messaggi MIDI ricevuti e l'ultimo `sampleRate` passato a `prepareToPlay`. `FakeGainEffect` moltiplica per un fattore.
- [ ] **Step 2: Write the failing tests**
  - Nessuno strumento → `renderAdd` lascia il buffer invariato.
  - Strumento + note-on → output ≈ 0.5 (dopo la rampa) sommato al contenuto preesistente del buffer.
  - Effetto `FakeGainEffect(0.5)` nello slot 0 → ≈ 0.25; `setEffect(3, …)` → false.
  - `setMuted(true)` → output ≈ 0 a fine rampa.
  - `requestPanic()` con nota e sustain tenuti → il FakeInstrument riceve i note-off e CC123; il blocco successivo è silenzio (Review Focus 2).
  - `setInstrument(nullptr)` + `setLoadError("File non trovato")` → `renderAdd` non suona, `getLoadError()` restituisce il testo (Review Focus 1).
  - `prepare(44100, 512)` poi `prepare(48000, 256)` → il FakeInstrument riporta 48000 (Review Focus 3).
  - Sostituendo lo strumento viene chiamato `onBeforeInstanceRemoved` con la vecchia istanza.
- [ ] **Step 3:** CI: Expected FAIL.
- [ ] **Step 4:** Implementare `Station`.
- [ ] **Step 5:** CI: Expected PASS.
- [ ] **Step 6: Commit** `feat: postazione con strumento, 3 effetti, volume, mute e panic`

### Task 4: PluginCatalog e scansione in processo separato

**Files:**
- Create: `Source/plugins/PluginCatalog.h/.cpp`, `Source/plugins/ScannerProcess.h/.cpp`, `Tests/PluginCatalogTests.cpp`
- Modify: `Source/Main.cpp` (all'avvio, se la riga di comando contiene l'UID dello scanner, avvia solo `ScannerProcess` e non la GUI)

**Interfaces:**
- Produces:
  ```cpp
  juce::String makePortableId(const juce::PluginDescription&);
  // "<formato>|<produttore>|<nome>|<uniqueId esadecimale>", es. "VST3|LennarDigital|Sylenth1|4c4e5331"
  class PluginCatalog {
  public:
      explicit PluginCatalog(juce::File listFile, juce::File deadMansPedal);
      juce::AudioPluginFormatManager& formats();     // VST3 + AU registrati
      juce::KnownPluginList& list();
      void load();  void save() const;               // XML su listFile
      void scanAsync(std::function<void(int found, juce::StringArray failed)> onDone); // scansione fuori processo
      const juce::PluginDescription* findByPortableId(const juce::String&) const;
      using LoadCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance>, juce::String error)>;
      void createInstanceAsync(const juce::PluginDescription&, double sampleRate, int blockSize, LoadCallback);
  };
  ```
- Scansione: `KnownPluginList::setCustomScanner` con uno scanner che delega a un processo figlio (`juce::ChildProcessCoordinator` / `ChildProcessWorker`, schema dell'AudioPluginHost di JUCE). Se il figlio muore su un plugin, quel file finisce nella blacklist e la scansione prosegue. Il dead-man's-pedal (`AppPaths::deadMansPedalFile()`) protegge anche da un crash del processo principale.
- La scansione parte solo dal pulsante "Scansiona plugin" o se `plugins.xml` non esiste (spec 5.6).
- Il caricamento usa `AudioPluginFormatManager::createPluginInstanceAsync` (mai sul thread audio). In caso di errore il callback riceve `nullptr` e un messaggio in italiano.

- [ ] **Step 1: Write the failing tests**
  - `makePortableId` su una `PluginDescription` costruita a mano (`pluginFormatName="VST3"`, `manufacturerName="LennarDigital"`, `name="Sylenth1"`, `uniqueId=0x4c4e5331`) == `"VST3|LennarDigital|Sylenth1|4c4e5331"`.
  - Round trip: aggiungere 2 descrizioni a `list()`, `save()` su file temporaneo, nuovo `PluginCatalog` sullo stesso file, `load()` → 2 tipi, `findByPortableId` trova entrambi, un ID inesistente → `nullptr`.
  - `createInstanceAsync` con una descrizione il cui `fileOrIdentifier` punta a un file inesistente → callback con `nullptr` ed errore non vuoto (Review Focus 1). Il test attende il callback con un `juce::WaitableEvent` e pompa il message loop.
- [ ] **Step 2:** CI: Expected FAIL.
- [ ] **Step 3:** Implementare `PluginCatalog` e `ScannerProcess`.
- [ ] **Step 4:** CI: Expected PASS.
- [ ] **Step 5: Commit** `feat: catalogo plugin VST3/AU con scansione in processo separato`

### Task 5: StageEngine e interfaccia minima

**Files:**
- Create: `Source/engine/StageEngine.h/.cpp`, `Source/ui/MainComponent.h/.cpp`, `Source/ui/StageLookAndFeel.h/.cpp`, `Assets/Fonts/BebasNeue-Regular.ttf`, `Assets/Fonts/OFL.txt`, `Tests/StageEngineTests.cpp`
- Modify: `Source/Main.cpp`, `CMakeLists.txt` (binary data per il font)

**Interfaces:**
- Consumes: `Station` (Task 3), `PluginCatalog` (Task 4), `AppPaths` (Task 1).
- Produces:
  ```cpp
  class StageEngine : private juce::AudioIODeviceCallback, private juce::MidiInputCallback {
  public:
      static constexpr int numStations = 4;
      StageEngine();                                   // non apre il device
      void start();                                    // ripristina il device audio da AppPaths::machineSettingsFile(), apre tutti gli ingressi MIDI
      void stop();
      juce::AudioDeviceManager& deviceManager();
      Station& station(int index0to3);
      void panicAll();                                 // requestPanic su tutte le postazioni
      void setMasterGainDb(float);
      // usato dai test e dal callback audio
      void renderBlock(juce::AudioBuffer<float>& out, const juce::MidiBuffer& midiForStation0);
      void injectMidiForTest(int station0to3, const juce::MidiBuffer&); // consumato al prossimo renderBlock
  };
  ```
- Fase 1: tutti gli ingressi MIDI vanno alla postazione 1 (indice 0), raccolti con `juce::MidiMessageCollector`. L'instradamento per tastiera arriva in Fase 2.
- Gli ingressi MIDI vengono riaperti quando cambia l'elenco dei dispositivi (`juce::MidiDeviceListConnection`, JUCE 7.0.6+), così una tastiera ricollegata torna attiva (Review Focus 5).
- `MainComponent` (Fase 1, funzionale): pulsante **IMPOSTAZIONI AUDIO** (`AudioDeviceSelectorComponent` in un dialog; la scelta salvata in `macchina.settings`), **SCANSIONA PLUGIN**, menu a tendina strumento (solo `isInstrument`) e 3 menu effetti per la postazione 1, **APRI PLUGIN** (editor in `DocumentWindow` separata), manopola **VOLUME**, **MUTE**, pulsante **PANIC** rosso sempre visibile, riga di stato con errore di caricamento e CPU (`deviceManager().getCpuUsage()`). Sfondo `#000000`, testo `#FFFFFF`, Bebas Neue.
- Prima di sostituire o rimuovere un'istanza, `onBeforeInstanceRemoved` chiude e distrugge la sua finestra editor (Review Focus 4).

- [ ] **Step 1: Write the failing tests**
  - `renderBlock` con `FakeInstrument` sulle postazioni 0 e 1, nota sulla 0: il buffer di uscita ≈ 0.5 (la postazione 1 riceve MIDI solo dalla sua sorgente, che in Fase 1 è vuota).
  - Con note su due postazioni (iniettate direttamente con un metodo di test `injectMidiForTest(int station, MidiBuffer)`) l'uscita ≈ 1.0, cioè la somma.
  - `setMasterGainDb(-6.0206f)` → uscita dimezzata a fine rampa.
  - `panicAll()` → tutte le postazioni smettono di suonare entro il blocco successivo.
- [ ] **Step 2:** CI: Expected FAIL.
- [ ] **Step 3:** Implementare `StageEngine`, poi `MainComponent` e `StageLookAndFeel`.
- [ ] **Step 4:** CI: Expected PASS e app compilata.
- [ ] **Step 5: Commit** `feat: motore a 4 postazioni e interfaccia minima Fase 1`

### Task 6: README, release e collaudo sul MacBook

**Files:**
- Modify: `README.md`
- Create: `docs/collaudo-fase1.md` (checklist che Mirco compila)

- [ ] **Step 1:** README in italiano: cosa fa la Fase 1, dove scaricare lo zip (Actions → artifact, oppure Releases), primo avvio di un'app non firmata (clic destro → Apri, oppure `xattr -dr com.apple.quarantine "/Applications/MBStudio91 Stage.app"`), dove stanno i dati (`~/Music/MBStudio91 Stage/`), come rifare la scansione.
- [ ] **Step 2:** `docs/collaudo-fase1.md`, checklist da provare sul MacBook Pro:
  1. L'app si apre e mostra la finestra nera.
  2. IMPOSTAZIONI AUDIO: scelta della scheda e del buffer; dopo il riavvio la scelta è ricordata.
  3. SCANSIONA PLUGIN trova Sylenth1, DUNE 2 e almeno due plugin MBStudio91 (VST3 o AU).
  4. Ogni strumento suona dalla Keystation 88 MK3 e dalle Launchkey.
  5. APRI PLUGIN mostra l'editor; cambiare strumento con l'editor aperto non fa crashare l'app.
  6. Effetto nello slot 1 udibile; VOLUME e MUTE senza clic.
  7. Note tenute col pedale + PANIC: silenzio immediato, nessuna nota appesa.
  8. Scollegare e ricollegare una Launchkey mentre l'app è aperta: torna a suonare.
  9. Cambiare frequenza di campionamento con un plugin caricato: suona all'intonazione giusta.
- [ ] **Step 3:** Tag `v0.1.0`; Expected: Release con `MBStudio91-Stage-macOS-Intel.zip`.
- [ ] **Step 4: Commit** `docs: README italiano e collaudo Fase 1`

---

## Fasi successive (piani separati, da scrivere a Fase 1 collaudata)

- **Fase 2**: assegnazione delle tastiere per identificativo dispositivo, trasposizione, estensione tasti, curve di velocity, fader Keystation → volume postazione 1.
- **Fase 3**: libreria (8 banchi × 64 patch), [SALVA], ritocchi "modificata", cambio patch con doppio slot A/B, Launchpad Mini MK1 (LED, banchi, mute, prev/next, tap, PANIC), tempo INT/EXT con MIDI clock, limitatore master.
- **Fase 4**: interfaccia MBStudio91 completa (skill `mbstudio-hardware-ui`), schermo intero, MIDI learn di manopole e fader.
- **Fase 5**: rifiniture e prova in sala prove.
