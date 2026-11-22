# MBStudio91 Push2 Test

App di prova per pilotare un Ableton Push 2 senza Live, in vista di MBStudio91 Stage.

Cosa fa:

- legge firmware, numero di serie e revisione della scheda (Identity Request);
- colora i 64 pad: la fila in basso ha un pad per ciascuno degli 8 setup, le altre righe prendono il colore del setup attivo;
- gli 8 tasti sopra il display e i pad in basso cambiano setup;
- un pad premuto diventa bianco finché lo tieni giù; in finestra compaiono nota e velocity;
- encoder e altri tasti compaiono in finestra come numero di CC;
- il display mostra le 8 caselle allineate ai tasti, il nome del setup attivo e il firmware;
- il nome al centro segue lo stile del logo MBSTUDIO91 (solo l'iniziale spessa); Sylenth1 e
  DUNE usano un carattere somigliante al loro marchio;
- accanto al nome compare la tastiera abbinata, scritta come sul pannello.

Usa solo comandi documentati da Ableton in
[push-interface](https://github.com/Ableton/push-interface). Prima di cambiare le voci 1-16
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

## Licenze

- Bebas Neue: lo stesso file di MBStudio91 Stage, SIL Open Font License (`Assets/Fonts/OFL.txt` nella radice).
- Comfortaa Light e Bold, per il nome in stile logo MBSTUDIO91: SIL Open Font License.
- Quantico Regular, per il nome di DUNE: SIL Open Font License.
- Yellowtail Regular, per il nome di Sylenth1: Apache License 2.0.
- libusb: LGPL 2.1.

Quantico e Yellowtail somigliano ai marchi di DUNE e Sylenth1, non sono i caratteri
originali, che non sono liberi. Il marchio MBSTUDIO91 e i nomi delle periferiche sono
disegnati con questi font, nessun file di terzi e' incluso.
