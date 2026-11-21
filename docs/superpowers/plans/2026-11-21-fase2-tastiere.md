# MBStudio91 Stage — Fase 2: tastiere — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Le quattro tastiere suonano ciascuna la propria postazione, assegnate per identificativo del dispositivo (con "premi un tasto"), con trasposizione, estensione tasti, curva di velocity e fader del volume; tutte e 4 le postazioni sono visibili e utilizzabili nell'interfaccia.

**Architecture:** Due unità pure e testate (`NoteProcessor` per trasposizione/estensione/velocity, `KeyboardRouter` per assegnazione e apprendimento), integrate in `StageEngine` con un `MidiMessageCollector` per postazione. L'interfaccia passa da un pannello unico a 4 `StationPanel` affiancati.

**Tech Stack:** C++17, JUCE 8.0.15, CMake, GitHub Actions `macos-15-intel`, `juce::UnitTest`.

**Spec:** `docs/superpowers/specs/2026-11-21-mbstudio91-stage-design.md` (sezioni 2.3, 5.2, 5.3, 5.7, 7 fase 2)

## Global Constraints

- Tutti i vincoli della Fase 1 restano validi (x86_64, macOS 12+, build solo su Actions, dati in `~/Music/MBStudio91 Stage/`, nessun caricamento sul thread audio).
- 4 postazioni; ogni tastiera fisica va a una sola postazione e ogni postazione ha al massimo una tastiera.
- Assegnazione per **identificativo del dispositivo MIDI**, non per nome; persistente al riavvio; tastiera assente = LED spento, nessun errore.
- Le porte non assegnate (comprese le porte DAW/InControl delle Launchkey e la Launchpad) non suonano nulla.
- Curve di velocity: `out = clamp(round(127 · (v/127)^γ), 1, 127)` per note-on con v ≥ 1; γ: lineare 1.0, morbida 0.6, dura 1.6, personalizzata = valore scelto tra 0.3 e 3.0.
- Fader volume: CC7 della tastiera assegnata imposta il volume della postazione e non arriva al plugin. `dB = 40·log10(v/127)`, minimo −60 dB (v = 0 → −60 dB). Attivo di default solo sulla postazione 1 (Keystation) **[proposta spec]**, attivabile sulle altre.
- Trasposizione −48…+48 semitoni; estensione tasti nota bassa/alta 0…127 riferita al tasto fisico, prima della trasposizione.

## Review Focus

1. **Trasposizione o split cambiati mentre una nota è tenuta**: il note-off deve spegnere la nota realmente suonata (nessuna nota appesa). → test in Task 1.
2. **Nota trasposta fuori da 0–127**: scartata sia al note-on sia al note-off, senza crash. → test in Task 1.
3. **Tastiera assegnata a una postazione e poi riassegnata a un'altra**: la prima postazione non la riceve più. → test in Task 2.
4. **PANIC dopo un cambio di trasposizione con note tenute**: nessuna nota resta. → test in Task 3.
5. **Messaggi da un dispositivo non assegnato**: ignorati, nessuna postazione suona. → test in Task 3.

---

## File Structure

```
Source/engine/NoteProcessor.h/.cpp    KeyZone + VelocityCurve + trasformazione MIDI per postazione (pura)
Source/engine/KeyboardRouter.h/.cpp   assegnazioni dispositivo→postazione, apprendimento, XML (pura, thread-safe)
Source/engine/StageEngine.h/.cpp      collector per postazione, NoteProcessor, CC7, learn, attività, file
Source/settings/AppPaths.h/.cpp       + keyboardsFile() "tastiere.xml", stationsFile() "postazioni.xml"
Source/ui/StationPanel.h/.cpp         pannello di una postazione (estratto da MainComponent)
Source/ui/MainComponent.h/.cpp        barra superiore + 4 StationPanel
Tests/NoteProcessorTests.cpp, Tests/KeyboardRouterTests.cpp, Tests/StageEngineTests.cpp (aggiornato)
docs/collaudo-fase2.md
```

---

### Task 1: NoteProcessor (trasposizione, estensione, velocity)

**Files:** Create `Source/engine/NoteProcessor.h/.cpp`, `Tests/NoteProcessorTests.cpp`

