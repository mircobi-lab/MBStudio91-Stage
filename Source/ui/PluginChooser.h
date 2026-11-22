#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "plugins/PluginCatalog.h"

// Elenco dei plugin trovati sul disco, con la spunta su quelli da scansionare.
// Leggere i nomi dei file e' immediato: nessun plugin viene aperto qui dentro.
class PluginChooser final : public juce::Component,
                            private juce::ListBoxModel
{
public:
    PluginChooser (PluginCatalog&, std::function<void (juce::StringArray)> onConfirm);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void rebuildVisible();
    void updateCount();

    PluginCatalog& catalog;
    std::function<void (juce::StringArray)> confirm;

    std::vector<PluginCatalog::FoundPlugin> all;
    std::vector<int> visible;            // indici dentro all
    juce::StringArray chosen;            // chiavi spuntate

    juce::ListBox list { {}, this };
    juce::TextEditor filter;
    juce::Label title, count;
    juce::TextButton none { "NESSUNO" }, known { "SOLO I GIA' NOTI" }, scan { "SCANSIONA I SCELTI" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginChooser)
};
