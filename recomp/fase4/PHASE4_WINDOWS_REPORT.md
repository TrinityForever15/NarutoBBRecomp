# Phase 4 — native Windows build and playable-flow chronology

Initial date: 2026-07-19. Updated through 2026-07-23.

This report preserves the engineering chronology. Current operational state is
in `../../docs/STATUS.md`.

## First validated Windows x64 build

The native Clang/CMake/Ninja build initialized D3D12 on an AMD Radeon RX 6650
XT, created a 1280x720 guest frontbuffer, accepted GPU draws, and ran for the
complete controlled interval without an unregistered guest call or fatal
runtime error.

Windows build work included:

- keeping the narrow CompareBackEnds diagnostic-compiler hook;
- enabling the existing `xboxkrnl_usbcam.cpp` implementation;
- retaining conditional USB camera stubs for an older binary SDK package;
- adding game/runtime build integration and diagnostic GPU counters;
- using Clang with the required Windows SDK, C++23, and SSSE3 settings;
- disabling Tracy in the validated source build.

The active public workflow no longer distributes executables or the prebuilt
SDK package. It rebuilds from source and the pinned patch series.

## Indirect guest functions found during Windows testing

The runtime exposed multiple narrow indirect targets as menus, input, and
gameplay progressed. Each confirmed range was added to both manifests and
regenerated. Notable follow-ups included:

- `0x82216E40-0x82216E68` for the first controller-input path;
- `0x82216E10-0x82216E40` for Battle -> Dojo;
- `0x822DC6A8-0x822DC6C0` for Naruto's jutsu;
- `0x8215D000-0x8215D03C` for the post-Orochimaru sequence.

The jutsu replay completed without another missing target. The exact
post-Orochimaru replay remains pending.

## Input

Mouse/keyboard mode is enabled by the launcher and Return maps to Start. An
8BitDo controller was recognized through XInput. F3 displays guest FPS, F9
requests one local GPU capture, and F10 toggles guest timing diagnostics.

## Presentation-pacing experiments

Early instrumentation showed that D3D12 present and the vblank callback were
short; bursty frame production came from title/runtime synchronization rather
than raw GPU saturation. An adaptive presenter was tested for content the game
already produced above roughly 50 FPS, with output around 59.7-60.3 FPS.

This did not make the 30 FPS open world simulate at 60 FPS. Presentation pacing
and simulation frequency are separate. The later phase-1 timing work identified
the active render-queue polling mechanism; see `FPS_PHASE1_REPORT.md`.

## Cutscene graphics investigation

Story cutscenes showed full-black, partial-black, silhouette, and purple
composition frames while gameplay continued. The following did not solve it:

- returning to original 30 FPS behavior;
- full readback synchronization;
- synchronous shader compilation;
- D3D12 ROV;
- alternate stencil output;
- changing depth transfer from `NOT_EQUAL` to `ALWAYS`.

Twelve local GPU traces enabled deterministic headless replay. Draw bisection
located a late full-screen depth-only draw that destroyed EDRAM color ownership.
The root cause was disabled CPU evaluation of the unclipped draw extent. Restoring
`execute_unclipped_draw_vs_on_cpu=true` fixed the local replay set without
regressing known-good frames, and the player confirmed the cutscenes were fully
correct in game.

The captures and screenshots remain local. Only tools, numeric baselines, and
the English technical report are public. Full history:
`HANDOFF_CUTSCENES_GPU_2026-07-19.md`.

## 2026-07-20 audit and failed global FPS candidate

The project audit clarified that `NARUTO_PACER` was a presenter, not a simulation
unlock. A candidate float at `0x820E8B58` held an approximate `1/30`, but its
role was unproven and applying the change from boot destabilized the title.

A later reversible in-memory F10 experiment in the open world produced no
perceptible improvement. Static analysis found 17 direct reads across 16
functions with mixed threshold, multiplication, and initialization uses. The
write was removed. The active XEX remained original throughout the supported
workflow.

## 2026-07-22 native crash fixed

`narutobb_043.log` completed normal initialization, then Windows reported access
violation `rexruntime.dll+0x566F3F` after roughly 20.6 seconds. Diagnostic symbols
and disassembly mapped it to `avcodec_flush_buffers`.

`XmaContext::ClearLocked` checked `AVCodecContext::codec`, but allocation sets
that field before the codec is opened and internal state exists. Replacing the
condition with `avcodec_is_open(av_context_)` removed the null internal-state
access while preserving valid flush behavior.

The active post-fix build survived 45 seconds. A clean independent Release build
passed a 30-second boot and all 12 numerical GPU replays. The automated boot
duration was raised to at least 30 seconds to cover the historical failure point.

## Synchronous XMA comparison

`use_dedicated_xma_thread=false` was added as a controlled diagnostic. It keeps
the graphics baseline, queue size, and original XEX unchanged while running XMA
kicks on the calling thread. Decoder buffer-transition, no-progress, and ring
state corrections were included.

Both modes passed automated boots. Manual logs `_050` and `_051` did not prove
an audible benefit for synchronous scheduling, so the dedicated worker remains
the default.

## Downmix experiment rejected

After the scheduling comparison, the remaining report involved apparently
missing sounds and heavy-sounding dialogue. An optional balanced 5.1-to-stereo
matrix and per-channel diagnostic logger were tested.

Manual log `_054` showed no recovery of the missing sound, no meaningful bass
change, persistent light crackle, and new dropouts. LFE was nearly inactive,
center reached a peak of 0.524, and no output clipping marker appeared. The
matrix was reverted.

The periodic logger also performed formatting and I/O inside the SDL real-time
callback. It was replaced with passive in-memory counters and one
`NARUTO_AUDIO_SILENCE_SUMMARY` emitted after normal shutdown. Automated active
and clean builds proved the mechanism; the exact affected scene is pending.

## 2026-07-23 timing phase 1

Runtime instrumentation measured menu, open world, and pause around 33.3 ms per
frame with only 0.028-0.054 ms of explicit kernel waits. `sub_8219F990` polled
render-queue progress for roughly 31-33 ms, with one `VdSwap` and two vblanks on
average. This proved an active synchronization cap rather than useful-work
saturation.

The renderer delta writer is known, but the simulation delta writer is not.
The next target is the writer of `+64/+68` in the object referenced through
`0x833A30CC`.

F10 trace shutdown was fixed by forwarding F10 without Alt from
`WM_SYSKEYDOWN/UP`; Alt+F4 continues to use normal Windows behavior.

The twelve-patch series reproduced expected tree
`62e97f17f8e6cfb4d73905c5158aef1d8d292151`, and the final integration build
passed invariants, a controlled 30-second boot, and all 12 comparable GPU
baselines. No 60 FPS mode was approved.
