# MBStudio91 Stage — documento di progetto

Data: 21 novembre 2026
Autore delle decisioni: Mirco (MBStudio91). Le scelte marcate **[proposta]** sono state fatte durante la progettazione e Mirco può correggerle.

## 1. Perché

Mirco vuole un'unica applicazione in stile MainStage per suonare i suoi strumenti virtuali in **studio e in sala prove dal vivo**: quattro tastiere collegate contemporaneamente, ciascuna con il suo suono, e una Launchpad per cambiare le patch al volo senza aprire una DAW.

MainStage non va bene sul suo hardware. Secondo l'App Store, la versione attuale richiede macOS 15.6 e la versione in abbonamento richiede un Mac Apple Silicon.

**Macchina di destinazione: solo il MacBook Pro 15" 2019** (Intel i9 8 core, 32 GB di RAM), usato in casa per le prove. Il Mac mini resta dedicato ad Ableton e per ora non è un obiettivo.

Il successo, per lui, è questo: accendere il Mac, aprire Stage, premere un pad e avere le quattro tastiere pronte con i suoni giusti, poi passare alla patch successiva mentre suona, senza buchi audio e senza note appese.

## 2. Obiettivi

1. Ospitare plugin di strumento ed effetto **VST3 e AU** su macOS Intel.
2. Plugin che devono funzionare di sicuro: tutti i plugin MBStudio91 (JUCE, VST3/AU: acido, MuMU, ICX7, Campione, LA-91/D-50 e i successivi), **Sylenth1** e **DUNE 2**.
3. **Quattro postazioni fisse**, una per tastiera fisica:

   | Postazione | Tastiera | Uso previsto |
   |---|---|---|
   | 1 | M-Audio Keystation 88 MK3 (88 tasti semi-pesati, non a martelletti) | pianoforti |
   | 2 | Novation Launchkey 61 MK2 | synth |
   | 3 | Novation Launchkey Mini MK2 | synth |
   | 4 | Novation Launchkey Mini MK1 | synth |

4. Una **patch** (o scena) memorizza per ogni postazione lo **stato completo** dello strumento e degli effetti, senza affidarsi al Program Change MIDI. Il motivo: funziona con qualsiasi plugin, comunque gestisca i propri preset.
5. Il **cambio patch dalla Launchpad Mini MK1** avviene senza interruzioni: le note tenute e le code del suono precedente finiscono in modo naturale, mentre le nuove note suonano già con la nuova patch.
6. **MIDI PANIC** sempre disponibile, sullo schermo e sulla Launchpad.
7. Interfaccia nello **stile hardware MBStudio91**: telaio nero, scritte bianche, LED, logo MBSTUDIO91™ impresso, font Bebas Neue (grassetto e sottile). Vedi la skill `mbstudio-hardware-ui`.
8. **Compilazione su GitHub Actions** con l'app pronta da scaricare. Mirco non compila in locale.
9. Gira su **Intel x86_64**, sul MacBook Pro 2019. **[proposta]** Il requisito minimo resta macOS 12, perché non costa nulla e lascia aperta la porta al Mac mini in futuro.
10. **Tempo (BPM) globale, con due sorgenti selezionabili**:
    - **Libero (interno)**: il BPM si imposta a mano e vale per tutte le patch, che non lo cambiano. Si regola con un campo o una manopola sullo schermo e con il **tap tempo** sul tasto tondo 7 della Launchpad **[proposta]**.
    - **MIDI clock esterno**: l'host segue il clock (24 impulsi per quarto) che arriva da un ingresso MIDI scelto nelle impostazioni, per esempio una drum machine o un altro strumento in sala. Start, Stop, Continue e Song Position Pointer muovono il trasporto. Il tempo ricavato va stabilizzato (media mobile) per non far tremolare i plugin. Se il clock si interrompe, l'host resta sull'ultimo BPM ricevuto e lo segnala.
    In entrambi i casi l'host passa tempo e posizione ai plugin (arpeggiatori, delay e LFO a tempo). In modalità libera il trasporto risulta sempre "in riproduzione".

## 3. Cosa resta fuori

**Non obiettivi della v1**
- Scaletta di brani e modalità concerto: si usa in studio e in sala prove.
- Basi, playback, registrazione e metronomo con audio.
- Layer e split di più strumenti sulla stessa tastiera: rimandati a una versione successiva. La struttura dei dati deve però permetterli senza rifare tutto (vedi 5.2).
- Windows, iPad e Apple Silicon nativo (l'app può essere universal se costa poco, ma non è un requisito).
- Importare concerti di MainStage.
- Usare Launchkey e Keystation in modalità InControl/DAW: in v1 vengono lette in modalità MIDI base.
- Invio del MIDI clock verso l'esterno (l'host lo riceve soltanto).
- Supporto ottimizzato per il Mac mini dual core.

