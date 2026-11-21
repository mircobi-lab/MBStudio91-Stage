#include "PatchPanel.h"
#include "StageLookAndFeel.h"

namespace
{
    const juce::Colour padEmpty  { 0xff161616 };
    const juce::Colour padAmber  { 0xffd99a1e };
    const juce::Colour padGreen  { 0xff3ddc4a };
    const juce::Colour padRed    { 0xffd8261c };

    juce::String bankLetter (int b) { return juce::String::charToString ((juce::juce_wchar) ('A' + b)); }
}

PatchPanel::PatchPanel (PatchManager& pm, Library& lib) : patches (pm), library (lib)
{
    title.setText ("PATCH", juce::dontSendNotification);
    title.setFont (StageLookAndFeel::titleFont (26.0f));
    addAndMakeVisible (title);

    nameLabel.setFont (StageLookAndFeel::titleFont (20.0f));
    nameLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (nameLabel);

    for (int b = 0; b < Library::numBanks; ++b)
    {
        auto& btn = bankButtons[(size_t) b];
        btn.setButtonText (bankLetter (b));
        btn.onClick = [this, b] { patches.setActiveBank (b); repaint(); };
        addAndMakeVisible (btn);
    }

    saveButton.onClick   = [this] { saveClicked(); };
    renameButton.onClick = [this] { renameClicked(); };
    copyButton.onClick   = [this] { clipboard = library.get (patches.activeBank(), selected); };
    pasteButton.onClick  = [this] { pasteClicked(); };
    deleteButton.onClick = [this] { deleteClicked(); };
    for (auto* b : { &saveButton, &renameButton, &copyButton, &pasteButton, &deleteButton })
        addAndMakeVisible (*b);

    startTimerHz (10);
}

PatchPanel::~PatchPanel()
{
    stopTimer();
}

//==============================================================================
void PatchPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    title.setBounds (r.removeFromTop (32));
    nameLabel.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    auto banks = r.removeFromTop (34);
    const int bw = banks.getWidth() / Library::numBanks;
    for (auto& b : bankButtons)
        b.setBounds (banks.removeFromLeft (bw).reduced (2));
    r.removeFromTop (10);

    const int side = juce::jmin (r.getWidth(), r.getHeight() - 140);
    gridArea = r.removeFromTop (side).withWidth (side);
    r.removeFromTop (12);

    auto row1 = r.removeFromTop (36);
    const int w1 = row1.getWidth() / 2;
    saveButton.setBounds (row1.removeFromLeft (w1).reduced (2));
    renameButton.setBounds (row1.reduced (2));
    r.removeFromTop (4);
    auto row2 = r.removeFromTop (36);
    const int w2 = row2.getWidth() / 3;
    copyButton.setBounds (row2.removeFromLeft (w2).reduced (2));
    pasteButton.setBounds (row2.removeFromLeft (w2).reduced (2));
    deleteButton.setBounds (row2.reduced (2));
}

juce::Rectangle<int> PatchPanel::padBounds (int index) const
{
    const float cell = (float) gridArea.getWidth() / 8.0f;
    const int row = index / 8, col = index % 8;
    return juce::Rectangle<float> (gridArea.getX() + col * cell, gridArea.getY() + row * cell, cell, cell)
               .reduced (2.0f).toNearestInt();
}

int PatchPanel::padAt (juce::Point<int> p) const
{
    for (int i = 0; i < Library::patchesPerBank; ++i)
        if (padBounds (i).contains (p))
            return i;
    return -1;
}

void PatchPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f, 1.5f);

    const int bank = patches.activeBank();
    const bool activeHere = patches.activePatchBank() == bank;

    g.setFont (StageLookAndFeel::titleFont (11.0f));
    for (int i = 0; i < Library::patchesPerBank; ++i)
    {
        const auto r = padBounds (i).toFloat();
        const auto& p = library.get (bank, i);

        juce::Colour c = padEmpty;
        if (patches.isLoading() && patches.loadingPatch() == i && bank == patches.activeBank())
            c = flashOn ? padRed : padEmpty;
        else if (activeHere && patches.activePatch() == i)
            c = padGreen;
        else if (p.has_value())
            c = padAmber;

        g.setColour (c);
        g.fillRoundedRectangle (r, 3.0f);

        if (i == selected)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 2.0f);
        }

        if (p.has_value())
        {
            g.setColour (c == padEmpty ? juce::Colours::white : juce::Colours::black);
            g.drawFittedText (p->name.toUpperCase(), r.reduced (2.0f).toNearestInt(), juce::Justification::centred, 2, 0.7f);
        }
    }
}

