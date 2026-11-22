# MBStudio91 Stage

Host per strumenti virtuali in stile MainStage, pensato per suonare i plugin MBStudio91, Sylenth1 e DUNE 2 in studio e in sala prove con quattro tastiere e una Launchpad.

Il progetto è descritto in `docs/superpowers/specs/2026-11-21-mbstudio91-stage-design.md`. Lo sviluppo procede a fasi: sono pronte la **Fase 1 (motore)**, la **Fase 2 (tastiere)** e la **Fase 3 (patch, Launchpad, tempo)**.

## Cosa fa la Fase 1

- Apre la scheda audio scelta nelle impostazioni e se la ricorda al riavvio.
- Cerca i plugin VST3 e AU installati. La ricerca gira in un processo separato: se un plugin si blocca o va in crash durante la scansione, viene scartato e l'app resta aperta.
- Carica sulla postazione 1 uno strumento e fino a tre effetti, con volume e mute.
- Apre la finestra originale di ogni plugin con APRI PLUGIN.
- Suona da tutte le tastiere MIDI collegate (la Launchpad è esclusa: servirà per le patch dalla Fase 3).
- PANIC ferma subito tutte le note, anche con il pedale premuto.

## Cosa aggiunge la Fase 2

- Quattro postazioni affiancate, ognuna con strumento, tre effetti, volume e mute.
- Ogni tastiera suona la sua postazione. Keystation 88 e Launchkey 61 vengono riconosciute da sole al primo avvio; le due Launchkey Mini si assegnano con ASSEGNA e un tasto qualsiasi. Le assegnazioni restano anche dopo il riavvio.
- Il LED della postazione è verde se la tastiera è collegata e lampeggia bianco quando arriva MIDI.
- Per ogni postazione: trasposizione (TRASP, ±48 semitoni), estensione dei tasti (DA, A; il Do centrale è C3 come in Ableton), curva di velocity (lineare, morbida, dura, personalizzata).
- Il fader della Keystation regola il volume della postazione 1 (FADER VOL, attivabile anche sulle altre).
- Le porte DAW e InControl delle Launchkey e la Launchpad non suonano nulla.

## Cosa aggiunge la Fase 3

- Libreria di 8 banchi (A–H) da 64 patch. Una patch ricorda per ogni postazione strumento, effetti con le loro impostazioni, volume, mute, trasposizione, estensione e curva.
- Pannello PATCH a destra: clic su un pad = richiamo; Maiusc + clic = solo selezione. SALVA, RINOMINA, COPIA, INCOLLA, CANCELLA. L'asterisco accanto al nome indica una patch modificata e non ancora salvata.
- Cambio patch senza buchi: le note tenute e il pedale restano sul suono di prima, le note nuove vanno al suono nuovo; il suono vecchio finisce la sua coda (3 secondi) e sfuma.
- Launchpad Mini MK1: griglia = patch del banco, tasti tondi in alto = banchi, a destra mute 1–4, precedente, successiva, TAP e PANIC (tenuto mezzo secondo).
- Tempo globale: INT (BPM a mano o TAP) oppure EXT (MIDI clock da un ingresso scelto in CLOCK DA). Arpeggiatori e delay a tempo dei plugin lo seguono.
- Limitatore di sicurezza sull'uscita a −1 dB.

Il prossimo passo è il supporto di Ableton Push 2 (pad colorati, pulsanti, manopole e schermo).

## Scaricare l'app

L'app viene compilata da GitHub Actions per Mac Intel (macOS 12 o successivo).

- Versioni pubblicate: pagina **Releases** del repository, file `MBStudio91-Stage-macOS-Intel.zip`.
- Ultima compilazione: scheda **Actions**, ultima esecuzione verde, sezione *Artifacts*.

Scompatta lo zip e trascina `MBStudio91 Stage.app` in Applicazioni.

## Primo avvio

