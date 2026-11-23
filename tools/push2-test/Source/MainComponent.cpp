#include "MainComponent.h"
#include "Push2Protocol.h"

#include <cmath>

#include "BinaryData.h"

namespace
{
const juce::Colour frameBlack { 0xff0b0b0b };
const juce::Colour panelGrey { 0xff8a8a8a };
} // namespace

MainComponent::MainComponent()
{
    // Abbinamenti plugin/periferica provvisori: i pianoforti sulla Keystation 88
    enum { lkMini1, lkMini2, lk61, ks88 };
    setups = { { { "Piano",    juce::Colour (0xfff2e6cc), ks88 },
                 { "Rhodes",   juce::Colour (0xffff8c1a), lk61 },
                 { "Sylenth1", juce::Colour (0xff1ad1ff), lk61,    TextPart::Face::script },
                 { "DUNE 2",   juce::Colour (0xff9b4dff), lkMini2, TextPart::Face::square },
                 { "Acido",    juce::Colour (0xffb8ff1a), lkMini1 },
                 { "ICX7",     juce::Colour (0xff1a5cff), lk61 },
                 { "MuMU",     juce::Colour (0xffff2a1a), lkMini2 },
                 { "LA-91",    juce::Colour (0xffff1aa8), lk61 } } };

    bebas = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf,
                                                     BinaryData::BebasNeueRegular_ttfSize);
    faces.bebas = bebas;
    faces.logoLight = juce::Typeface::createSystemTypefaceFor (BinaryData::ComfortaaLight_ttf, BinaryData::ComfortaaLight_ttfSize);
    faces.logoBold  = juce::Typeface::createSystemTypefaceFor (BinaryData::ComfortaaBold_ttf, BinaryData::ComfortaaBold_ttfSize);
    faces.script = juce::Typeface::createSystemTypefaceFor (BinaryData::YellowtailRegular_ttf, BinaryData::YellowtailRegular_ttfSize);
    faces.square = juce::Typeface::createSystemTypefaceFor (BinaryData::QuanticoRegular_ttf, BinaryData::QuanticoRegular_ttfSize);
    faces.logo = juce::ImageCache::getFromMemory (BinaryData::mbstudio91_wordmark_png, BinaryData::mbstudio91_wordmark_pngSize);
    auto font = [this] (float h) { return juce::Font (juce::FontOptions().withTypeface (bebas).withHeight (h)); };

    title.setText ("MBSTUDIO91 STAGE  -  TEST PUSH 2", juce::dontSendNotification);
    title.setFont (font (34.0f));
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (title);

    for (auto* l : { &midiStatus, &displayStatus, &firmwareStatus, &eventLabel })
    {
        l->setFont (font (20.0f));
        l->setColour (juce::Label::textColourId, panelGrey);
        addAndMakeVisible (*l);
    }

    for (int i = 0; i < numSetups; ++i)
    {
        auto& b = setupButtons[(size_t) i];
        b.setButtonText (setups[(size_t) i].name);
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff161616));
        b.setColour (juce::TextButton::buttonOnColourId, setups[(size_t) i].colour.darker (0.3f));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.onClick = [this, i] { selectSetup (i); };
        addAndMakeVisible (b);
    }

    volume.fill (0.75f);

    setSize (760, 380);
    selectSetup (0);
    updateStatus();
    startTimer (100);
}

MainComponent::~MainComponent()
{
    stopTimer();
    disconnectMidi (true);
}

// ---------------------------------------------------------------- MIDI

