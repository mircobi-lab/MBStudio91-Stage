#include "StationPanel.h"
#include "StageLookAndFeel.h"

namespace
{
    juce::String noteName (double v) { return juce::MidiMessage::getMidiNoteName ((int) v, true, true, 3); }

    void setupIncDec (juce::Slider& s, double lo, double hi)
    {
        s.setRange (lo, hi, 1.0);
        s.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 26);
    }

    constexpr juce::uint32 activityFlashMs = 120;
}

//==============================================================================
class StationPanel::EditorWindow final : public juce::DocumentWindow
{
public:
    EditorWindow (juce::AudioPluginInstance& p, const juce::String& title, std::function<void()> onClose)
        : juce::DocumentWindow (title, juce::Colours::black, juce::DocumentWindow::closeButton),
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

    ~EditorWindow() override { clearContentComponent(); }   // l'editor muore prima dell'istanza

    void closeButtonPressed() override
    {
        if (closeCallback)
            closeCallback();
    }

private:
    std::function<void()> closeCallback;
};

//==============================================================================
StationPanel::StationPanel (StageEngine& e, PluginCatalog& c, int i)
    : engine (e), catalog (c), index (i), station (e.station (i))
{
    title.setText (juce::String (index + 1) + "  POSTAZIONE " + juce::String (index + 1), juce::dontSendNotification);
    title.setFont (StageLookAndFeel::titleFont (26.0f));
    addAndMakeVisible (title);

    deviceLabel.setFont (StageLookAndFeel::titleFont (18.0f));
    addAndMakeVisible (deviceLabel);

    assignButton.onClick = [this] { assignClicked(); };
    addAndMakeVisible (assignButton);

    for (int s = 0; s < numSlots; ++s)
    {
        slotLabels[(size_t) s].setText (s == 0 ? "STRUMENTO" : "EFFETTO " + juce::String (s), juce::dontSendNotification);
        addAndMakeVisible (slotLabels[(size_t) s]);

        slotMenus[(size_t) s].onChange = [this, s] { slotChosen (s); };
        addAndMakeVisible (slotMenus[(size_t) s]);

        openButtons[(size_t) s].setButtonText ("APRI");
        openButtons[(size_t) s].onClick = [this, s] { openEditor (s); };
        addAndMakeVisible (openButtons[(size_t) s]);
    }

    volume.setRange (-60.0, 6.0, 0.1);
    volume.setTextValueSuffix (" dB");
    volume.setDoubleClickReturnValue (true, 0.0);
    volume.setValue (station.getGainDb(), juce::dontSendNotification);
    volume.onValueChange = [this] { station.setGainDb ((float) volume.getValue()); };
    addAndMakeVisible (volume);

    muteButton.setClickingTogglesState (true);
    muteButton.onClick = [this] { station.setMuted (muteButton.getToggleState()); };
    addAndMakeVisible (muteButton);

    faderButton.setClickingTogglesState (true);
    faderButton.onClick = [this] { zoneControlChanged(); };
    addAndMakeVisible (faderButton);

    setupIncDec (transpose, -48.0, 48.0);
    transpose.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v); };
    setupIncDec (lowNote, 0.0, 127.0);
    setupIncDec (highNote, 0.0, 127.0);
    lowNote.textFromValueFunction = noteName;
    highNote.textFromValueFunction = noteName;
    for (auto* s : { &lowNote, &highNote })
        s->valueFromTextFunction = [] (const juce::String& t)
        {
            for (int n = 0; n < 128; ++n)
                if (noteName (n).equalsIgnoreCase (t.trim())) return (double) n;
            return t.getDoubleValue();
        };

    gamma.setRange (0.3, 3.0, 0.05);
    gamma.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);

    curveMenu.addItemList ({ "LINEARE", "MORBIDA", "DURA", "PERSONALIZZATA" }, 1);

    for (auto* s : { &transpose, &lowNote, &highNote, &gamma })
    {
        s->onValueChange = [this] { zoneControlChanged(); };
        addAndMakeVisible (*s);
    }
    curveMenu.onChange = [this] { zoneControlChanged(); };
    addAndMakeVisible (curveMenu);

    for (auto* l : { &transposeLabel, &lowLabel, &highLabel, &curveLabel, &gammaLabel })
        addAndMakeVisible (*l);

    errorLabel.setColour (juce::Label::textColourId, StageLookAndFeel::panicRed.brighter (0.4f));
    errorLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (errorLabel);

    // Prima che un'istanza venga distrutta, la sua finestra va chiusa.
    station.onBeforeInstanceRemoved = [this] (juce::AudioPluginInstance& p) { closeEditorFor (p); };

    loadZoneIntoControls();
    for (auto* sl : { &transpose, &lowNote, &highNote })
        sl->updateText();   // applica i nomi delle note impostati dopo setRange
    refreshPluginMenus();
    startTimerHz (15);
}

