# Technical decision record

Last updated: 2026-07-24.

This file prevents future sessions from repeating rejected approaches or
treating temporary diagnostics as the active architecture.

## D001 — Build against the ReXGlue source checkout

The active build uses `tooling/rexglue-sdk`, not the prebuilt SDK package. Audio,
GPU, timing, trace, and kernel fixes live in that source tree. The prebuilt
package is a historical comparison only.

## D002 — Keep the active `default.xex` original

Do not apply a permanent patch to `recomp/fase4/assets/default.xex`. The global
FPS constant experiment did not improve the open world and was unstable when
applied from boot. F10 is diagnostic only.

## D003 — Generated guest C++ is never edited

`native/narutobb/generated/default` is codegen output. Fix the manifest, hooks,
or ReXGlue code generator and regenerate.

## D004 — Add only confirmed, narrow guest-function ranges

Update both manifests only after the exact entry and end are known. Confirm the
address in generated registration output and replay the original scene.

## D005 — Skip the embedded CompareBackEnds debug compiler

Keep the hook for `sub_8217AB20`. The Fox/Jade diagnostic compiler returns
`E_FAIL` in the recompiled environment and leaves a null object. It is not the
game's production renderer.

## D006 — D3D12 RTV is the default rendering path

Use `--render_target_path_d3d12=rtv`. ROV loaded more slowly and produced a
mostly black/partial image on the RX 6650 XT. Do not retry ROV, alternate
stencil, or `ALWAYS` depth transfer without new backend evidence.

## D007 — Cutscene correctness requires real draw extents

Keep `execute_unclipped_draw_vs_on_cpu=true`. With it disabled, a depth/stencil
mask claimed the full EDRAM range with wrap and destroyed the color target.

## D008 — Presentation pacing and simulation rate are separate

The adaptive presenter may smooth content the engine already produces near
60 FPS. It cannot create simulation updates. Open-world 60 FPS requires changes
to the guest/runtime synchronization relationship and validation of simulation
speed.

## D009 — Active audio strategy

Use a 64-frame maximum queue, 16-frame refill, concealment fades, pending-credit
recovery, limiting/sample validation, and safe XMA/FFmpeg cleanup. Dedicated XMA
scheduling is the default; synchronous scheduling is a controlled test only.

## D010 — Reproduce graphics failures offline

Prefer F9 capture, `.xtr` replay, numerical summaries, and draw bisection before
broad renderer changes. Proprietary traces and images remain local and are never
published.

## D011 — Operating-system security is external to runtime correctness

Smart App Control previously blocked unsigned local builds. Do not recommend
weakening host security as a distribution strategy. Public releases require a
trustworthy build and signing/distribution plan.

## D012 — Documentation is part of every material change

Update status and tests after relevant work. Update this record for changed or
rejected approaches, the project map for path/responsibility changes, and
`SKILL.md` or a phase report for new technical findings.

## D013 — Distribute ReXGlue changes as upstream base plus patches

Do not commit the SDK checkout. The portable form is the ordered patch series
in `patches/rexglue-sdk`, applied to pinned upstream commit
`2bdb97f95f154f32d281aaa08446ae007b8ca117`. The expected final tree is
`5144c7af01ce1483a5c59cbde7e419517f5a062e`.

## D014 — Clean build and regressions gate integration

Runtime and build changes must pass `build_clean_windows.ps1` and
`run_regression.ps1` when applicable. Automated coverage does not approve
auditory quality, Story Mode progression, or correct 60 FPS simulation.

## D015 — The 2026-07-22 opening crash belonged to XMA cleanup

Windows failure RVA `0x566F3F` mapped to `avcodec_flush_buffers`. An allocated
`AVCodecContext` may have `codec` before it has been opened. Call flush only when
`avcodec_is_open` succeeds, and keep the automated boot at 30 seconds or longer.

## D016 — Reject the balanced downmix experiment

Manual test `_054` retained the light crackle, failed to restore missing sound,
and introduced dropouts. LFE was nearly inactive, center was active, and no
output clipping occurred. Restore the legacy downmix and do not perform logging,
formatting, or allocation in the real-time SDL callback. Use passive counters
and emit one summary after shutdown.