bool MainComponent::connectMidi()
{
    auto pick = [] (const juce::Array<juce::MidiDeviceInfo>& list) -> std::optional<juce::MidiDeviceInfo>
    {
        std::optional<juce::MidiDeviceInfo> any;
        for (const auto& d : list)
        {
            if (! d.name.containsIgnoreCase ("Push 2"))
                continue;
            if (d.name.containsIgnoreCase ("Live"))
                return d;          // porta 1: e' quella attiva quando il Push e' in modalita' Live
            if (! any) any = d;
        }
        return any;
    };

    auto in = pick (juce::MidiInput::getAvailableDevices());
    auto out = pick (juce::MidiOutput::getAvailableDevices());
    if (! in || ! out)
        return false;

    midiOut = juce::MidiOutput::openDevice (out->identifier);
    midiIn = juce::MidiInput::openDevice (in->identifier, this);
    if (midiIn == nullptr || midiOut == nullptr)
    {
        midiIn.reset();
        midiOut.reset();
        return false;
    }

    midiPortName = out->name;
    midiIn->start();

    paletteBackup.clear();
    paletteApplied = false;
    paletteWaitTicks = 0;
    firmware.reset();

    sendSysex (push2::identityRequest());
    configurePush();
    // Salva le voci di palette che andiamo a cambiare, per rimetterle a posto all'uscita
    for (int i = firstPaletteIndex; i < firstPaletteIndex + 2 * numSetups; ++i)
        sendSysex ({ 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x04, (std::uint8_t) i, 0xF7 });

    return true;
}

void MainComponent::disconnectMidi (bool restore)
{
    if (midiOut != nullptr && restore)
    {
        allLedsOff();
        restorePalette();
    }
    if (midiIn != nullptr)
        midiIn->stop();
    midiIn.reset();
    midiOut.reset();
    midiPortName.clear();
    firmware.reset();
}

void MainComponent::send (const juce::MidiMessage& m)
{
    if (midiOut != nullptr)
        midiOut->sendMessageNow (m);
}

void MainComponent::sendSysex (const std::vector<std::uint8_t>& bytes)
{
    // JUCE vuole il sysex senza F0 e F7
    if (bytes.size() >= 2)
        send (juce::MidiMessage::createSysExMessage (bytes.data() + 1, (int) bytes.size() - 2));
}

void MainComponent::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::MessageManager::callAsync ([safe, m]
    {
        if (safe != nullptr)
            safe->handleMessage (m);
    });
}

