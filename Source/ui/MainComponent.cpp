#include "MainComponent.h"
#include "StageLookAndFeel.h"

MainComponent::MainComponent (StageEngine& e, PluginCatalog& c, PatchManager& pm, Library& lib, SurfaceCoordinator& sc)
    : engine (e), catalog (c), patches (pm), coordinator (sc)
{
    // Tempo: INT/EXT, BPM, TAP, sorgente del clock
    tempoSourceButton.setClickingTogglesState (true);
    tempoSourceButton.onClick = [this] { tempoControlsChanged(); };
    bpm.setRange (20.0, 300.0, 0.1);
    bpm.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    bpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 30);
    bpm.setTextValueSuffix (" BPM");
    bpm.onValueChange = [this] { engine.tempo().setBpm (bpm.getValue()); engine.saveKeyboardSettings(); };
    tapButton.onClick = [this] { coordinator.handleTap (juce::Time::getMillisecondCounterHiRes() * 0.001); };
    clockMenu.onChange = [this] { tempoControlsChanged(); };
    clockLost.setColour (juce::Label::textColourId, StageLookAndFeel::panicRed.brighter (0.3f));
    for (auto* comp : std::initializer_list<juce::Component*> { &tempoSourceButton, &bpm, &tapButton, &clockLabel, &clockMenu, &clockLost })
        addAndMakeVisible (comp);
    tempoSourceButton.setToggleState (engine.tempo().getSource() == TempoTracker::Source::external, juce::dontSendNotification);
    refreshClockMenu();
    midiDevicesChanged = juce::MidiDeviceListConnection::make ([this] { refreshClockMenu(); });

    patchPanel = std::make_unique<PatchPanel> (pm, lib);
    patchPanel->onLibraryChanged = [this] { if (onLibraryChanged) onLibraryChanged(); };
    addAndMakeVisible (*patchPanel);

    title.setText ("MBSTUDIO91(TM) STAGE", juce::dontSendNotification);
    title.setFont (StageLookAndFeel::titleFont (34.0f));
    addAndMakeVisible (title);

    for (auto* b : { &audioButton, &scanButton, &panicButton })
        addAndMakeVisible (*b);

    panicButton.setColour (juce::TextButton::buttonColourId, StageLookAndFeel::panicRed);
    panicButton.onClick = [this] { engine.panicAll(); };
    audioButton.onClick = [this] { showAudioSettings(); };
    scanButton.onClick  = [this] { startScan(); };

    for (int i = 0; i < StageEngine::numStations; ++i)
    {
        panels[(size_t) i] = std::make_unique<StationPanel> (engine, catalog, i);
        panels[(size_t) i]->onStatus = [this] (const juce::String& s) { statusMessage = s; };
        panels[(size_t) i]->onUserEdit = [this] { patches.markModified(); };
        addAndMakeVisible (*panels[(size_t) i]);
    }

    engine.onLearnFinished = [this] (int station)
    {
        if (auto a = engine.keyboards().assignmentFor (station))
            statusMessage = a->deviceName + " assegnata alla postazione " + juce::String (station + 1);
    };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    // Dopo un richiamo le postazioni mostrano i valori della patch.
    patches.onStateChanged = [this]
    {
        for (auto& p : panels)
            p->refreshFromEngine();
        if (patchPanel != nullptr)
            patchPanel->repaint();
    };

    setSize (1600, 900);
    startTimerHz (4);
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.onLearnFinished = nullptr;
    patches.onStateChanged = nullptr;
    // Il dialog va chiuso prima che il motore (e il suo AudioDeviceManager) venga distrutto.
    if (auto* w = audioSettingsWindow.getComponent())
        delete w;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (50);
    panicButton.setBounds (top.removeFromRight (130).reduced (2));
    top.removeFromRight (8);
    scanButton.setBounds (top.removeFromRight (170).reduced (2));
    top.removeFromRight (8);
    audioButton.setBounds (top.removeFromRight (190).reduced (2));
    top.removeFromRight (20);
    clockLost.setBounds (top.removeFromRight (120));
    clockMenu.setBounds (top.removeFromRight (180).reduced (2, 9));
    clockLabel.setBounds (top.removeFromRight (70));
    top.removeFromRight (8);
    tapButton.setBounds (top.removeFromRight (70).reduced (2));
    bpm.setBounds (top.removeFromRight (160).reduced (2, 8));
    tempoSourceButton.setBounds (top.removeFromRight (60).reduced (2));
    title.setBounds (top);

    statusLabel.setBounds (r.removeFromBottom (28));
    r.removeFromTop (12);
    r.removeFromBottom (8);

    patchPanel->setBounds (r.removeFromRight (380));
    r.removeFromRight (10);

    const int gap = 10;
    const int w = (r.getWidth() - gap * (StageEngine::numStations - 1)) / StageEngine::numStations;
    for (auto& p : panels)
    {
        p->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
}