## D017 — Measure render-queue synchronization before changing timing

In menu, open world, and pause, explicit waits are negligible while
`sub_8219F990` polls queue progress for most of the 33.3 ms frame. Treat high CPU
usage inside that scope as synchronization evidence, not by itself as proof of
simulation or physics cost. Any experiment must be reversible in the runtime
and must measure simulation timing independently. Phase 2 subsequently revised
the interpretation of this evidence; see D019.

## D018 — Public repository contains code and reproducible procedures only

Do not publish game data, XEX files, keys, extracted shaders, media, fonts,
screenshots, GPU traces, generated guest C++, or other proprietary-derived
artifacts. Contributors supply their own legal game files locally. The repository
uses English for all new public documentation and collaboration text.

## D019 — Treat menu 30 FPS as serialized main/render cost, not a removable vblank cap

The menu-only one-vblank queue-release hook executed but output stayed near
30 FPS. The render frame contains approximately 16-17 ms active plus a 16-18 ms
work-event wait, while the main handoff contains approximately 17 ms active plus
a 15-16 ms renderer-ready wait. A temporary zero-timeout wait only moved the
latency into the recursive guest graphics-device ownership acquire.

Reject direct event-wait and device-ownership bypasses. Future 60 FPS work must
profile and reduce the active cost on both serialized legs or establish a
correct parallel ownership handoff, then separately validate simulation delta
and visible speed.

## D020 — Treat 30 FPS as vblank quantization with a separate fixed simulation step

The guarded 120 Hz guest-vblank experiment supersedes the claim that two full
serialized active legs necessarily require 33 ms. At a measured 120-123 guest
vblanks/s, menu and pause produced 60-61 complete frames/s and the open world
produced approximately 57-60 FPS when scene load allowed it. Returning to 60 Hz
guest vblank restored 30 FPS immediately. Keep the vblank intervention in the
runtime, reversible, context-guarded, and opt-in while it is experimental.

Presentation cadence and simulation timing must be changed together. Dynamic
store tracing proved that `sub_82BC8FA8` writes fixed 1/30 deltas at guest PCs
`0x82BC9000/0x82BC9008` even while rendering at 60 FPS, which caused the
player-confirmed 2x animation speed. The controlled correction substitutes
1/60 in the clock configuration field `+76` before the original updater runs,
so derived deltas and internal ticks agree. Capture and restore the original
field value whenever the guarded experiment is disabled; do not patch the XEX.
Manual log `_089` subsequently confirmed 60 FPS with normal animation speed.
This approves the timing relationship, not performance stability: the same run
dropped to 41 FPS with continuous scope tracing. With that trace disabled,
manual log `_090` stayed near 60 with an observed minimum around 56. Because
world-consumer markers are intermittent, use them only to validate the initial
30-frame world context; once active, latch the experiment until explicit F8
suspension instead of oscillating the vblank and simulation clocks.

The same central-clock correction is required in menus. Log `_095` and player
observation confirmed 60.00 FPS with normal UI speed. The user-facing experiment
therefore combines both validated interventions behind one disarmed cvar: F8
enables or restores vblank and simulation timing together across every context.

## D021 — Reject broad half-rate battle-task hooks

Battle and open-world Tracy captures proved that the accelerated battle path is
not another switch of the central clock object and is not explained by the
known 1/30 consumers, collision work, or the sampled scheduler virtual stage.
A reversible every-other-frame skip of `sub_829C17E0` made overall battle
cadence appear correct, but the jutsu visual still ran fast. This separates a
battle state/command cadence from the remaining visual animation timeline; it
does not validate skipping the whole task as a fix.

Do not ship or repeat broad skips of `sub_829C17E0`, `sub_82AAF0B8`, or
`sub_82AB8338`. Halving `sub_82AAF0B8` slowed the fight without correcting the
jutsu, halving `sub_82AB8338` had no visible effect, and the refined probe build
introduced new environment artifacts. All F11 battle probes were removed.
Future work must identify the jutsu-specific animation-state writer or visual
timeline and scale that narrow value while retaining the proven menu/open-world
vblank and central-clock relationship.