**Interfaces — Produces:**
```cpp
enum class VelocityCurve { lineare, morbida, dura, personalizzata };
struct KeyZone {
    int transpose = 0;            // -48..+48
    int lowNote = 0, highNote = 127;
    VelocityCurve curve = VelocityCurve::lineare;
    float customGamma = 1.0f;     // 0.3..3.0, usato solo con personalizzata
    bool faderControlsVolume = false;
    juce::ValueTree toValueTree() const;  static KeyZone fromValueTree (const juce::ValueTree&);
    bool operator== (const KeyZone&) const;
};
int applyVelocityCurve (int velocity1to127, VelocityCurve, float customGamma);
float faderValueToDb (int cc7value);
class NoteProcessor {
public:
    void setZone (const KeyZone&);        // qualsiasi thread (SpinLock); l'audio usa try-lock e l'ultima copia
    KeyZone getZone() const;
    // Restituisce i messaggi da passare allo strumento; se faderControlsVolume e arriva CC7, lo toglie e scrive il dB in faderDb.
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, std::optional<float>& faderDb);
    void reset();                         // dimentica le note tenute (dopo PANIC)
};
```
Le note accese vengono memorizzate per (canale, tasto fisico) → nota suonata; il note-off usa la nota memorizzata, non la zona corrente. Note-on fuori estensione o trasposte fuori 0–127 non vengono memorizzate e il loro note-off viene scartato. Aftertouch polifonico segue la stessa mappa. Gli altri messaggi passano invariati.

- [ ] **Step 1: test RED** — `applyVelocityCurve(64, lineare)==64`, `(64, morbida)==84`, `(64, dura)==42`, `(1, dura)==1`, `(127, x)==127` per ogni curva, `(64, personalizzata, 1.0f)==64`; `faderValueToDb(127)==0`, `faderValueToDb(64)≈-11.9 (±0.1)`, `faderValueToDb(0)==-60`; trasposizione +12: note-on 60 → 72; zona 36–59: note 60 scartata, 59 passa; cambio trasposizione a +5 con nota 60 tenuta: il note-off esce come 72; trasposizione +12 e nota 120: note-on e note-off scartati; CC1 e pitch bend passano; con faderControlsVolume CC7=127 → `faderDb==0` e CC7 non in uscita, senza → CC7 in uscita e `faderDb` vuoto; `reset()` poi note-off 60 → scartato; round trip `KeyZone::fromValueTree(z.toValueTree())==z`.
- [ ] **Step 2:** verificare il fallimento. **Step 3:** implementare. **Step 4:** verde. **Step 5: Commit** `feat: trasposizione, estensione e curve di velocity per postazione`

### Task 2: KeyboardRouter (assegnazione e apprendimento)

**Files:** Create `Source/engine/KeyboardRouter.h/.cpp`, `Tests/KeyboardRouterTests.cpp`; Modify `Source/settings/AppPaths.h/.cpp`

**Interfaces — Produces:**
```cpp
class KeyboardRouter {   // thread-safe (CriticalSection): MIDI thread e message thread
public:
    struct Assignment { juce::String deviceId, deviceName; };
    void assign (int station0to3, const juce::String& deviceId, const juce::String& deviceName); // toglie il dispositivo da altre postazioni
    void clear (int station0to3);
    int stationFor (const juce::String& deviceId) const;          // -1 se non assegnato
    std::optional<Assignment> assignmentFor (int station0to3) const;
    void startLearn (int station0to3);  void cancelLearn();  int learningStation() const; // -1 se nessuna
    // Se in apprendimento e il messaggio e' un note-on: assegna, chiude l'apprendimento, restituisce la postazione; altrimenti -1.
    int handleLearn (const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage&);
    static int suggestStationForDeviceName (const juce::String& name); // "Keystation"→0, "Launchkey 61"→1, altrimenti -1; mai porte "DAW"/"InControl"
    std::unique_ptr<juce::XmlElement> toXml() const;  void fromXml (const juce::XmlElement&);
};
```
AppPaths: `keyboardsFile()` = `tastiere.xml`, `stationsFile()` = `postazioni.xml`.

- [ ] **Step 1: test RED** — assign 0 → `stationFor` 0; assign stesso id a 2 → `stationFor`==2 e postazione 0 vuota (Review Focus 3); assegnare un altro id a 2 libera il precedente; `clear(2)`; learn: `startLearn(1)`, CC → -1, note-on → 1 e `learningStation()==-1`; suggest: "Keystation 88 MK3"→0, "Launchkey 61"→1, "Launchkey 61 LK 61 InControl"→-1, "Launchkey Mini"→-1, "Launchpad Mini"→-1; round trip XML con 2 assegnazioni; AppPaths nuovi nomi file.
- [ ] **Steps 2–4** RED/GREEN. **Step 5: Commit** `feat: assegnazione delle tastiere alle postazioni con apprendimento`

### Task 3: Instradamento nel motore

**Files:** Modify `Source/engine/StageEngine.h/.cpp`, `Tests/StageEngineTests.cpp`, `Tests/FakeInstrument.h` (se serve)

