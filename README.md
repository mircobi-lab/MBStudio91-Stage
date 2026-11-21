# MBStudio91 Stage

Host per strumenti virtuali in stile MainStage, pensato per suonare i plugin MBStudio91, Sylenth1 e DUNE 2 in studio e in sala prove con quattro tastiere e una Launchpad.

Il progetto è descritto in `docs/superpowers/specs/2026-11-21-mbstudio91-stage-design.md`. Lo sviluppo procede a fasi; questa è la **Fase 1 (motore)**.

## Cosa fa la Fase 1

- Apre la scheda audio scelta nelle impostazioni e se la ricorda al riavvio.
- Cerca i plugin VST3 e AU installati. La ricerca gira in un processo separato: se un plugin si blocca o va in crash durante la scansione, viene scartato e l'app resta aperta.
- Carica sulla postazione 1 uno strumento e fino a tre effetti, con volume e mute.
- Apre la finestra originale di ogni plugin con APRI PLUGIN.
- Suona da tutte le tastiere MIDI collegate (la Launchpad è esclusa: servirà per le patch dalla Fase 3).
- PANIC ferma subito tutte le note, anche con il pedale premuto.

Le postazioni 2, 3 e 4, le patch, la Launchpad e il tempo arrivano nelle fasi successive.

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

## Dove stanno i dati

Tutto in `~/Music/MBStudio91 Stage/`:

| File | Contenuto |
|---|---|
| `plugins.xml` | elenco dei plugin trovati |
| `macchina.settings` | scheda audio, frequenza e buffer |
| `scan-in-corso.txt` | file temporaneo della scansione |

Per rifare la ricerca da zero basta premere SCANSIONA PLUGIN. Per azzerare tutto, chiudi l'app e cancella `plugins.xml`.

## Compilare

Non serve compilare in locale: ogni push su GitHub avvia la compilazione e i test. Per chi volesse farlo comunque, servono CMake 3.22 e Xcode:

```
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target MBStudio91Stage --config Release
```

I test: `cmake --build build --target StageTests` e poi `ctest --test-dir build`.

## Licenze

Basato su [JUCE](https://juce.com) 8. Il font Bebas Neue (Dharma Type) è distribuito con licenza SIL Open Font License 1.1, testo in `Assets/Fonts/OFL.txt`.