void MainComponent::handleMessage (const juce::MidiMessage& m)
{
    if (m.isSysEx())
    {
        const auto* d = m.getSysExData();
        const int n = m.getSysExDataSize();

        if (auto id = push2::parseIdentityReply (d, n))
        {
            firmware = juce::String (id->major) + "." + juce::String (id->minor) + "." + juce::String (id->build)
                     + "   seriale " + juce::String ((juce::int64) id->serial)
                     + "   scheda rev. " + juce::String (id->boardRevision);
            updateStatus();
            renderDisplay();
            return;
        }

        // Risposta a "Get LED Color Palette Entry": 00 21 1D 01 01 04 i r r g g b b w w
        if (n == 13 && d[0] == 0x00 && d[1] == 0x21 && d[2] == 0x1D && d[5] == 0x04)
        {
            std::vector<std::uint8_t> entry { 0xF0 };
            entry.insert (entry.end(), d, d + n);
            entry.push_back (0xF7);
            entry[6] = 0x03; // stessa voce, comando "Set"
            paletteBackup[d[6]] = entry;
        }
        return;
    }

    // Striscia a sinistra dei pad: nota 12 al tocco, poi pitch bend o CC 1
    if ((m.isNoteOn() || m.isNoteOff()) && m.getNoteNumber() == push2::touchStripNote)
    {
        screen.stripTouched = m.isNoteOn() && m.getVelocity() > 0;
        if (! screen.stripTouched && ! screen.stripMod)
            screen.strip = 0.5f; // in modo pitch bend la striscia torna al centro da sola
        lastEvent = juce::String ("Striscia ") + (screen.stripTouched ? "toccata" : "lasciata");
        renderDisplay();
        updateStatus();
        return;
    }

    if (m.isPitchWheel())
    {
        screen.strip = juce::jlimit (0.0f, 1.0f, (float) m.getPitchWheelValue() / 16383.0f);
        renderDisplay();
        return;
    }

    if (m.isChannelPressure() || m.isAftertouch())
    {
        screen.aftertouch = m.isChannelPressure() ? m.getChannelPressureValue() : m.getAfterTouchValue();
        renderDisplay();
        return;
    }

    // Tocco sui rotativi: note 0..7. Un tocco senza giro blocca o sblocca il volume.
    const int note0 = m.getNoteNumber() - push2::trackEncoderTouchNote;
    if ((m.isNoteOn() || m.isNoteOff()) && note0 >= 0 && note0 < push2::numTrackEncoders
        && note0 < numSetups)
    {
        if (m.isNoteOn() && m.getVelocity() > 0)
        {
            touchedEncoder = note0;
            encoderMoved[(size_t) note0] = false;
            lastEvent = "Rotativo " + juce::String (note0 + 1) + " toccato";
        }
        else
        {
            if (! encoderMoved[(size_t) note0])
            {
                volumeLocked[(size_t) note0] = ! volumeLocked[(size_t) note0];
                lastEvent = "Volume " + setups[(size_t) note0].name + " "
                          + (volumeLocked[(size_t) note0] ? "bloccato" : "sbloccato");
            }
            touchedEncoder = -1;
        }
        renderDisplay();
        updateStatus();
        return;
    }

    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        if (auto pos = push2::padPosition (note))
        {
            padDown[(size_t) note] = true;
            lastEvent = "Pad riga " + juce::String (pos->first + 1) + ", colonna " + juce::String (pos->second + 1)
                      + "   nota " + juce::String (note) + "   velocity " + juce::String (m.getVelocity());
            if (pos->first == 7)
                selectSetup (pos->second);
            else
                refreshLeds();
            renderDisplay();
            updateStatus();
        }
        return;
    }

    if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
    {
        const int note = m.getNoteNumber();
        if (note >= 0 && note < 128 && padDown[(size_t) note])
        {
            padDown[(size_t) note] = false;
            refreshLeds();
        }
        return;
    }

    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int v = m.getControllerValue();

        // Rotativi: volume del setup che hanno sopra, uno scatto = 1%
        const int enc = cc - push2::trackEncoderCC;
        if (enc >= 0 && enc < push2::numTrackEncoders && enc < numSetups)
        {
            encoderMoved[(size_t) enc] = true;
            if (volumeLocked[(size_t) enc])
            {
                lastEvent = "Volume " + setups[(size_t) enc].name + " bloccato";
            }
            else
            {
                volume[(size_t) enc] = juce::jlimit (0.0f, 1.0f,
                                                     volume[(size_t) enc] + 0.01f * (float) push2::encoderDelta (v));
                lastEvent = "Volume " + setups[(size_t) enc].name + " "
                          + juce::String (juce::roundToInt (volume[(size_t) enc] * 100.0f)) + "%";
            }
            renderDisplay();
            updateStatus();
            return;
        }

        if (cc >= push2::upperDisplayButtonCC && cc < push2::upperDisplayButtonCC + numSetups)
        {
            if (v > 0)
                selectSetup (cc - push2::upperDisplayButtonCC);
            return;
        }

        // Fila sotto il display: mute del setup di quella colonna
        if (cc >= push2::lowerDisplayButtonCC && cc < push2::lowerDisplayButtonCC + numSetups)
        {
            if (v > 0)
            {
                const auto i = (size_t) (cc - push2::lowerDisplayButtonCC);
                muted[i] = ! muted[i];
                lastEvent = setups[i].name + (muted[i] ? " in mute" : " acceso");
                refreshLeds();
                renderDisplay();
                updateStatus();
            }
            return;
        }

        // Colonna a destra dei pad: richiama una scena, con Shift la memorizza
        for (int row = 0; row < push2::numSceneButtons; ++row)
            if (cc == push2::sceneButtonCC (row))
            {
                if (v > 0)
                {
                    if (screen.shift)
                        storeScene (row);
                    else
                        recallScene (row);
                }
                return;
            }

        switch (cc)
        {
            case push2::shiftCC:
                screen.shift = v > 0;
                renderDisplay();
                return;

            case push2::masterEncoderCC:
                screen.master = juce::jlimit (0.0f, 1.0f, screen.master + 0.01f * (float) push2::encoderDelta (v));
                lastEvent = "Master " + juce::String (juce::roundToInt (screen.master * 100.0f)) + "%";
                renderDisplay();
                updateStatus();
                return;

            case push2::masterCC:
                if (v > 0)
                {
                    screen.masterMuted = ! screen.masterMuted;
                    lastEvent = screen.masterMuted ? "Master in mute" : "Master acceso";
                    refreshLeds();
                    renderDisplay();
                    updateStatus();
                }
                return;

            case push2::tempoEncoderCC:
                screen.bpm = juce::jlimit (20, 300, screen.bpm + push2::encoderDelta (v));
                lastEvent = "BPM " + juce::String (screen.bpm);
                renderDisplay();
                updateStatus();
                return;

            case push2::tapTempoCC:
                if (v > 0)
                {
                    const auto now = juce::Time::currentTimeMillis();
                    const auto gap = now - lastTap;
                    if (lastTap > 0 && gap > 200 && gap < 3000)
                    {
                        screen.bpm = juce::jlimit (20, 300, (int) juce::roundToInt (60000.0 / (double) gap));
                        lastEvent = "BPM " + juce::String (screen.bpm) + " battuto a mano";
                    }
                    lastTap = now;
                    renderDisplay();
                    updateStatus();
                }
                return;

            case push2::metronomeCC:
            case push2::playCC:
                if (v > 0)
                {
                    screen.metronome = cc == push2::playCC ? true : ! screen.metronome;
                    refreshLeds();
                    renderDisplay();
                }
                return;

            case push2::stopCC:
                if (v > 0)
                {
                    screen.metronome = false;
                    screen.beat = false;
                    refreshLeds();
                    renderDisplay();
                }
                return;

            case push2::muteCC:
                if (v > 0)
                {
                    muted[(size_t) activeSetup] = ! muted[(size_t) activeSetup];
                    refreshLeds();
                    renderDisplay();
                }
                return;

            case push2::soloCC:
                if (v > 0)
                {
                    soloed = (soloed == activeSetup) ? -1 : activeSetup;
                    refreshLeds();
                    renderDisplay();
                }
                return;

            case push2::upCC:
                if (v > 0) selectSetup (activeSetup - 1);
                return;
            case push2::downCC:
                if (v > 0) selectSetup (activeSetup + 1);
                return;
            case push2::leftCC:
                if (v > 0) recallScene (juce::jmax (0, screen.scene - 1));
                return;
            case push2::rightCC:
                if (v > 0) recallScene (juce::jmin (7, screen.scene + 1));
                return;

            case push2::octaveUpCC:
            case push2::octaveDownCC:
                if (v > 0)
                {
                    auto& t = transpose[(size_t) activeSetup];
                    t = juce::jlimit (-4, 4, t + (cc == push2::octaveUpCC ? 1 : -1));
                    lastEvent = setups[(size_t) activeSetup].name + " trasposto di " + juce::String (t) + " ottave";
                    renderDisplay();
                    updateStatus();
                }
                return;

            case 31: // Layout: striscia pitch bend o mod wheel
                if (v > 0)
                {
                    screen.stripMod = ! screen.stripMod;
                    screen.strip = screen.stripMod ? 0.0f : 0.5f;
                    configurePush();
                    lastEvent = juce::String ("Striscia in ") + (screen.stripMod ? "mod wheel" : "pitch bend");
                    renderDisplay();
                    updateStatus();
                }
                return;

            case 57: // Accent: aftertouch per pad o globale
                if (v > 0)
                {
                    screen.polyAftertouch = ! screen.polyAftertouch;
                    configurePush();
                    lastEvent = juce::String ("Aftertouch ") + (screen.polyAftertouch ? "per pad" : "globale");
                    renderDisplay();
                    updateStatus();
                }
                return;

            case push2::modWheelCC: // la striscia in modo mod wheel
                screen.strip = (float) v / 127.0f;
                renderDisplay();
                return;

            case push2::pedal1CC:
                screen.sustain = v >= 64;
                lastEvent = screen.sustain ? "Sustain premuto" : "Sustain rilasciato";
                renderDisplay();
                updateStatus();
                return;

            case push2::pedal2CC:
                if (v != 0 && v != 127)
                    screen.pedal2Continuous = true; // e' un pedale di espressione, non un interruttore
                screen.pedal2 = v;
                if (screen.pedal2Continuous)
                {
                    if (! volumeLocked[(size_t) activeSetup])
                        volume[(size_t) activeSetup] = (float) v / 127.0f;
                }
                else if (v > 0)
                {
                    muted[(size_t) activeSetup] = ! muted[(size_t) activeSetup];
                    refreshLeds();
                }
                renderDisplay();
                updateStatus();
                return;

            default:
                break;
        }
        // Gli altri controlli (encoder, tasti) li mostro e basta: utile per capire la mappa
        lastEvent = "CC " + juce::String (cc) + "   valore " + juce::String (v);
        updateStatus();
    }
}