void MainComponent::showAudioSettings()
{
    if (auto* w = audioSettingsWindow.getComponent())
    {
        w->toFront (true);
        return;
    }

    auto* selector = new juce::AudioDeviceSelectorComponent (engine.deviceManager(), 0, 0, 2, 2,
                                                             false, false, true, false);
    selector->setSize (520, 360);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Impostazioni audio";
    o.dialogBackgroundColour = juce::Colours::black;
    o.useNativeTitleBar = true;
    o.resizable = false;
    audioSettingsWindow = o.launchAsync();
}

void MainComponent::startScan()
{
    if (catalog.isScanning())
        return;

    statusMessage = "Scansione dei plugin in corso...";
    scanButton.setEnabled (false);

    catalog.scanAsync ([safe = juce::Component::SafePointer<MainComponent> (this)] (int found, juce::StringArray failed)
    {
        if (auto* self = safe.getComponent())
        {
            self->scanButton.setEnabled (true);
            self->statusMessage = "Scansione finita: " + juce::String (found) + " plugin nuovi"
                                + (failed.isEmpty() ? juce::String() : ", " + juce::String (failed.size()) + " scartati");
            for (auto& p : self->panels)
                p->refreshPluginMenus();
        }
    });
}

void MainComponent::startInitialScanIfNeeded (bool listFileExisted)
{
    if (! listFileExisted)
        startScan();
}

void MainComponent::refreshClockMenu()
{
    clockIds.clear();
    clockMenu.clear (juce::dontSendNotification);
    clockMenu.addItem ("- NESSUNO -", 1);
    clockIds.add ({});
    for (const auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (StageEngine::isReservedControlDevice (d.name))
            continue;
        clockIds.add (d.identifier);
        clockMenu.addItem (d.name, clockIds.size());
    }
    const int idx = clockIds.indexOf (engine.getClockSourceDevice());
    clockMenu.setSelectedId (idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);
}

void MainComponent::tempoControlsChanged()
{
    const bool ext = tempoSourceButton.getToggleState();
    engine.tempo().setSource (ext ? TempoTracker::Source::external : TempoTracker::Source::internal);
    const int sel = clockMenu.getSelectedId() - 1;
    engine.setClockSourceDevice (juce::isPositiveAndBelow (sel, clockIds.size()) ? clockIds[sel] : juce::String());
    engine.saveKeyboardSettings();
}

void MainComponent::timerCallback()
{
    // Tempo
    auto& tempo = engine.tempo();
    const bool ext = tempo.getSource() == TempoTracker::Source::external;
    tempoSourceButton.setButtonText (ext ? "EXT" : "INT");
    bpm.setEnabled (! ext);
    tapButton.setEnabled (! ext);
    if (! bpm.isMouseButtonDown())
        bpm.setValue (tempo.getBpm(), juce::dontSendNotification);
    clockLost.setText (ext && tempo.isClockLost (juce::Time::getMillisecondCounterHiRes() * 0.001) ? "CLOCK ASSENTE" : "",
                       juce::dontSendNotification);

    juce::String text;
    if (statusMessage.isNotEmpty())
        text << statusMessage << "    ";

    const auto& dm = engine.deviceManager();
    if (auto* dev = dm.getCurrentAudioDevice())
        text << "AUDIO " << dev->getName() << "  " << juce::String (dev->getCurrentSampleRate() / 1000.0, 1)
             << " KHZ / " << dev->getCurrentBufferSizeSamples() << "    ";
    else
        text << "NESSUNA USCITA AUDIO    ";

    text << "CPU " << juce::String (juce::roundToInt (dm.getCpuUsage() * 100.0)) << "%";
    statusLabel.setText (text, juce::dontSendNotification);
}
