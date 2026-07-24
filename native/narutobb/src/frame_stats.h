#pragma once

#include <rex/ui/overlay/debug_overlay.h>

// Returns the rate at which the guest submits complete frames to VdSwap.
// This is the useful in-game FPS value, independent of the monitor refresh.
rex::ui::FrameStats GetNarutoFrameStats();

// Enables or disables the frame/wait/vblank/timebase trace and the counters
// for functions that statically read the former 0x820E8B58 candidate.
void RequestNarutoTimingTraceToggle();

// Suspends or rearms the menu-only experiment selected by its launch cvar.
// No intervention can be enabled without an explicit command-line opt-in.
void RequestNarutoMenuExperimentToggle();
