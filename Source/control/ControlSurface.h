#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <functional>

// Azioni comuni a tutte le superfici di controllo (Launchpad Mini MK1, poi Push 2).
enum class SurfaceActionType { none, patch, bank, mute, previous, next, tap, panicDown, panicUp, panic,
                               stationVolume,   // index = postazione, delta = scatti del rotativo
                               masterVolume,    // delta = scatti
                               tempoDelta };    // delta = scatti, un BPM a scatto

struct SurfaceAction
{
    SurfaceActionType type = SurfaceActionType::none;
    int index = 0;
    int delta = 0;   // rotativi: scatti, positivi verso destra
};

// Quello che una superficie deve mostrare.
struct SurfaceState
{
    int bank = 0;
    int activePatch = -1;    // nel banco mostrato, -1 se la patch attiva e' in un altro banco
    int loadingPatch = -1;
    std::array<bool, 64> occupied {};
    std::array<bool, 8> bankOccupied {};
    std::array<bool, 4> muted {};
    bool tapLit = false;
    juce::String patchName;  // per le superfici con display
    bool modified = false;
    double bpm = 120.0;

    bool operator== (const SurfaceState& o) const
    {
        return bank == o.bank && activePatch == o.activePatch && loadingPatch == o.loadingPatch
            && occupied == o.occupied && bankOccupied == o.bankOccupied && muted == o.muted
            && tapLit == o.tapLit && patchName == o.patchName && modified == o.modified
            && juce::approximatelyEqual (bpm, o.bpm);
    }
    bool operator!= (const SurfaceState& o) const { return ! (*this == o); }
};

class ControlSurface
{
public:
    virtual ~ControlSurface() = default;
    virtual juce::String getName() const = 0;
    virtual bool isConnected() const = 0;
    virtual void showState (const SurfaceState&) = 0;   // message thread

    std::function<void (SurfaceAction)> onAction;       // chiamato sul message thread
};