void PatchPanel::mouseDown (const juce::MouseEvent& e)
{
    const int pad = padAt (e.getPosition());
    if (pad < 0)
        return;

    selected = pad;
    // Clic = richiama (se c'e' una patch); Maiusc/Alt + clic = solo selezione.
    if (! (e.mods.isShiftDown() || e.mods.isAltDown()) && library.get (patches.activeBank(), pad).has_value())
        patches.recall (patches.activeBank(), pad);
    repaint();
}

void PatchPanel::timerCallback()
{
    flashOn = ! flashOn;

    for (int b = 0; b < Library::numBanks; ++b)
    {
        auto& btn = bankButtons[(size_t) b];
        const bool active = b == patches.activeBank();
        btn.setColour (juce::TextButton::buttonColourId,
                       active ? padGreen.darker (0.4f) : (library.bankHasPatches (b) ? padAmber.darker (0.6f) : juce::Colour (0xff121212)));
    }

    juce::String name = "NESSUNA PATCH";
    if (const auto& p = library.get (patches.activePatchBank(), patches.activePatch()))
        name = bankLetter (patches.activePatchBank()) + "-" + juce::String (patches.activePatch() + 1).paddedLeft ('0', 2)
             + "  " + p->name.toUpperCase() + (patches.isModified() ? "  *" : "");
    if (patches.isLoading())
        name = "CARICO...";
    nameLabel.setText (name, juce::dontSendNotification);

    const bool hasSel = library.get (patches.activeBank(), selected).has_value();
    renameButton.setEnabled (hasSel);
    copyButton.setEnabled (hasSel);
    deleteButton.setEnabled (hasSel);
    pasteButton.setEnabled (clipboard.has_value());

    repaint (gridArea.expanded (4));
}

//==============================================================================
void PatchPanel::askName (const juce::String& titleText, const juce::String& initial, std::function<void (juce::String)> done)
{
    dialog = std::make_unique<juce::AlertWindow> (titleText, "Nome della patch:", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor ("nome", initial);
    dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("ANNULLA", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<PatchPanel> (this), done] (int result)
        {
            auto* self = safe.getComponent();
            if (self == nullptr || self->dialog == nullptr)
                return;
            const auto text = self->dialog->getTextEditorContents ("nome").trim();
            self->dialog = nullptr;
            if (result == 1 && text.isNotEmpty())
                done (text);
        }), false);
}

void PatchPanel::confirm (const juce::String& question, std::function<void()> onYes)
{
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "MBStudio91 Stage", question,
                                        "SI", "NO", this,
                                        juce::ModalCallbackFunction::create ([onYes] (int r) { if (r == 1) onYes(); }));
}

void PatchPanel::libraryChanged()
{
    repaint();
    if (onLibraryChanged)
        onLibraryChanged();
}

void PatchPanel::saveClicked()
{
    const int bank = patches.activeBank(), index = selected;
    const auto& existing = library.get (bank, index);
    const auto initial = existing.has_value() ? existing->name : "PATCH " + bankLetter (bank) + juce::String (index + 1);

    auto doSave = [this, bank, index] (juce::String name)
    {
        patches.saveCurrentTo (bank, index, name);
        libraryChanged();
    };

    askName ("Salva patch", initial, [this, bank, index, doSave] (juce::String name)
    {
        if (library.get (bank, index).has_value() && ! (patches.activePatchBank() == bank && patches.activePatch() == index))
            confirm ("Sovrascrivere la patch " + bankLetter (bank) + "-" + juce::String (index + 1) + "?", [doSave, name] { doSave (name); });
        else
            doSave (name);
    });
}

void PatchPanel::renameClicked()
{
    const int bank = patches.activeBank(), index = selected;
    if (const auto& p = library.get (bank, index))
        askName ("Rinomina patch", p->name, [this, bank, index] (juce::String name)
        {
            library.rename (bank, index, name);
            libraryChanged();
        });
}

void PatchPanel::pasteClicked()
{
    if (! clipboard.has_value())
        return;
    const int bank = patches.activeBank(), index = selected;
    auto doPaste = [this, bank, index] { library.set (bank, index, *clipboard); libraryChanged(); };
    if (library.get (bank, index).has_value())
        confirm ("Sostituire la patch " + bankLetter (bank) + "-" + juce::String (index + 1) + " con quella copiata?", doPaste);
    else
        doPaste();
}

void PatchPanel::deleteClicked()
{
    const int bank = patches.activeBank(), index = selected;
    if (! library.get (bank, index).has_value())
        return;
    confirm ("Cancellare la patch " + bankLetter (bank) + "-" + juce::String (index + 1) + "?",
             [this, bank, index] { library.clear (bank, index); libraryChanged(); });
}