void MainComponent::timerCallback()
{
    static int ticks = 0;
    ++ticks;

    if (midiOut == nullptr)
    {
        if (ticks % 20 == 0 || ticks == 1)
            if (connectMidi())
                updateStatus();
    }
    else
    {
        // Push scollegato?
        bool present = false;
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            present = present || d.name == midiPortName;
        if (! present)
        {
            disconnectMidi (false);
            updateStatus();
        }
        else if (! paletteApplied)
        {
            ++paletteWaitTicks;
            if ((int) paletteBackup.size() == 2 * numSetups || paletteWaitTicks >= 10)
            {
                applyPalette();
                refreshLeds();
                renderDisplay();
            }
        }
    }

    // lampeggio a tempo: due accensioni per battuta bastano a vedere il BPM
    if (screen.metronome)
    {
        const double beatMs = 60000.0 / juce::jmax (1, screen.bpm);
        const double phase = std::fmod ((double) juce::Time::currentTimeMillis(), beatMs) / beatMs;
        const bool on = phase < 0.3;
        if (on != screen.beat)
        {
            screen.beat = on;
            renderDisplay();
        }
    }
    else if (screen.beat)
    {
        screen.beat = false;
        renderDisplay();
    }

    if (display.getState() != shownDisplayState)
    {
        shownDisplayState = display.getState();
        renderDisplay();
        updateStatus();
    }
}

