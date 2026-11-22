# MBStudio91 Push2 Test

App di prova per pilotare un Ableton Push 2 senza Live, in vista di MBStudio91 Stage.

Cosa fa:

| Controllo sul Push 2 | MIDI | Funzione |
|---|---|---|
| 8 rotativi in alto | CC 71-78 | volume del setup sotto, uno scatto 1% |
| tocco di un rotativo | note 0-7 | senza girarlo blocca o sblocca quel volume |
| 8 tasti sopra il display | CC 102-109 | seleziona il setup |
| 8 tasti sotto il display | CC 20-27 | mute del setup di quella colonna |
| colonna a destra dei pad | CC 43 in alto ... 36 in basso | richiama una scena, con Shift la memorizza |
| Shift | CC 49 | modificatore |
| encoder Master | CC 79 | volume generale |
| tasto Master | CC 28 | mute generale |
| striscia a sinistra | nota 12, pitch bend o CC 1 | pitch bend oppure mod wheel |
| Layout | CC 31 | commuta la striscia fra pitch bend e mod wheel |
| pad | note 36-99 + aftertouch | note, con pressione mostrata a schermo |
| Accent | CC 57 | aftertouch globale o per singolo pad |
| pedale jack 1 (destro) | CC 64 | sustain |
| pedale jack 2 (sinistro) | CC 69 | volume a pedale se e' di espressione, mute se e' a interruttore |
| frecce su/giu | CC 46/47 | setup precedente e successivo |
| frecce sinistra/destra | CC 44/45 | scena precedente e successiva |
| Octave su/giu | CC 55/54 | traspone il setup attivo, fino a 4 ottave |
| encoder Tempo | CC 14 | BPM |
| Tap Tempo | CC 3 | batte il BPM a mano |
| Metronome, Play, Stop | CC 9, 85, 29 | accende e spegne il lampeggio a tempo |
| Mute, Solo | CC 60, 61 | mute e solo del setup attivo |

Una scena e' una istantanea degli otto volumi e dei mute. Un volume bloccato non lo
muove nemmeno una scena richiamata: resta dove l'hai messo.

Il display mostra in alto le otto caselle con nome, barra del volume, lucchetto se il
volume e' bloccato e nome barrato se il setup e' in mute; al centro il setup attivo con
la tastiera abbinata, oppure il volume del rotativo che stai tenendo; poi una riga di
stato con BPM, master, scena, striscia, aftertouch e pedali; in fondo il marchio.

Usa solo comandi documentati da Ableton in
[push-interface](https://github.com/Ableton/push-interface): numeri di pad, tasti e
rotativi da `doc/Push2-map.json` (rotativi CC 71-78, tocco note 0-7, tasti sopra il
display CC 102-109), valore relativo dei rotativi in complemento a due. Prima di cambiare le voci 1-16
della palette colori le legge dal Push e le rimette a posto quando chiudi l'app.

## Come usarla

1. Chiudi Ableton Live: tiene occupato il display e manderebbe i suoi colori ai pad.
2. Apri `MBStudio91 Push2 Test.app`. La prima volta macOS può bloccarla perché non è firmata:
   tasto destro sull'app, Apri, poi Apri di nuovo.
3. In finestra controlla le tre righe: MIDI, Display, Firmware.

Con la sola alimentazione USB LED e display restano tenui: è il Push che si limita per stare nei 500 mA.

## Compilazione

Il workflow `.github/workflows/push2-test.yml` compila su macOS Intel (x86_64, macOS 12 o successivo)
a ogni push che tocca questa cartella e mette lo zip negli artifact del run "Push2 Test".
Con un tag `push2-test-v*` lo zip finisce anche in una Release. Si può avviare a mano da Actions.

Dipendenze scaricate da CMake: JUCE 8.0.15 e libusb (tramite libusb-cmake, compilata statica).

Le prove del protocollo girano anche senza JUCE (dalla cartella `tools/push2-test`):

```
cmake -S . -B build -DPUSH2_ONLY_TESTS=ON && cmake --build build && ctest --test-dir build
```

## Icona

`Assets/brand/icon.png` (1024x1024) e' generata da `Assets/brand/make_icon.py`:
telaio nero con angoli alla macOS, display incassato col marchio MBSTUDIO91 stampato
e tre file di pad, l'ultima accesa coi colori dei LED MBStudio (blu, verde, ambra,
rosso). Per rifarla: `python3 Assets/brand/make_icon.py`. CMake la passa a JUCE con
`ICON_BIG` e il bundle .app si prende l'Icon.icns da sola.

## Licenze

- Bebas Neue: lo stesso file di MBStudio91 Stage, SIL Open Font License (`Assets/Fonts/OFL.txt` nella radice).
- Comfortaa Light e Bold, per il nome in stile logo MBSTUDIO91: SIL Open Font License.
- Quantico Regular, per il nome di DUNE: SIL Open Font License.
- Yellowtail Regular, per il nome di Sylenth1: Apache License 2.0.
- libusb: LGPL 2.1.

Quantico e Yellowtail somigliano ai marchi di DUNE e Sylenth1, non sono i caratteri
originali, che non sono liberi. Il marchio MBSTUDIO91 e i nomi delle periferiche sono
disegnati con questi font, nessun file di terzi e' incluso.