**Cose che renderebbero il progetto un fallimento, anche se "funziona"**
- Un plugin che va in crash e si porta via tutta l'app durante una prova.
- Clic, silenzi o buchi audio al cambio patch.
- Note appese dopo il cambio patch o dopo un PANIC.
- Una scansione dei plugin che blocca l'avvio per minuti ogni volta.
- Un cambio patch che dipende dal mouse: tutto il necessario in sala prove si deve fare dalla Launchpad.

## 4. Approcci considerati

| | Approccio | Pro | Contro |
|---|---|---|---|
| **A (scelto)** | Host proprio "MBStudio91 Stage" in JUCE | Interfaccia e comportamento esattamente come li vuole Mirco, gira su Intel e Monterey, stessa base tecnica dei suoi plugin, compilazione su GitHub Actions | Progetto lungo da costruire a fasi, v1 essenziale |
| B | Host commerciale (Gig Performer, Camelot…) | Subito pronto, collaudato | A pagamento, interfaccia non sua, Launchpad MK1 da mappare a mano |
| C | Set di Ableton Live 11 con rack | Nessun codice | Pesante, cambi patch meno puliti, richiede Live aperto |

Mirco ha scelto **A**.

## 5. Come funziona

### 5.1 Architettura

- **Framework**: JUCE 8 con CMake, app standalone per macOS (non un plugin). **[proposta]** Nome dell'app: *MBStudio91 Stage*. Nome del repo: `mircobi-lab/MBStudio91-Stage`.
- **Motore audio**: un `AudioProcessorGraph`, o un grafo equivalente scritto a mano, con 4 postazioni e un bus master.
- **Catena di una postazione**: strumento → fino a **3 effetti** **[proposta]** → volume di postazione → master.
- **Master**: volume generale e un limitatore di sicurezza leggero **[proposta]**, che evita i picchi quando una patch arriva troppo forte.
- **Uscita audio**: dispositivo, frequenza di campionamento e buffer si scelgono nelle impostazioni. Uscita stereo unica.

### 5.2 Modello dei dati

```
Libreria
 └─ Banco A…H (8 banchi)
     └─ Patch 1…64 (griglia 8×8 della Launchpad)
         ├─ nome, colore/etichetta
         └─ Postazione 1…4
             ├─ strumento: id plugin + stato binario (getStateInformation)
             ├─ effetti[0..3]: id plugin + stato
             ├─ volume, mute
             ├─ trasposizione (semitoni), estensione tasti (nota bassa/alta)
             ├─ curva di velocity (lineare, morbida, dura, personalizzata)
             └─ mappature manopole/fader → parametri (per patch)
```

- Le postazioni sono una lista: aggiungere più strati per tastiera in futuro deve voler dire "più elementi nella lista", senza cambiare il formato del file.
- **Salvataggio**: un file di libreria (JSON con gli stati in base64, oppure il formato XML/ValueTree di JUCE; la scelta spetta al costruttore) in `~/Music/MBStudio91 Stage/`. Copia di sicurezza con la data a ogni avvio. Esportazione e importazione della libreria intera o di un singolo banco.
- **Ritocchi durante l'esecuzione [proposta]**: se Mirco gira una manopola o modifica un plugin mentre suona, la modifica resta valida finché non cambia patch. La patch viene segnata come "modificata" con un LED o un asterisco. Si scrive nella libreria solo con **[SALVA]**. Richiamare una patch ricarica sempre lo stato salvato. Il salvataggio automatico riguarda solo le modifiche della libreria in sé (nomi, copie, spostamenti, impostazioni di postazione fatte dalla schermata di modifica).
- **Libreria e impostazioni della macchina separate [proposta]**: assegnazione delle tastiere, elenco dei plugin scansionati e dispositivo audio stanno in un file di impostazioni locale, diverso per ogni Mac. La libreria contiene solo le patch, così resta portabile se un giorno servirà su un altro Mac. I plugin si identificano con un ID portabile (formato + produttore + nome + identificativo univoco). Se sull'altro Mac il plugin manca, la postazione lo segnala senza bloccare la patch.

### 5.3 Instradamento MIDI