// ---------------------------------------------------------------- Push 2

void MainComponent::applyPalette()
{
    for (int i = 0; i < numSetups; ++i)
    {
        const auto c = setups[(size_t) i].colour;
        const auto w = (std::uint8_t) (c.getPerceivedBrightness() * 255.0f);
        sendSysex (push2::setPaletteEntry (firstPaletteIndex + i, c.getRed(), c.getGreen(), c.getBlue(), w));

        const auto dim = [] (std::uint8_t v) { return (std::uint8_t) (v / 6); };
        sendSysex (push2::setPaletteEntry (firstPaletteIndex + numSetups + i,
                                           dim (c.getRed()), dim (c.getGreen()), dim (c.getBlue()), dim (w)));
    }
    sendSysex (push2::reapplyPalette());
    paletteApplied = true;
}

void MainComponent::restorePalette()
{
    if (! paletteApplied)
        return;
    for (const auto& [index, entry] : paletteBackup)
        sendSysex (entry);
    sendSysex (push2::reapplyPalette());
    paletteApplied = false;
}

void MainComponent::allLedsOff()
{
    for (int note = 36; note <= 99; ++note)
        send (juce::MidiMessage::noteOn (1, note, (juce::uint8) push2::colourBlack));
    for (int i = 0; i < numSetups; ++i)
    {
        send (juce::MidiMessage::controllerEvent (1, push2::upperDisplayButtonCC + i, push2::colourBlack));
        send (juce::MidiMessage::controllerEvent (1, push2::lowerDisplayButtonCC + i, push2::colourBlack));
    }
    for (int row = 0; row < push2::numSceneButtons; ++row)
        send (juce::MidiMessage::controllerEvent (1, push2::sceneButtonCC (row), push2::colourBlack));
    for (int cc : { push2::masterCC, push2::metronomeCC, push2::muteCC, push2::soloCC, push2::playCC })
        send (juce::MidiMessage::controllerEvent (1, cc, 0));
}