L'app non è firmata con un certificato Apple, quindi al primo avvio macOS la blocca. Due modi per aprirla:

1. In Applicazioni fai clic destro su `MBStudio91 Stage` e scegli **Apri**, poi conferma.
2. Oppure dal Terminale:

   ```
   xattr -dr com.apple.quarantine "/Applications/MBStudio91 Stage.app"
   ```

Al primo avvio parte da sola la ricerca dei plugin. Con molti plugin installati può richiedere qualche minuto; la riga in basso mostra quando ha finito.

## Ableton Push 2

Il Push 2 funziona come la Launchpad, da selettore di patch, e in piu' ha il display.

| Sul Push | Cosa fa |
|---|---|
| i 64 pad | le 64 patch del banco mostrato, la prima in alto a sinistra |
| 8 tasti sopra il display | i banchi A-H |
| colonna a destra, dall'alto | mute delle quattro postazioni, patch precedente, successiva, TAP, PANIC |
| PANIC | si tiene premuto mezzo secondo |
| display | nome della patch, banco, numero, BPM e mute delle postazioni |

Stage apre la porta **User** del Push se c'e': e' quella che Live lascia libera, cosi'
le due cose convivono. Senza Live aperto va bene anche l'altra porta.

I colori dei pad non sono fissi sul Push, quindi Stage si definisce le sei voci di
palette che usa, dopo essersi fatto mandare quelle originali, e le rimette a posto
quando esce: Live non si ritrova colori strani.

Se il Push non e' collegato, il display va lo stesso sul visore di
`tools/push2-test` (porta 127.0.0.1:48484), comodo per lavorare senza hardware.

I VST suonano dal vivo e basta: Stage non registra e non crea tracce.

## Cartelle dei plugin

Oltre alle cartelle standard del sistema puoi farne battere altre, per esempio quella
dove compili i plugin MBStudio91. Il tasto SCANSIONA PLUGIN apre un menu con
"Cartelle in piu'...": una cartella per riga, oppure scegli con il selettore. Alle
AudioUnit non servono, le trova il sistema.

## Dove stanno i dati

Tutto in `~/Music/MBStudio91 Stage/`:

| File | Contenuto |
|---|---|
| `plugins.xml` | elenco dei plugin trovati |
| `macchina.settings` | scheda audio, frequenza e buffer |
| `scan-in-corso.txt` | file temporaneo della scansione |
| `tastiere.xml` | quale tastiera suona quale postazione |
| `postazioni.xml` | impostazioni correnti delle postazioni e del tempo |
| `libreria.xml` | le patch; ogni giorno una copia `libreria-AAAA-MM-GG.xml` |
| `cartelle-plugin.txt` | cartelle in piu' da battere nella scansione, una per riga |

Per rifare la ricerca da zero basta premere SCANSIONA PLUGIN. Per azzerare tutto, chiudi l'app e cancella `plugins.xml`.

## Prova del Push 2

In `tools/push2-test` c'e' un'app a parte per provare un Ableton Push 2 collegato:
legge firmware e numero di serie, accende pad e tasti, gestisce rotativi, striscia,
aftertouch e pedali, e scrive sul display. Insieme c'e' un visore che mostra il
display del Push sullo schermo, utile per lavorare senza l'hardware davanti.
Ha un workflow suo e non entra nell'app Stage. Vedi `tools/push2-test/README.md`.

## Compilare

Non serve compilare in locale: ogni push su GitHub avvia la compilazione e i test. Per chi volesse farlo comunque, servono CMake 3.22 e Xcode:

```
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target MBStudio91Stage --config Release
```

I test: `cmake --build build --target StageTests` e poi `ctest --test-dir build`.

## Licenze

Basato su [JUCE](https://juce.com) 8. Il font Bebas Neue (Dharma Type) è distribuito con licenza SIL Open Font License 1.1, testo in `Assets/Fonts/OFL.txt`.
