#pragma once

#include <juce_graphics/juce_graphics.h>
#include "control/ControlSurface.h"

// Disegna il display 960x160 del Push 2 per Stage: nome della patch, banco,
// tempo, mute delle postazioni. Stile MBStudio: fondo nero, stampa bianca.
juce::Image renderSurfaceScreen (const SurfaceState&, juce::Typeface::Ptr);
