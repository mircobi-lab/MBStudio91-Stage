#include "Patch.h"

namespace
{
    const juce::Identifier patchId ("PATCH"), stationId ("STATION"), slotId ("SLOT"), nameId ("name"),
                           pidId ("portableId"), stateId ("state"), roleId ("role"), gainId ("gainDb"),
                           mutedId ("muted"), libraryId ("LIBRERIA"), bankId ("bank"), indexId ("index");

    const std::optional<Patch> emptyPatch;

    juce::ValueTree slotToTree (const PluginSlot& s, int role)
    {
        juce::ValueTree t (slotId);
        t.setProperty (roleId, role, nullptr);   // 0 = strumento, 1..3 = effetti
        t.setProperty (pidId, s.portableId, nullptr);
        t.setProperty (nameId, s.name, nullptr);
        t.setProperty (stateId, s.state.toBase64Encoding(), nullptr);
        return t;
    }

    PluginSlot slotFromTree (const juce::ValueTree& t)
    {
        PluginSlot s;
        s.portableId = t.getProperty (pidId).toString();
        s.name = t.getProperty (nameId).toString();
        s.state.fromBase64Encoding (t.getProperty (stateId).toString());
        return s;
    }
}

//==============================================================================
juce::ValueTree Patch::toValueTree() const
{
    juce::ValueTree t (patchId);
    t.setProperty (nameId, name, nullptr);

    for (const auto& st : stations)
    {
        juce::ValueTree s (stationId);
        s.setProperty (gainId, st.gainDb, nullptr);
        s.setProperty (mutedId, st.muted, nullptr);
        s.appendChild (slotToTree (st.instrument, 0), nullptr);
        for (int i = 0; i < (int) st.effects.size(); ++i)
            s.appendChild (slotToTree (st.effects[(size_t) i], i + 1), nullptr);
        s.appendChild (st.zone.toValueTree(), nullptr);
        t.appendChild (s, nullptr);
    }
    return t;
}

Patch Patch::fromValueTree (const juce::ValueTree& t)
{
    Patch p;
    p.name = t.getProperty (nameId).toString();

    int si = 0;
    for (const auto& s : t)
    {
        if (! s.hasType (stationId) || si >= (int) p.stations.size())
            continue;
        auto& st = p.stations[(size_t) si++];
        st.gainDb = (float) s.getProperty (gainId, 0.0f);
        st.muted = (bool) s.getProperty (mutedId, false);

        for (const auto& child : s)
        {
            if (child.hasType (slotId))
            {
                const int role = (int) child.getProperty (roleId, -1);
                if (role == 0)
                    st.instrument = slotFromTree (child);
                else if (role >= 1 && role <= (int) st.effects.size())
                    st.effects[(size_t) role - 1] = slotFromTree (child);
            }
            else
            {
                st.zone = KeyZone::fromValueTree (child);
            }
        }
    }
    return p;
}

//==============================================================================
bool Library::valid (int bank, int index)
{
    return juce::isPositiveAndBelow (bank, numBanks) && juce::isPositiveAndBelow (index, patchesPerBank);
}

const std::optional<Patch>& Library::get (int bank, int index) const
{
    return valid (bank, index) ? patches[(size_t) (bank * patchesPerBank + index)] : emptyPatch;
}

void Library::set (int bank, int index, Patch p)
{
    if (valid (bank, index))
        patches[(size_t) (bank * patchesPerBank + index)] = std::move (p);
}

void Library::clear (int bank, int index)
{
    if (valid (bank, index))
        patches[(size_t) (bank * patchesPerBank + index)].reset();
}

void Library::rename (int bank, int index, const juce::String& newName)
{
    if (valid (bank, index))
        if (auto& p = patches[(size_t) (bank * patchesPerBank + index)])
            p->name = newName;
}

bool Library::bankHasPatches (int bank) const
{
    if (! juce::isPositiveAndBelow (bank, numBanks))
        return false;
    for (int i = 0; i < patchesPerBank; ++i)
        if (patches[(size_t) (bank * patchesPerBank + i)].has_value())
            return true;
    return false;
}

std::optional<std::pair<int, int>> Library::neighbour (int bank, int index, int direction) const
{
    const int total = numBanks * patchesPerBank;
    const int step = direction >= 0 ? 1 : -1;
    const int start = valid (bank, index) ? bank * patchesPerBank + index : 0;

    for (int k = 1; k <= total; ++k)
    {
        const int pos = ((start + step * k) % total + total) % total;
        if (patches[(size_t) pos].has_value())
            return std::make_pair (pos / patchesPerBank, pos % patchesPerBank);
    }
    return std::nullopt;
}

juce::ValueTree Library::toValueTree() const
{
    juce::ValueTree t (libraryId);
    for (int i = 0; i < (int) patches.size(); ++i)
        if (patches[(size_t) i].has_value())
        {
            auto pt = patches[(size_t) i]->toValueTree();
            pt.setProperty (bankId, i / patchesPerBank, nullptr);
            pt.setProperty (indexId, i % patchesPerBank, nullptr);
            t.appendChild (pt, nullptr);
        }
    return t;
}

void Library::fromValueTree (const juce::ValueTree& t)
{
    for (auto& p : patches)
        p.reset();
    for (const auto& pt : t)
        if (pt.hasType (patchId))
            set ((int) pt.getProperty (bankId, -1), (int) pt.getProperty (indexId, -1), Patch::fromValueTree (pt));
}

bool Library::saveTo (const juce::File& f) const
{
    if (auto xml = toValueTree().createXml())
        return xml->writeTo (f);
    return false;
}

bool Library::loadFrom (const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr)
        return false;
    fromValueTree (juce::ValueTree::fromXml (*xml));
    return true;
}

juce::File Library::dailyBackupFor (const juce::File& libraryFile, juce::Time when)
{
    return libraryFile.getSiblingFile (libraryFile.getFileNameWithoutExtension() + "-"
                                       + when.formatted ("%Y-%m-%d") + libraryFile.getFileExtension());
}