StationPanel::~StationPanel()
{
    stopTimer();
    editors.clear();
    station.onBeforeInstanceRemoved = nullptr;
}

void StationPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f, 1.5f);

    g.setColour (ledColour);
    g.fillEllipse (ledArea.toFloat());
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawEllipse (ledArea.toFloat(), 1.0f);
}

void StationPanel::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto head = r.removeFromTop (32);
    ledArea = head.removeFromLeft (16).withSizeKeepingCentre (14, 14);
    head.removeFromLeft (8);
    title.setBounds (head);

    auto dev = r.removeFromTop (30);
    assignButton.setBounds (dev.removeFromRight (110).reduced (1));
    deviceLabel.setBounds (dev);
    r.removeFromTop (8);

    for (int s = 0; s < numSlots; ++s)
    {
        slotLabels[(size_t) s].setBounds (r.removeFromTop (20));
        auto row = r.removeFromTop (30);
        openButtons[(size_t) s].setBounds (row.removeFromRight (64).reduced (1));
        row.removeFromRight (4);
        slotMenus[(size_t) s].setBounds (row.reduced (1));
        r.removeFromTop (4);
    }
    r.removeFromTop (6);

    auto vol = r.removeFromTop (120);
    auto buttons = vol.removeFromRight (120);
    volume.setBounds (vol);
    buttons.removeFromTop (14);
    muteButton.setBounds (buttons.removeFromTop (36).reduced (2));
    buttons.removeFromTop (8);
    faderButton.setBounds (buttons.removeFromTop (36).reduced (2));
    r.removeFromTop (6);

    auto row = [&r] (juce::Label& l, juce::Component& c)
    {
        auto line = r.removeFromTop (30);
        l.setBounds (line.removeFromLeft (70));
        c.setBounds (line.reduced (1));
        r.removeFromTop (4);
    };
    row (transposeLabel, transpose);
    row (lowLabel, lowNote);
    row (highLabel, highNote);
    row (curveLabel, curveMenu);
    row (gammaLabel, gamma);

    errorLabel.setBounds (r.removeFromBottom (24));
}

//==============================================================================
void StationPanel::loadZoneIntoControls()
{
    const auto z = engine.getZone (index);
    transpose.setValue (z.transpose, juce::dontSendNotification);
    lowNote.setValue (z.lowNote, juce::dontSendNotification);
    highNote.setValue (z.highNote, juce::dontSendNotification);
    curveMenu.setSelectedId ((int) z.curve + 1, juce::dontSendNotification);
    gamma.setValue (z.customGamma, juce::dontSendNotification);
    faderButton.setToggleState (z.faderControlsVolume, juce::dontSendNotification);

    const bool custom = z.curve == VelocityCurve::personalizzata;
    gamma.setVisible (custom);
    gammaLabel.setVisible (custom);
}

void StationPanel::zoneControlChanged()
{
    KeyZone z;
    z.transpose = (int) transpose.getValue();
    z.lowNote = (int) lowNote.getValue();
    z.highNote = (int) highNote.getValue();
    if (z.lowNote > z.highNote)   // gli estremi non si incrociano
    {
        z.highNote = z.lowNote;
        highNote.setValue (z.highNote, juce::dontSendNotification);
    }
    z.curve = (VelocityCurve) juce::jlimit (0, 3, curveMenu.getSelectedId() - 1);
    z.customGamma = (float) gamma.getValue();
    z.faderControlsVolume = faderButton.getToggleState();

    engine.setZone (index, z);
    engine.saveKeyboardSettings();

    const bool custom = z.curve == VelocityCurve::personalizzata;
    gamma.setVisible (custom);
    gammaLabel.setVisible (custom);
}

