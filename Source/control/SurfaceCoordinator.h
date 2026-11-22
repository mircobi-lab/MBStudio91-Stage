#pragma once

#include "ControlSurface.h"
#include "engine/StageEngine.h"
#include "patches/PatchManager.h"
#include <vector>

// Collega le superfici di controllo (Launchpad, Push 2) a patch, postazioni e tempo.
class SurfaceCoordinator : private juce::Timer
{
public:
    SurfaceCoordinator (StageEngine&, Library&, PatchManager&, bool startTimer = true);
    ~SurfaceCoordinator() override;

    void addSurface (ControlSurface&);

    SurfaceState buildState() const;
    void handleAction (SurfaceAction);
    void handleTap (double nowSeconds);

private:
    void timerCallback() override;

    StageEngine& engine;
    Library& library;
    PatchManager& patches;
    std::vector<ControlSurface*> surfaces;

    JUCE_DECLARE_NON_COPYABLE (SurfaceCoordinator)
};
