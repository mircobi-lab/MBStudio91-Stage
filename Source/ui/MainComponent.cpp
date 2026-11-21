#include "MainComponent.h"
#include "StageLookAndFeel.h"

//==============================================================================
// Finestra separata con l'interfaccia originale del plugin.
class MainComponent::EditorWindow final : public juce::DocumentWindow
{
public:
    EditorWindow (juce::AudioPluginInstance& p, std::function<void()> onClose)
        : juce::DocumentWindow (p.getName(), juce::Colours::black, juce::DocumentWindow::closeButton),
          closeCallback (std::move (onClose))
    {
        setUsingNativeTitleBar (true);

        juce::AudioProcessorEditor* ed = p.hasEditor() ? p.createEditorAndMakeActive() : nullptr;
        if (ed == nullptr)
            ed = new juce::GenericAudioProcessorEditor (p);

        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~EditorWindow() override { clearContentComponent(); }   // distrugge l'editor prima dell'istanza

    void closeButtonPressed() override
    {
        if (closeCallback)
            closeCallback();
    }

private:
    std::function<void()> closeCallback;
};

//==============================================================================
MainComponent::MainComponent (StageEngine& e, PluginCatalog& c)
    : engine (e), catalog (c), station (e.station (0))
{
    title.setText ("MBSTUDIO91(TM) STAGE", juce::dontSendNotification);
    title.setFont (StageLookAndFeel::titleFont (34.0f));
    addAndMakeVisible (title);

    stationTitle.setText ("1  POSTAZIONE 1", juce::dontSendNotification);
    stationTitle.setFont (StageLookAndFeel::titleFont (24.0f));
    addAndMakeVisible (stationTitle);

    for (auto* b : { &audioButton, &scanButton, &panicButton, &muteButton })
        addAndMakeVisible (*b);

    panicButton.setColour (juce::TextButton::buttonColourId, StageLookAndFeel::panicRed);
    panicButton.onClick = [this] { engine.panicAll(); };
    audioButton.onClick = [this] { showAudioSettings(); };
    scanButton.onClick  = [this] { startScan(); };

    muteButton.setClickingTogglesState (true);
    muteButton.onClick = [this] { station.setMuted (muteButton.getToggleState()); };

    volume.setRange (-60.0, 6.0, 0.1);
    volume.setValue (0.0, juce::dontSendNotification);
    volume.setTextValueSuffix (" dB");
    volume.setDoubleClickReturnValue (true, 0.0);
    volume.onValueChange = [this] { station.setGainDb ((float) volume.getValue()); };
    addAndMakeVisible (volume);
    volumeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (volumeLabel);

    for (int s = 0; s < numSlots; ++s)
    {
        slotLabels[(size_t) s].setText (s == 0 ? "STRUMENTO" : "EFFETTO " + juce::String (s), juce::dontSendNotification);
        addAndMakeVisible (slotLabels[(size_t) s]);

        slotMenus[(size_t) s].onChange = [this, s] { slotChosen (s); };
        addAndMakeVisible (slotMenus[(size_t) s]);

        openButtons[(size_t) s].setButtonText ("APRI PLUGIN");
        openButtons[(size_t) s].onClick = [this, s] { openEditor (s); };
        addAndMakeVisible (openButtons[(size_t) s]);
    }

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    // Prima che un'istanza venga distrutta, la sua finestra va chiusa (Review Focus 4).
    station.onBeforeInstanceRemoved = [this] (juce::AudioPluginInstance& p) { closeEditorFor (p); };

    refreshPluginMenus();
    setSize (1100, 560);
    startTimerHz (4);
}

MainComponent::~MainComponent()
{
    stopTimer();
    // Il dialog va chiuso prima che il motore (e il suo AudioDeviceManager) venga distrutto.
    if (auto* w = audioSettingsWindow.getComponent())
        delete w;
    editors.clear();
    station.onBeforeInstanceRemoved = nullptr;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawRoundedRectangle (stationTitle.getBounds().withBottom (volume.getBottom() + 10).toFloat().expanded (10.0f), 6.0f, 1.5f);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (20);

    auto top = r.removeFromTop (50);
    panicButton.setBounds (top.removeFromRight (140).reduced (2));
    top.removeFromRight (10);
    scanButton.setBounds (top.removeFromRight (190).reduced (2));
    top.removeFromRight (10);
    audioButton.setBounds (top.removeFromRight (210).reduced (2));
    title.setBounds (top);

    statusLabel.setBounds (r.removeFromBottom (30));
    r.removeFromTop (30);

    auto panel = r.reduced (10);
    stationTitle.setBounds (panel.removeFromTop (34));
    panel.removeFromTop (10);

    auto right = panel.removeFromRight (180);
    volumeLabel.setBounds (right.removeFromTop (24));
    volume.setBounds (right.removeFromTop (140));
    right.removeFromTop (10);
    muteButton.setBounds (right.removeFromTop (40).reduced (20, 0));

    panel.removeFromRight (20);
    for (int s = 0; s < numSlots; ++s)
    {
        auto row = panel.removeFromTop (44);
        slotLabels[(size_t) s].setBounds (row.removeFromLeft (120));
        openButtons[(size_t) s].setBounds (row.removeFromRight (150).reduced (2));
        row.removeFromRight (10);
        slotMenus[(size_t) s].setBounds (row.reduced (2));
        panel.removeFromTop (8);
    }
}

void MainComponent::refreshPluginMenus()
{
    instrumentTypes.clear();
    effectTypes.clear();

    auto types = catalog.list().getTypes();
    std::sort (types.begin(), types.end(), [] (const auto& a, const auto& b)
               { return a.name.compareNatural (b.name) < 0; });

    for (const auto& d : types)
        (d.isInstrument ? instrumentTypes : effectTypes).push_back (d);

    for (int s = 0; s < numSlots; ++s)
    {
        auto& menu = slotMenus[(size_t) s];
        const auto& list = s == 0 ? instrumentTypes : effectTypes;
        menu.clear (juce::dontSendNotification);
        menu.addItem ("- NESSUNO -", 1);
        for (size_t i = 0; i < list.size(); ++i)
            menu.addItem (list[i].name + "  (" + list[i].manufacturerName + ", " + list[i].pluginFormatName + ")",
                          (int) i + 2);

        // Riseleziona il plugin gia' caricato, se c'e'.
        int selected = 1;
        if (auto* inst = instanceInSlot (s))
        {
            const auto current = inst->getPluginDescription();
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i].isDuplicateOf (current))
                    selected = (int) i + 2;
        }
        menu.setSelectedId (selected, juce::dontSendNotification);
    }
}

