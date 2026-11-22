#include "Push2Controller.h"
#include "push2/Push2Protocol.h"
#include "push2/Push2SurfaceScreen.h"

Push2Controller::Push2Controller (juce::Typeface::Ptr displayFont, bool openDevices)
    : font (std::move (displayFont))
{
    if (! openDevices)
        return;

    reconnect();
    devicesChanged = juce::MidiDeviceListConnection::make ([this] { reconnect(); });
    startTimerHz (20);
}

Push2Controller::~Push2Controller()
{
    stopTimer();

    if (output != nullptr)
    {
        send (Push2Mapping::allOffMessages());
        restorePalette();
    }

    if (input != nullptr)
        input->stop();
}

bool Push2Controller::isConnected() const
{
    return input != nullptr && output != nullptr;
}

void Push2Controller::showState (const SurfaceState& s)
{
    wanted = s;
}

void Push2Controller::send (const std::vector<juce::MidiMessage>& msgs)
{
    if (output == nullptr)
        return;
    for (const auto& m : msgs)
        output->sendMessageNow (m);
}

void Push2Controller::restorePalette()
{
    if (originalPalette.empty())
        return;
    send (originalPalette);
    send ({ juce::MidiMessage::createSysExMessage (push2::reapplyPalette().data() + 1,
                                                   (int) push2::reapplyPalette().size() - 2) });
    originalPalette.clear();
}

std::optional<juce::MidiDeviceInfo> Push2Controller::choosePort (const juce::Array<juce::MidiDeviceInfo>& list,
                                                                 bool preferUserPort)
{
    const char* wanted = preferUserPort ? "User" : "Live";
    std::optional<juce::MidiDeviceInfo> fallback;

    for (const auto& d : list)
    {
        if (! d.name.containsIgnoreCase ("Push 2"))
            continue;
        if (d.name.containsIgnoreCase (wanted))
            return d;
        if (! fallback.has_value())
            fallback = d;
    }
    return fallback;
}

void Push2Controller::setUseUserPort (bool user)
{
    if (useUserPort == user)
        return;
    useUserPort = user;
    reconnect();
}

void Push2Controller::reconnect()
{
    const auto in = choosePort (juce::MidiInput::getAvailableDevices(), useUserPort);
    const auto out = choosePort (juce::MidiOutput::getAvailableDevices(), useUserPort);
    const auto inId = in.has_value() ? in->identifier : juce::String();
    const auto outId = out.has_value() ? out->identifier : juce::String();

    const bool inChanged = input == nullptr || input->getIdentifier() != inId;
    const bool outChanged = output == nullptr || output->getIdentifier() != outId;
    if (! inChanged && ! outChanged)
        return;

    if (output != nullptr)
        restorePalette();
    if (input != nullptr)
        input->stop();
    input = nullptr;
    output = nullptr;

    portName.clear();

    if (inId.isEmpty() || outId.isEmpty())
        return;   // Push non collegato: si riprova al prossimo cambio dei dispositivi

    input = juce::MidiInput::openDevice (inId, this);
    output = juce::MidiOutput::openDevice (outId);

    if (output != nullptr)
    {
        portName = out->name.contains ("User") ? "PORTA USER" : "PORTA LIVE";
        // Prima si chiedono le voci di palette che useremo, per poterle rimettere
        // a posto all'uscita, poi si impostano le nostre.
        send (Push2Mapping::readPaletteMessages());
        send (Push2Mapping::paletteMessages());
    }

    needsFullRefresh = true;
    screenDirty = true;
    if (input != nullptr)
        input->start();
}

void Push2Controller::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    // Risposta a "Get LED Color Palette Entry": la conserviamo per rimetterla all'uscita
    if (m.isSysEx())
    {
        const auto* d = m.getSysExData();
        const int n = m.getSysExDataSize();
        if (n == 13 && d[0] == 0x00 && d[1] == 0x21 && d[2] == 0x1D && d[5] == 0x04)
        {
            std::vector<juce::uint8> entry (d, d + n);
            entry[5] = 0x03;   // stessa voce, comando "Set"
            juce::MessageManager::callAsync ([safe = juce::WeakReference<Push2Controller> (this), entry]
            {
                if (auto* self = safe.get())
                    self->originalPalette.push_back (
                        juce::MidiMessage::createSysExMessage (entry.data(), (int) entry.size()));
            });
        }
        return;
    }

    const auto action = Push2Mapping::decode (m);
    if (action.type == SurfaceActionType::none)
        return;

    juce::MessageManager::callAsync ([safe = juce::WeakReference<Push2Controller> (this), action]
    {
        if (auto* self = safe.get())
            self->handleAction (action, juce::Time::getMillisecondCounter());
    });
}

void Push2Controller::handleAction (SurfaceAction a, juce::uint32 nowMs)
{
    switch (a.type)
    {
        case SurfaceActionType::panicDown:
            panicPressed = true;
            panicFired = false;
            panicPressedAt = nowMs;
            return;

        case SurfaceActionType::panicUp:
            panicPressed = false;
            return;

        default:
            if (onAction)
                onAction (a);
    }
}

void Push2Controller::tick (juce::uint32 nowMs)
{
    if (panicPressed && ! panicFired && nowMs - panicPressedAt >= panicHoldMs)
    {
        panicFired = true;
        if (onAction)
            onAction ({ SurfaceActionType::panic, 0 });
    }
}

void Push2Controller::timerCallback()
{
    tick (juce::Time::getMillisecondCounter());

    const bool changed = wanted != shown;

    if (output != nullptr && (needsFullRefresh || changed))
    {
        send (Push2Mapping::ledMessages (wanted, needsFullRefresh ? nullptr : &shown));
        needsFullRefresh = false;
    }

    // Il display va per conto suo: funziona anche se il MIDI non e' collegato
    if (changed || screenDirty)
    {
        display.setImage (renderSurfaceScreen (wanted, font));
        screenDirty = false;
    }

    shown = wanted;
}
