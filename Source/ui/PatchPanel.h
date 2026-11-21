#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "patches/PatchManager.h"

// Banchi A-H, griglia 8x8 come la Launchpad, salvataggio e gestione delle patch.
class PatchPanel final : public juce::Component,
                         private juce::Timer
{
public:
    PatchPanel (PatchManager&, Library&);
    ~PatchPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onLibraryChanged;   // da salvare su disco

private:
    void timerCallback() override;
    juce::Rectangle<int> padBounds (int index) const;
    int padAt (juce::Point<int>) const;
    void saveClicked();
    void renameClicked();
    void pasteClicked();
    void deleteClicked();
    void askName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)>);
    void confirm (const juce::String& question, std::function<void()> onYes);
    void libraryChanged();

    PatchManager& patches;
    Library& library;

    juce::Label title, nameLabel;
    std::array<juce::TextButton, Library::numBanks> bankButtons;
    juce::TextButton saveButton { "SALVA" }, renameButton { "RINOMINA" }, copyButton { "COPIA" },
                     pasteButton { "INCOLLA" }, deleteButton { "CANCELLA" };
    juce::Rectangle<int> gridArea;
    int selected = 0;
    std::optional<Patch> clipboard;
    bool flashOn = false;

    std::unique_ptr<juce::AlertWindow> dialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchPanel)
};
