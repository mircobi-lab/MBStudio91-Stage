#include "MainComponent.h"
#include "StageLookAndFeel.h"
#include "settings/AppPaths.h"

MainComponent::MainComponent (StageEngine& e, PluginCatalog& c)
    : engine (e), catalog (c)
{
    title.setText ("MBSTUDIO91(TM) STAGE", juce::dontSendNotification);
    title.setFont (StageLookAndFeel::titleFont (34.0f));
    addAndMakeVisible (title);

    for (auto* b : { &audioButton, &foldersButton, &scanButton, &panicButton })
        addAndMakeVisible (*b);

    panicButton.setColour (juce::TextButton::buttonColourId, StageLookAndFeel::panicRed);
    panicButton.onClick = [this] { engine.panicAll(); };
    audioButton.onClick = [this] { showAudioSettings(); };
    scanButton.onClick  = [this] { startScan(); };
    foldersButton.onClick = [this] { showScanFolders(); };

    for (int i = 0; i < StageEngine::numStations; ++i)
    {
        panels[(size_t) i] = std::make_unique<StationPanel> (engine, catalog, i);
        panels[(size_t) i]->onStatus = [this] (const juce::String& s) { statusMessage = s; };
        addAndMakeVisible (*panels[(size_t) i]);
    }

    engine.onLearnFinished = [this] (int station)
    {
        if (auto a = engine.keyboards().assignmentFor (station))
            statusMessage = a->deviceName + " assegnata alla postazione " + juce::String (station + 1);
    };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    setSize (1400, 820);
    startTimerHz (4);
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.onLearnFinished = nullptr;
    // Il dialog va chiuso prima che il motore (e il suo AudioDeviceManager) venga distrutto.
    if (auto* w = audioSettingsWindow.getComponent())
        delete w;
    if (auto* w = foldersWindow.getComponent())
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
    panicButton.setBounds (top.removeFromRight (140).reduced (2));
    top.removeFromRight (10);
    scanButton.setBounds (top.removeFromRight (190).reduced (2));
    top.removeFromRight (10);
    foldersButton.setBounds (top.removeFromRight (120).reduced (2));
    top.removeFromRight (10);
    audioButton.setBounds (top.removeFromRight (210).reduced (2));
    title.setBounds (top);

    statusLabel.setBounds (r.removeFromBottom (28));
    r.removeFromTop (12);
    r.removeFromBottom (8);

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

void MainComponent::startScan (std::function<void()> afterwards)
{
    if (catalog.isScanning())
        return;

    const auto folders = catalog.foldersToScan();
    statusMessage = "Scansione in corso su " + juce::String (folders.size()) + " cartelle...";
    scanButton.setEnabled (false);

    catalog.scanAsync ([safe = juce::Component::SafePointer<MainComponent> (this), afterwards]
                       (int found, juce::StringArray failed)
    {
        if (auto* self = safe.getComponent())
        {
            self->scanButton.setEnabled (true);
            self->statusMessage = "Scansione finita: " + juce::String (found) + " plugin nuovi"
                                + (failed.isEmpty() ? juce::String() : ", " + juce::String (failed.size()) + " scartati");
            for (auto& p : self->panels)
                p->refreshPluginMenus();
            if (afterwards)
                afterwards();
        }
    });
}

//==============================================================================
void MainComponent::loadScanFolders()
{
    juce::StringArray folders;
    folders.addLines (AppPaths::scanFoldersFile().loadFileAsString());
    folders.trim();
    folders.removeEmptyStrings();
    catalog.setExtraFolders (folders);
}

void MainComponent::saveScanFolders (const juce::StringArray& folders)
{
    catalog.setExtraFolders (folders);
    AppPaths::scanFoldersFile().replaceWithText (catalog.getExtraFolders().joinIntoString ("\n"));
}

void MainComponent::showScanFolders()
{
    if (auto* w = foldersWindow.getComponent())
    {
        w->toFront (true);
        return;
    }

    auto* panel = new juce::Component();
    panel->setSize (560, 320);

    auto* info = new juce::Label ({}, "Cartelle in piu' da battere, una per riga. Quelle standard del sistema "
                                      "sono gia' comprese.");
    info->setFont (StageLookAndFeel::titleFont (16.0f));
    info->setJustificationType (juce::Justification::topLeft);
    info->setBounds (12, 8, 536, 40);
    panel->addAndMakeVisible (info);

    auto* text = new juce::TextEditor();
    text->setMultiLine (true, false);
    text->setReturnKeyStartsNewLine (true);
    text->setText (catalog.getExtraFolders().joinIntoString ("\n"));
    text->setBounds (12, 52, 536, 190);
    panel->addAndMakeVisible (text);

    auto* add = new juce::TextButton ("AGGIUNGI CARTELLA...");
    add->setBounds (12, 254, 220, 30);
    add->onClick = [this, text]
    {
        chooser = std::make_unique<juce::FileChooser> ("Scegli una cartella di plugin");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [text] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir.isDirectory())
                text->setText ((text->getText().trim() + "\n" + dir.getFullPathName()).trim());
        });
    };
    panel->addAndMakeVisible (add);

    auto* ok = new juce::TextButton ("SALVA");
    ok->setBounds (438, 254, 110, 30);
    ok->onClick = [this, text]
    {
        juce::StringArray folders;
        folders.addLines (text->getText());
        folders.trim();
        folders.removeEmptyStrings();
        saveScanFolders (folders);
        statusMessage = "Cartelle salvate: " + juce::String (folders.size())
                      + " in piu'. Premi SCANSIONA PLUGIN per rileggerle.";
        if (auto* w = foldersWindow.getComponent())
            w->exitModalState (0);
    };
    panel->addAndMakeVisible (ok);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (panel);
    o.dialogTitle = "Cartelle dei plugin";
    o.dialogBackgroundColour = juce::Colours::black;
    o.useNativeTitleBar = true;
    o.resizable = false;
    foldersWindow = o.launchAsync();
}