juce::AudioPluginInstance* MainComponent::instanceInSlot (int slot) const
{
    return slot == 0 ? station.getInstrument() : station.getEffect (slot - 1);
}

void MainComponent::slotChosen (int slot)
{
    const int id = slotMenus[(size_t) slot].getSelectedId();
    const auto& list = slot == 0 ? instrumentTypes : effectTypes;

    if (id <= 1)
    {
        if (slot == 0) station.setInstrument (nullptr);
        else           station.setEffect (slot - 1, nullptr);
        return;
    }

    const auto desc = list[(size_t) (id - 2)];
    scanMessage = "Carico " + desc.name + "...";

    catalog.createInstanceAsync (desc, engine.getSampleRate(), engine.getBlockSize(),
        [safe = juce::Component::SafePointer<MainComponent> (this), slot]
        (std::unique_ptr<juce::AudioPluginInstance> inst, juce::String error)
        {
            auto* self = safe.getComponent();
            if (self == nullptr)
                return;

            if (inst == nullptr)
            {
                // Il plugin precedente resta caricato e continua a suonare (Review Focus 1).
                self->station.setLoadError (error);
                self->scanMessage = {};
                self->refreshPluginMenus();
                return;
            }

            if (slot == 0) self->station.setInstrument (std::move (inst));
            else           self->station.setEffect (slot - 1, std::move (inst));
            self->station.setLoadError ({});
            self->scanMessage = {};
        });
}

void MainComponent::openEditor (int slot)
{
    auto* inst = instanceInSlot (slot);
    if (inst == nullptr)
        return;

    if (auto it = editors.find (inst); it != editors.end())
    {
        it->second->toFront (true);
        return;
    }

    editors[inst] = std::make_unique<EditorWindow> (*inst,
        [safe = juce::Component::SafePointer<MainComponent> (this), inst]
        {
            juce::MessageManager::callAsync ([safe, inst]
            {
                if (auto* self = safe.getComponent())
                    self->editors.erase (inst);
            });
        });
}

void MainComponent::closeEditorFor (juce::AudioPluginInstance& p)
{
    editors.erase (&p);
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

    scanMessage = "Scansione dei plugin in corso...";
    scanButton.setEnabled (false);

    catalog.scanAsync ([safe = juce::Component::SafePointer<MainComponent> (this)] (int found, juce::StringArray failed)
    {
        if (auto* self = safe.getComponent())
        {
            self->scanButton.setEnabled (true);
            self->scanMessage = "Scansione finita: " + juce::String (found) + " plugin nuovi"
                              + (failed.isEmpty() ? juce::String() : ", " + juce::String (failed.size()) + " scartati");
            self->refreshPluginMenus();
        }
    });
}

void MainComponent::startInitialScanIfNeeded (bool listFileExisted)
{
    if (! listFileExisted)
        startScan();
}

void MainComponent::timerCallback()
{
    juce::String text;
    const auto err = station.getLoadError();
    if (err.isNotEmpty())
        text << "ERRORE: " << err << "    ";
    if (scanMessage.isNotEmpty())
        text << scanMessage << "    ";

    const auto& dm = engine.deviceManager();
    if (auto* dev = dm.getCurrentAudioDevice())
        text << "AUDIO " << dev->getName() << "  " << juce::String (dev->getCurrentSampleRate() / 1000.0, 1)
             << " KHZ / " << dev->getCurrentBufferSizeSamples() << "    ";
    else
        text << "NESSUNA USCITA AUDIO    ";

    text << "CPU " << juce::String (juce::roundToInt (dm.getCpuUsage() * 100.0)) << "%";
    statusLabel.setText (text, juce::dontSendNotification);

    for (int s = 0; s < numSlots; ++s)
        openButtons[(size_t) s].setEnabled (instanceInSlot (s) != nullptr);
}
