# Technical decision record

Last updated: 2026-07-23.

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
`62e97f17f8e6cfb4d73905c5158aef1d8d292151`.

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

## D017 — The 30 FPS limit is active render-queue synchronization

In menu, open world, and pause, explicit waits are negligible while
`sub_8219F990` polls queue progress for most of the 33.3 ms frame. Treat high CPU
usage as busy-wait evidence, not proof of useful-work saturation. The first
60 FPS experiment must be reversible in the runtime and must measure simulation
timing independently.

## D018 — Public repository contains code and reproducible procedures only

Do not publish game data, XEX files, keys, extracted shaders, media, fonts,
screenshots, GPU traces, generated guest C++, or other proprietary-derived
artifacts. Contributors supply their own legal game files locally. The repository
uses English for all new public documentation and collaboration text.