- Ogni tastiera fisica viene associata alla sua postazione tramite l'**identificativo del dispositivo MIDI**, non solo tramite il nome: le due Launchkey Mini potrebbero avere nomi simili. Nelle impostazioni c'è il pulsante "Assegna: premi un tasto sulla tastiera", con cui la postazione impara il dispositivo.
- L'associazione resta valida dopo il riavvio. Se una tastiera manca, la postazione lo segnala (LED spento) senza errori.
- Note, pitch bend, modulazione, sustain (CC64) ed espressione passano allo strumento della postazione, dopo trasposizione, filtro sull'estensione e curva di velocity.
- **Keystation 88 MK3**: il suo fader controlla il volume della postazione 1 **[proposta]**. La curva di velocity si regola per patch, per compensare la tastiera semi-pesata.
- **Manopole e fader delle Launchkey**: MIDI learn per patch. Si clicca un parametro del plugin o un controllo di postazione, poi si muove la manopola. I numeri di CC predefiniti di ciascun modello vanno verificati sul dispositivo con un monitor MIDI e non dati per scontati. Se esistono già, conviene riusare i profili Novation/M-Audio previsti per i plugin MBStudio91.
- I pad delle Launchkey in v1 non hanno funzioni **[proposta]**, oppure possono passare note allo strumento: decide il costruttore.

### 5.4 Launchpad Mini MK1 (LED rosso/verde, protocollo "classico")

Disposizione **[proposta]**:

```
 [A][B][C][D][E][F][G][H]      ← 8 tasti tondi in alto: banchi A…H
 [ ][ ][ ][ ][ ][ ][ ][ ] (1)  ← tasti tondi a destra:
 [ ][ ][ ][ ][ ][ ][ ][ ] (2)     1–4 = mute postazioni 1–4
 [ ][ ][ ][ ][ ][ ][ ][ ] (3)     5   = patch precedente
 [ ][ ][ ][ ][ ][ ][ ][ ] (4)     6   = patch successiva
 [ ][ ][ ][ ][ ][ ][ ][ ] (5)     7   = TAP TEMPO (lampeggia a tempo)
 [ ][ ][ ][ ][ ][ ][ ][ ] (6)     8   = PANIC (tenere premuto 0,5 s)
 [ ][ ][ ][ ][ ][ ][ ][ ] (7)
 [ ][ ][ ][ ][ ][ ][ ][ ] (8)   griglia 8×8 = 64 patch del banco attivo
```

Colori dei LED (la Launchpad MK1 mescola rosso e verde):
- spento = patch vuota;
- ambra = patch salvata;
- verde = patch attiva;
- rosso lampeggiante = patch in caricamento;
- banco attivo = verde, altri banchi con patch = ambra;
- mute attivo = rosso.