void MainComponent::refreshLeds()
{
    if (midiOut == nullptr || ! paletteApplied)
        return;

    const int full = firstPaletteIndex + activeSetup;
    const int dim  = firstPaletteIndex + numSetups + activeSetup;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
        {
            const int note = push2::padNote (row, col);
            int colour;
            if (padDown[(size_t) note])
                colour = push2::colourWhite;
            else if (row == 7) // fila in basso: un pad per setup
                colour = (col == activeSetup) ? firstPaletteIndex + col : firstPaletteIndex + numSetups + col;
            else
                colour = (col == activeSetup) ? full : dim;
            // canale MIDI 1 in JUCE = canale 0 del manuale: colore fisso
            send (juce::MidiMessage::noteOn (1, note, (juce::uint8) colour));
        }

    for (int i = 0; i < numSetups; ++i)
    {
        send (juce::MidiMessage::controllerEvent (1, push2::upperDisplayButtonCC + i,
                                                  i == activeSetup ? push2::colourWhite : firstPaletteIndex + numSetups + i));
        // fila sotto il display: acceso quando il setup suona, spento in mute
        send (juce::MidiMessage::controllerEvent (1, push2::lowerDisplayButtonCC + i,
                                                  muted[(size_t) i] ? push2::colourBlack : firstPaletteIndex + i));
    }

    // colonna delle scene: bianca quella richiamata, tenue quelle memorizzate
    for (int row = 0; row < push2::numSceneButtons; ++row)
    {
        const int colour = row == screen.scene      ? push2::colourWhite
                         : scenes[(size_t) row].used ? firstPaletteIndex + numSetups
                                                     : push2::colourBlack;
        send (juce::MidiMessage::controllerEvent (1, push2::sceneButtonCC (row), colour));
    }

    // tasti con LED bianco: acceso o spento
    auto white = [this] (int cc, bool on)
    { send (juce::MidiMessage::controllerEvent (1, cc, on ? 127 : 0)); };
    white (push2::masterCC, screen.masterMuted);
    white (push2::metronomeCC, screen.metronome);
    send (juce::MidiMessage::controllerEvent (1, push2::muteCC,
                                              muted[(size_t) activeSetup] ? push2::colourRed : push2::colourBlack));
    send (juce::MidiMessage::controllerEvent (1, push2::soloCC,
                                              soloed >= 0 ? push2::colourWhite : push2::colourBlack));
    send (juce::MidiMessage::controllerEvent (1, push2::playCC,
                                              screen.metronome ? push2::colourWhite : push2::colourBlack));
}

void MainComponent::configurePush()
{
    sendSysex (push2::setTouchStripConfig (screen.stripMod ? push2::stripModWheel : push2::stripPitchBend));
    sendSysex (push2::setAftertouchMode (screen.polyAftertouch));
}

void MainComponent::storeScene (int index)
{
    auto& sc = scenes[(size_t) index];
    sc.volume = volume;
    sc.muted = muted;
    sc.used = true;
    screen.scene = index;
    lastEvent = "Scena " + juce::String (index + 1) + " memorizzata";
    refreshLeds();
    renderDisplay();
    updateStatus();
}