void StationPanel::assignClicked()
{
    auto& router = engine.keyboards();
    if (router.learningStation() == index)
        router.cancelLearn();
    else
        router.startLearn (index);
}

//==============================================================================
void StationPanel::refreshPluginMenus()
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
            menu.addItem (list[i].name + "  (" + list[i].pluginFormatName + ")", (int) i + 2);

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

juce::AudioPluginInstance* StationPanel::instanceInSlot (int slot) const
{
    return slot == 0 ? station.getInstrument() : station.getEffect (slot - 1);
}

void StationPanel::slotChosen (int slot)
{
    const int id = slotMenus[(size_t) slot].getSelectedId();
    const auto& list = slot == 0 ? instrumentTypes : effectTypes;

    if (id <= 1)
    {
        if (slot == 0) station.setInstrument (nullptr);
        else           station.setEffect (slot - 1, nullptr);
        station.setLoadError ({});
        engine.saveKeyboardSettings();   // cosi' al prossimo avvio la postazione torna com'e' adesso
        return;
    }

    const auto desc = list[(size_t) (id - 2)];
    if (onStatus)
        onStatus ("Carico " + desc.name + "...");

    catalog.createInstanceAsync (desc, engine.getSampleRate(), engine.getBlockSize(),
        [safe = juce::Component::SafePointer<StationPanel> (this), slot]
        (std::unique_ptr<juce::AudioPluginInstance> inst, juce::String error)
        {
            auto* self = safe.getComponent();
            if (self == nullptr)
                return;

            if (self->onStatus)
                self->onStatus ({});

            if (inst == nullptr)
            {
                // Il plugin precedente resta caricato e continua a suonare.
                self->station.setLoadError (error);
                self->refreshPluginMenus();
                return;
            }

            if (slot == 0) self->station.setInstrument (std::move (inst));
            else           self->station.setEffect (slot - 1, std::move (inst));
            self->station.setLoadError ({});
            self->engine.saveKeyboardSettings();
        });
}

void StationPanel::openEditor (int slot)
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
        inst->getName() + "  -  POSTAZIONE " + juce::String (index + 1),
        [safe = juce::Component::SafePointer<StationPanel> (this), inst]
        {
            juce::MessageManager::callAsync ([safe, inst]
            {
                if (auto* self = safe.getComponent())
                    self->editors.erase (inst);
            });
        });
}

void StationPanel::closeEditorFor (juce::AudioPluginInstance& p)
{
    editors.erase (&p);
}

//==============================================================================
void StationPanel::timerCallback()
{
    // Tastiera assegnata e apprendimento
    const bool learning = engine.keyboards().learningStation() == index;
    assignButton.setButtonText (learning ? "PREMI UN TASTO..." : "ASSEGNA");
    assignButton.setToggleState (learning, juce::dontSendNotification);

    const auto a = engine.keyboards().assignmentFor (index);
    const bool connected = engine.isStationDeviceConnected (index);
    deviceLabel.setText (a.has_value() ? a->deviceName.toUpperCase() : "NESSUNA TASTIERA", juce::dontSendNotification);
    deviceLabel.setColour (juce::Label::textColourId, connected || ! a.has_value() ? juce::Colours::white
                                                                                    : juce::Colours::grey);

    // LED: spento = nessuna tastiera o scollegata, verde = collegata, bianco = sta arrivando MIDI
    const auto now = juce::Time::getMillisecondCounter();
    const auto last = engine.lastActivityMs (index);
    juce::Colour led = juce::Colour (0xff1a1a1a);
    if (connected)
        led = (last != 0 && now - last < activityFlashMs) ? juce::Colours::white : StageLookAndFeel::ledGreen;
    if (led != ledColour)
    {
        ledColour = led;
        repaint (ledArea.expanded (2));
    }

    // Il fader della tastiera muove la manopola del volume
    if (! volume.isMouseButtonDown())
        volume.setValue (station.getGainDb(), juce::dontSendNotification);

    errorLabel.setText (station.getLoadError(), juce::dontSendNotification);

    for (int s = 0; s < numSlots; ++s)
        openButtons[(size_t) s].setEnabled (instanceInSlot (s) != nullptr);
}