Protocollo: note-on con il colore nella velocity (velocity = 16 × verde + rosso + flag; 12 per l'uso normale), tasti tondi in alto su CC 104–111, griglia in layout X-Y. Fonte: Novation, *Launchpad Programmer's Reference*. Il costruttore deve verificarlo sul dispositivo reale e usare il double buffering se serve a evitare sfarfallii.

La Launchpad deve funzionare anche se viene collegata dopo l'avvio dell'app (rilevamento a caldo).

### 5.5 Cambio patch senza interruzioni

Requisito: nessun buco audio, nessuna nota appesa, la coda del suono precedente continua.

Approccio **[proposta]**, simile a MainStage: ogni postazione ha **due slot** (A/B).
1. Al cambio patch, lo slot libero riceve lo strumento e gli effetti della nuova patch. Se il plugin è già caricato, riceve solo lo stato; se no, viene caricato in un thread separato e il LED lampeggia in rosso.
2. Le **nuove** note vanno allo slot nuovo. Le note ancora premute e il pedale di sustain restano sul vecchio slot finché non vengono rilasciati.
3. Il vecchio slot continua a suonare la coda, poi sfuma e viene liberato dopo un tempo massimo regolabile (predefinito 3 s).
4. Il caricamento di un plugin non deve mai bloccare il thread audio.

**Obiettivi di tempo**:
- patch che usano plugin già caricati: suono nuovo disponibile entro circa 100 ms dalla pressione del pad;
- plugin da caricare: il più velocemente possibile, con indicazione visiva fino a quando la patch è pronta.

Il costruttore può tenere in memoria i plugin già usati (cache delle istanze) per rendere i cambi successivi immediati. C'è margine: il MacBook ha 32 GB di RAM e 8 core.

### 5.6 Stabilità dei plugin

- **Scansione dei plugin in un processo separato**, con elenco dei plugin "sospetti" (meccanismo dead-man's-pedal di JUCE): un plugin che fa crashare la scansione viene saltato la volta successiva.
- La scansione si fa solo su richiesta o quando compaiono plugin nuovi, non a ogni avvio.
- **[proposta]** In v1 i plugin girano nello stesso processo dell'app; l'isolamento di ogni plugin in un processo separato è fuori dalla v1. Se un plugin non si carica, la postazione mostra l'errore e il resto continua a funzionare.

### 5.7 Interfaccia

Schermata principale (bozza):

```
┌ MBSTUDIO91™ STAGE ───────────── BANCO A · PATCH 03 "Fusion Rhodes" ── [PANIC] ┐
│ ① KEYSTATION 88  │ ② LAUNCHKEY 61  │ ③ MINI MK2      │ ④ MINI MK1          │
│ ● Campione Piano │ ● DUNE 2        │ ● Sylenth1      │ ○ —                 │
│ FX: Reverb       │ FX: Delay, Chor │ FX: —           │                     │
│ VOL ▮▮▮▮▮▯  MUTE │ VOL ▮▮▮▮▯▯ MUTE │ VOL ▮▮▮▯▯▯ MUTE │                     │
│ Curva: morbida   │ C1–B3  ±0       │ +12 st          │                     │
│ [APRI PLUGIN]    │ [APRI PLUGIN]   │ [APRI PLUGIN]   │                     │
├──────────────────┴─────────────────┴─────────────────┴─────────────────────┤
│ BANCHI [A●][B][C][D][E][F][G][H]                                           │
│ PATCH  griglia 8×8 come sulla Launchpad (clic = richiama, [SALVA] = salva)  │
├────────────────────────────────────────────────────────────────────────────┤
│ CPU ▮▮▯▯  · ♩ 120 BPM [INT|EXT] [TAP] · audio 48 kHz / 128 · MIDI ● ● ● ● ● LP                     MASTER ▮▮▮▮▯ │
└────────────────────────────────────────────────────────────────────────────┘
```

- **[APRI PLUGIN]** apre l'interfaccia originale del plugin in una finestra separata.
- Si salva con **[SALVA]**: la patch corrente, compresi gli eventuali ritocchi, va nel pad selezionato, con la richiesta di conferma prima di sovrascrivere (vedi 5.2).
- Le patch si possono rinominare, copiare e incollare, anche da un banco all'altro.
- I LED MIDI di ogni tastiera e della Launchpad lampeggiano quando arriva un segnale. In sala prove è il modo più rapido per capire se un cavo è staccato.
- È disponibile la **modalità a schermo intero** con caratteri grandi, leggibile a distanza.

## 6. Vincoli

- Intel x86_64, MacBook Pro 2019 (macOS installato su di esso; requisito minimo dichiarato macOS 12).
- Compilazione solo su GitHub Actions. Il workflow produce uno zip o un dmg scaricabile negli artifact e nelle Release. L'app non è firmata con un certificato Apple: le istruzioni di primo avvio (clic destro → Apri, oppure `xattr`) vanno nel README in italiano.
- I plugin di terze parti (Sylenth1, DUNE 2) non si possono includere né testare su GitHub: i test automatici usano i plugin MBStudio91 o plugin liberi, mentre la prova con Sylenth1 e DUNE 2 la fa Mirco sul suo Mac.
- Stile grafico: skill `mbstudio-hardware-ui`, Bebas Neue incorporato con licenza OFL.

## 7. Fasi proposte **[proposta]**

1. **Motore**: host con 1 postazione, scansione dei plugin, caricamento VST3/AU, uscita audio, PANIC. Build su Actions.
2. **4 postazioni e MIDI**: assegnazione delle tastiere, trasposizione, estensione, curve di velocity.
3. **Patch e Launchpad**: libreria, banchi, salvataggio, cambio senza interruzioni, LED, tempo libero/tap e MIDI clock esterno.
4. **Interfaccia MBStudio91** completa, schermo intero, MIDI learn per manopole e fader.
5. Rifiniture e prova in sala prove.

## 8. Lasciato al costruttore

- Formato esatto del file di libreria, struttura delle classi, uso di `AudioProcessorGraph` o di un grafo proprio.
- Strategia di cache delle istanze, nel rispetto della RAM del MacBook.
- Tipo e regolazione del limitatore master.
- Dettagli del protocollo della Launchpad e numeri di CC delle Launchkey, da verificare sui dispositivi.
- Uso dei pad delle Launchkey in v1.
