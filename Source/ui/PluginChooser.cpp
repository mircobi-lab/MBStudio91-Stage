#include "PluginChooser.h"
#include "StageLookAndFeel.h"

PluginChooser::PluginChooser (PluginCatalog& c, std::function<void (juce::StringArray)> onConfirm)
    : catalog (c), confirm (std::move (onConfirm))
{
    all = catalog.findPluginFiles();
    chosen = catalog.getChosenPlugins();

    // Primo avvio senza scelte: si parte da quelli gia' noti, cosi' non si riparte da zero.
    if (chosen.isEmpty())
        for (const auto& p : all)
            if (p.alreadyKnown)
                chosen.add (p.key());

    title.setText ("Spunta i plugin da scansionare. Aprirli e' la parte lenta: meno ne scegli, "
                   "piu' in fretta finisce.", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (title);

    filter.setTextToShowWhenEmpty ("filtra per nome...", juce::Colours::grey);
    filter.onTextChange = [this] { rebuildVisible(); };
    addAndMakeVisible (filter);

    list.setRowHeight (26);
    addAndMakeVisible (list);

    count.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (count);

    none.onClick = [this] { chosen.clear(); list.repaint(); updateCount(); };
    known.onClick = [this]
    {
        chosen.clear();
        for (const auto& p : all)
            if (p.alreadyKnown)
                chosen.add (p.key());
        list.repaint();
        updateCount();
    };
    scan.onClick = [this] { if (confirm) confirm (chosen); };

    for (auto* b : { &none, &known, &scan })
        addAndMakeVisible (*b);

    rebuildVisible();
    setSize (640, 520);
}

void PluginChooser::rebuildVisible()
{
    const auto text = filter.getText().trim();
    visible.clear();
    for (int i = 0; i < (int) all.size(); ++i)
        if (text.isEmpty() || all[(size_t) i].displayName.containsIgnoreCase (text))
            visible.push_back (i);

    list.updateContent();
    list.repaint();
    updateCount();
}

void PluginChooser::updateCount()
{
    count.setText ("Trovati " + juce::String ((int) all.size()) + ", scelti "
                   + juce::String (chosen.size()), juce::dontSendNotification);
}

int PluginChooser::getNumRows() { return (int) visible.size(); }

void PluginChooser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size()))
        return;

    const auto& p = all[(size_t) visible[(size_t) row]];
    const bool ticked = chosen.contains (p.key());

    if (row % 2)
        g.fillAll (juce::Colour (0xff0e0e0e));

    juce::Rectangle<float> box (8.0f, (float) height * 0.5f - 7.0f, 14.0f, 14.0f);
    g.setColour (ticked ? StageLookAndFeel::ledGreen : juce::Colour (0xff4a4a4a));
    ticked ? g.fillRect (box) : g.drawRect (box, 1.0f);

    g.setColour (ticked ? juce::Colours::white : juce::Colour (0xff9b9da2));
    g.setFont (StageLookAndFeel::titleFont (17.0f));
    g.drawText (p.displayName, 32, 0, width - 140, height, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff6c6e72));
    g.setFont (StageLookAndFeel::titleFont (15.0f));
    g.drawText (p.formatName + (p.alreadyKnown ? "  NOTO" : ""), width - 140, 0, 132, height,
                juce::Justification::centredRight);
}

void PluginChooser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size()))
        return;

    const auto key = all[(size_t) visible[(size_t) row]].key();
    if (chosen.contains (key))
        chosen.removeString (key);
    else
        chosen.add (key);

    list.repaintRow (row);
    updateCount();
}

void PluginChooser::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }

void PluginChooser::resized()
{
    auto r = getLocalBounds().reduced (12);
    title.setBounds (r.removeFromTop (44));
    filter.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    auto bottom = r.removeFromBottom (36);
    scan.setBounds (bottom.removeFromRight (190).reduced (2));
    bottom.removeFromRight (8);
    known.setBounds (bottom.removeFromRight (160).reduced (2));
    bottom.removeFromRight (8);
    none.setBounds (bottom.removeFromRight (110).reduced (2));
    count.setBounds (bottom);

    r.removeFromBottom (8);
    list.setBounds (r);
}
