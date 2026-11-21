#include "LaunchpadController.h"

LaunchpadController::LaunchpadController (bool openDevices)
{
    if (! openDevices)
        return;

    reconnect();
    devicesChanged = juce::MidiDeviceListConnection::make ([this] { reconnect(); });
    startTimerHz (20);
}

LaunchpadController::~LaunchpadController()
{
    stopTimer();
    devicesChanged = {};
    if (output != nullptr)
        output->sendMessageNow (juce::MidiMessage::controllerEvent (1, 0, 0));   // spegne i LED
    if (input != nullptr)
        input->stop();
}

bool LaunchpadController::isConnected() const
{
    return input != nullptr && output != nullptr;
}

void LaunchpadController::reconnect()
{
    auto findDevice = [] (const juce::Array<juce::MidiDeviceInfo>& list) -> juce::String
    {
        for (const auto& d : list)
            if (d.name.containsIgnoreCase ("Launchpad"))
                return d.identifier;
        return {};
    };

    const auto inId = findDevice (juce::MidiInput::getAvailableDevices());
    const auto outId = findDevice (juce::MidiOutput::getAvailableDevices());

    const bool inChanged = input == nullptr || input->getIdentifier() != inId;
    const bool outChanged = output == nullptr || output->getIdentifier() != outId;
    if (! inChanged && ! outChanged)
        return;

    if (input != nullptr)
        input->stop();
    input = nullptr;
    output = nullptr;

    if (inId.isEmpty() || outId.isEmpty())
        return;   // Launchpad non collegata: si riprova al prossimo cambio dei dispositivi

    input = juce::MidiInput::openDevice (inId, this);
    output = juce::MidiOutput::openDevice (outId);

    if (output != nullptr)
        for (const auto& m : LaunchpadMapping::initMessages())
            output->sendMessageNow (m);

    needsFullRefresh = true;
    if (input != nullptr)
        input->start();
}

void LaunchpadController::showState (const SurfaceState& s)
{
    wanted = s;
}

void LaunchpadController::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    const auto action = LaunchpadMapping::decode (m);
    if (action.type == SurfaceActionType::none)
        return;

    juce::MessageManager::callAsync ([safe = juce::WeakReference<LaunchpadController> (this), action]
    {
        if (auto* self = safe.get())
            self->handleAction (action, juce::Time::getMillisecondCounter());
    });
}

void LaunchpadController::handleAction (SurfaceAction a, juce::uint32 nowMs)
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

void LaunchpadController::tick (juce::uint32 nowMs)
{
    if (panicPressed && ! panicFired && nowMs - panicPressedAt >= panicHoldMs)
    {
        panicFired = true;
        if (onAction)
            onAction ({ SurfaceActionType::panic, 0 });
    }
}

void LaunchpadController::timerCallback()
{
    tick (juce::Time::getMillisecondCounter());

    if (output == nullptr || (! needsFullRefresh && wanted == shown))
        return;

    for (const auto& m : LaunchpadMapping::ledMessages (wanted, needsFullRefresh ? nullptr : &shown))
        output->sendMessageNow (m);

    shown = wanted;
    needsFullRefresh = false;
}