//==============================================================================
void MainComponent::restoreSavedPlugins()
{
    const auto& saved = engine.savedStationStates();

    juce::StringArray missing;
    int wanted = 0;
    for (const auto& st : saved)
        for (const auto& id : st.allPluginIds())
        {
            ++wanted;
            if (! catalog.findByPortableId (id))
                missing.add (id);
        }

    if (wanted == 0)
        return;

    // Un plugin che non risulta nell'elenco puo' essere appena stato compilato:
    // vale la pena rifare la scansione una volta sola, poi si riprova.
    if (! missing.isEmpty() && ! scannedForMissingPlugins && ! catalog.isScanning())
    {
        scannedForMissingPlugins = true;
        statusMessage = juce::String (missing.size()) + " plugin delle postazioni non sono nell'elenco: riscansiono";
        startScan ([safe = juce::Component::SafePointer<MainComponent> (this)]
                   { if (auto* self = safe.getComponent()) self->restoreSavedPlugins(); });
        return;
    }

    pluginsToRestore = wanted;
    pluginsRestored = 0;
    statusMessage = "Carico i plugin delle postazioni...";

    for (int i = 0; i < StageEngine::numStations; ++i)
    {
        const auto& st = saved[(size_t) i];
        for (int slot = 0; slot <= Station::maxEffects; ++slot)
        {
            const auto id = slot == 0 ? st.instrumentId : st.effectIds[(size_t) (slot - 1)];
            if (id.isEmpty())
                continue;

            const auto desc = catalog.findByPortableId (id);
            if (! desc.has_value())
            {
                ++pluginsRestored;
                engine.station (i).setLoadError ("non trovato: " + id);
                continue;
            }

            catalog.createInstanceAsync (*desc, engine.getSampleRate(), engine.getBlockSize(),
                [safe = juce::Component::SafePointer<MainComponent> (this), i, slot]
                (std::unique_ptr<juce::AudioPluginInstance> inst, juce::String error)
                {
                    auto* self = safe.getComponent();
                    if (self == nullptr)
                        return;

                    auto& station = self->engine.station (i);
                    if (inst == nullptr)
                    {
                        station.setLoadError (error);
                    }
                    else
                    {
                        if (slot == 0) station.setInstrument (std::move (inst));
                        else           station.setEffect (slot - 1, std::move (inst));
                        station.setLoadError ({});
                    }

                    ++self->pluginsRestored;
                    self->panels[(size_t) i]->refreshPluginMenus();

                    if (self->pluginsRestored >= self->pluginsToRestore)
                        self->statusMessage = "Postazioni pronte: " + juce::String (self->pluginsToRestore)
                                            + " plugin caricati";
                });
        }
    }
}

void MainComponent::startInitialScanIfNeeded (bool listFileExisted)
{
    loadScanFolders();

    if (! listFileExisted)
        startScan ([safe = juce::Component::SafePointer<MainComponent> (this)]
                   { if (auto* self = safe.getComponent()) self->restoreSavedPlugins(); });
    else
        restoreSavedPlugins();
}

void MainComponent::timerCallback()
{
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