**Interfaces:**
- Consumes: `NoteProcessor`, `KeyZone`, `faderValueToDb` (Task 1); `KeyboardRouter` (Task 2).
- Produces (sostituisce `renderBlock(out, midiForStation0)`):
  ```cpp
  void renderBlock (juce::AudioBuffer<float>& out);
  void routeIncoming (const juce::String& deviceId, const juce::String& deviceName, const juce::MidiMessage&); // chiamato da handleIncomingMidiMessage; pubblico per i test
  KeyboardRouter& keyboards();
  void setZone (int station, const KeyZone&);  KeyZone getZone (int station) const;
  bool isStationDeviceConnected (int station) const;   // dispositivo assegnato presente ora
  juce::uint32 lastActivityMs (int station) const;     // Time::getMillisecondCounter dell'ultimo messaggio
  void loadKeyboardSettings();  void saveKeyboardSettings() const; // tastiere.xml + postazioni.xml (zone)
  std::function<void (int station)> onLearnFinished;   // chiamato sul message thread
  ```
- Un `MidiMessageCollector` per postazione; `routeIncoming` → learn, poi `stationFor`, poi il collector della postazione (dispositivi non assegnati ignorati).
- Nel blocco audio: collector → `NoteProcessor::process` → se `faderDb` → `station.setGainDb` → `renderAdd`.
- `panicAll()` chiama anche `NoteProcessor::reset()` di ogni postazione.
- Al primo avvio senza `tastiere.xml`: per ogni dispositivo presente, `suggestStationForDeviceName` se la postazione è libera.
- Station: aggiungere `float getGainDb() const` (per l'interfaccia).
- Zona predefinita della postazione 1: `faderControlsVolume = true`.

- [ ] **Step 1: test RED** — note da un dispositivo assegnato alla postazione 2 suonano solo la 2; dispositivo non assegnato → silenzio (Review Focus 5); zona con trasposizione +12 sulla 1: il FakeInstrument riceve 72; CC7=64 con fader attivo → `station(0).getGainDb()≈-11.9` e il fake non riceve CC7; learn: `startLearn(3)` + note-on da "dev-x" → `stationFor("dev-x")==3`; PANIC dopo cambio trasposizione con nota tenuta → silenzio e nessuna nota nel tracker del fake (Review Focus 4); `lastActivityMs` aggiornato. I messaggi dei test hanno timestamp `Time::getMillisecondCounterHiRes()*0.001`.
- [ ] **Steps 2–4** RED/GREEN (aggiornare i test esistenti alla nuova firma di `renderBlock`). **Step 5: Commit** `feat: ogni tastiera suona la sua postazione`

### Task 4: Interfaccia a 4 postazioni

**Files:** Create `Source/ui/StationPanel.h/.cpp`; Modify `Source/ui/MainComponent.h/.cpp`, `Source/Main.cpp`, `CMakeLists.txt`

- `StationPanel(StageEngine&, PluginCatalog&, int index)`: intestazione "n POSTAZIONE n", nome tastiera assegnata o "NESSUNA TASTIERA", LED (verde se collegata, lampeggio bianco per 120 ms all'attività, spento se assente), pulsante **ASSEGNA** (in apprendimento: "PREMI UN TASTO…", clic di nuovo annulla), menu strumento + 3 effetti con **APRI** (logica e finestre editor spostate da MainComponent), manopola **VOLUME** sincronizzata con `getGainDb()` (il fader la muove), **MUTE**, **TRASP** (−48…+48), **DA** / **A** (nomi nota, es. C1), menu **CURVA** (LINEARE, MORBIDA, DURA, PERSONALIZZATA) con manopola γ visibile solo per personalizzata, interruttore **FADER VOL**.
- `MainComponent`: barra superiore (titolo, IMPOSTAZIONI AUDIO, SCANSIONA PLUGIN, PANIC), 4 `StationPanel` affiancati, riga di stato. Finestra 1400×820.
- `Main.cpp`: `engine->loadKeyboardSettings()` dopo `start()`; `saveKeyboardSettings()` allo `shutdown` e a ogni modifica di zona/assegnazione.
- Verifica: app compila su Linux e su CI; i test restano verdi. **Commit** `feat: interfaccia a quattro postazioni`

### Task 5: Documenti, collaudo, revisione

- `docs/collaudo-fase2.md`: assegnazione delle 4 tastiere con ASSEGNA; riavvio e assegnazioni ricordate; tastiera staccata = LED spento; trasposizione con nota tenuta senza note appese; split; curve sulla Keystation; fader Keystation → volume; porte DAW delle Launchkey non suonano; 4 strumenti diversi insieme.
- README: sezione Fase 2.
- Revisione finale dell'intero ramo, correzioni, merge.
