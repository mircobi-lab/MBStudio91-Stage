#include "MainComponent.h"
#include "Push2Protocol.h"

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
    faces.logo = juce::ImageCache::getFromMemory (BinaryData::mbstudio91wordmark_png, BinaryData::mbstudio91wordmark_pngSize);
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

    setSize (760, 360);
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
        if (cc >= push2::upperDisplayButtonCC && cc < push2::upperDisplayButtonCC + numSetups)
        {
            if (v > 0)
                selectSetup (cc - push2::upperDisplayButtonCC);
            return;
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
        send (juce::MidiMessage::controllerEvent (1, push2::upperDisplayButtonCC + i, push2::colourBlack));
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
        send (juce::MidiMessage::controllerEvent (1, push2::upperDisplayButtonCC + i,
                                                  i == activeSetup ? push2::colourWhite : firstPaletteIndex + numSetups + i));
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
        list.push_back ({ s.name, deviceNames[(size_t) s.device], s.colour, s.nameFace });
    display.setImage (renderScreen (list, activeSetup, devices::launchpadMiniMk1(),
                                    firmware ? firmware->upToFirstOccurrenceOf (" ", false, false) : juce::String(), faces));
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
        case Push2Display::State::busy:      ds = "Display: occupato, chiudi Ableton Live e attendi"; break;
        case Push2Display::State::searching: ds = "Display: in attesa del Push 2"; break;
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