void MainComponent::recallScene (int index)
{
    const auto& sc = scenes[(size_t) index];
    if (! sc.used)
    {
        lastEvent = "Scena " + juce::String (index + 1) + " vuota: tienila con Shift per memorizzarla";
        updateStatus();
        return;
    }
    for (int i = 0; i < numSetups; ++i)
        if (! volumeLocked[(size_t) i]) // un volume bloccato non lo tocca nemmeno la scena
            volume[(size_t) i] = sc.volume[(size_t) i];
    muted = sc.muted;
    screen.scene = index;
    lastEvent = "Scena " + juce::String (index + 1) + " richiamata";
    refreshLeds();
    renderDisplay();
    updateStatus();
}

void MainComponent::selectSetup (int index)
{
    activeSetup = juce::jlimit (0, numSetups - 1, index);
    for (int i = 0; i < numSetups; ++i)
        setupButtons[(size_t) i].setToggleState (i == activeSetup, juce::dontSendNotification);
    refreshLeds();
    renderDisplay();
}

void MainComponent::renderDisplay()
{
    static const std::array<StyledText, 4> deviceNames { devices::launchkeyMiniMk1(), devices::launchkeyMiniMk2(),
                                                         devices::launchkey61Mk2(), devices::keystation88Mk3() };
    std::vector<ScreenSetup> list;
    for (const auto& s : setups)
    {
        const auto i = (size_t) (&s - setups.data());
        const bool off = muted[i] || (soloed >= 0 && soloed != (int) i);
        list.push_back ({ s.name, deviceNames[(size_t) s.device], s.colour, s.nameFace,
                          volume[i], volumeLocked[i], off, transpose[i] });
    }

    screen.active = activeSetup;
    screen.touched = touchedEncoder;
    screen.firmware = firmware ? firmware->upToFirstOccurrenceOf (" ", false, false) : juce::String();
    display.setImage (renderScreen (list, screen, devices::launchpadMiniMk1(), faces));
    repaint();
}

// ---------------------------------------------------------------- finestra

void MainComponent::updateStatus()
{
    midiStatus.setText (midiOut != nullptr ? "MIDI: collegato (" + midiPortName + ")"
                                           : "MIDI: Push 2 non trovato, riprovo ogni 2 secondi",
                        juce::dontSendNotification);

    juce::String ds;
    switch (display.getState())
    {
        case Push2Display::State::connected: ds = "Display: collegato"; break;
        case Push2Display::State::simulator: ds = "Display: sul simulatore (127.0.0.1:48484)"; break;
        case Push2Display::State::busy:      ds = "Display: occupato, chiudi Ableton Live e attendi"; break;
        case Push2Display::State::searching: ds = "Display: in attesa del Push 2 o del simulatore"; break;
    }
    if (const auto err = display.getLastError(); err.isNotEmpty() && display.getState() != Push2Display::State::connected)
        ds << "  (" << err << ")";
    displayStatus.setText (ds, juce::dontSendNotification);

    firmwareStatus.setText ("Firmware: " + (firmware ? *firmware : juce::String ("-")), juce::dontSendNotification);
    eventLabel.setText (lastEvent, juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (frameBlack);
    g.setColour (juce::Colour (0xff1e1e1e));
    g.drawRect (getLocalBounds().reduced (6), 2);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (22);
    title.setBounds (r.removeFromTop (44));
    r.removeFromTop (6);

    auto buttons = r.removeFromTop (52);
    const int w = buttons.getWidth() / numSetups;
    for (auto& b : setupButtons)
        b.setBounds (buttons.removeFromLeft (w).reduced (3));

    r.removeFromTop (14);
    for (auto* l : { &midiStatus, &displayStatus, &firmwareStatus, &eventLabel })
        l->setBounds (r.removeFromTop (34));
}
