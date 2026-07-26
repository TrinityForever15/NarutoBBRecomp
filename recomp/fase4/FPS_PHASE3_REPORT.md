# FPS phase 3: vblank quantization, simulation timing, and battle boundary

Date: 2026-07-24

> **Historical report.** Phase 4 direct speed telemetry superseded the battle
> diagnosis and resume point below. Battle is a fixed-timestep context that the
> default port was already running near twice speed; the 60 FPS experiment was
> masking that baseline defect. Continue from `FPS_PHASE4_REPORT.md`.

## Result

The title's default 30 FPS cadence is quantized by the guest-visible vblank
rate. A reversible runtime multiplier that changes guest vblank from 60 to
120 Hz lets menus and pause produce 60-61 complete guest frames per second and
lets the open world produce approximately 57-60 FPS when scene cost permits.
The original XEX remains unchanged.

Rendering cadence and simulation timing are separate. At 120 Hz vblank, the
central updater `sub_82BC8FA8` continued deriving 1/30 deltas until the runtime
hook supplied 1/60 in configuration field `+76`. With both changes active,
manual tests confirmed correctly timed menus and sampled open-world gameplay.
Clean observation found a minimum around 55-56 FPS with little perceptible
instability.

This combination is not a global 60 FPS solution. Battles retain the corrected
central clock but run some animations, including the visible jutsu sequence,
too quickly. Broad battle-task throttles did not provide a complete or safe
correction and were removed.

## Safety and scope

- The active `default.xex` was never modified.
- Every intervention is implemented in runtime or game-project hooks, is
  disabled by default, and is reversible through F8.
- Generated guest C++ was inspected but never edited manually.
- Tracy captures, logs, game data, and other proprietary-derived artifacts are
  local evidence and are not part of the repository.
- The normal launcher remains at original timing.

## Decisive vblank experiment

The runtime vblank worker was extended with an opt-in multiplier and a Windows
high-resolution wait path for sub-16.7 ms intervals. Measurements showed
120-123 guest vblank pulses per second with zero coalesced intervals in the
validated samples. Returning the multiplier to one immediately restored the
approximately 30 FPS cadence.

This revised the phase-2 interpretation: the two approximately one-refresh
pipeline legs were quantized by guest vblank and were not proof of an
irreducible 32 ms workload. Event and recursive graphics-device ownership
bypasses remain rejected because they merely moved latency and violated the
handoff model.

## Central simulation clock

Dynamic store tracing identified the authoritative writes:

```text
0x82BC9000 -> simulation_clock+68 = 0x3D088889 (1/30)
0x82BC9008 -> simulation_clock+64 = 0x3D088889 (1/30)
```

Both instructions belong to `sub_82BC8FA8`. Field `+76` is the configured fixed
step from which `+64`, `+68`, and internal tick state are derived. The retained
hook captures the original `+76`, supplies `0x3C888889` (1/60) before invoking
the original updater, and restores the captured bits when F8 disables the
experiment. Overwriting only the derived fields after the call is rejected
because it would leave internal ticks inconsistent.

Manual evidence:

- `_089`: 60 FPS and normal open-world animation speed; the traced build
  dropped as low as 41 FPS under heavy instrumentation.
- `_090`: trace-disabled observation stayed near 60 FPS with a reported
  minimum around 56 FPS.
- `_092`: the guarded world mode latched at 120 Hz/1/60 without oscillating
  during marker gaps; reported minimum was approximately 55-56 FPS.
- `_095`: menu reached 60.00 FPS with normal UI speed.
- `_097`: unified menu and open-world timing passed, but battle animation was
  accelerated while the same corrected clock remained active.

## Battle and world profiling

Tracy was enabled in a separate profiling build. Bounded capture launchers and
CLI capture/export tools were used so profiling did not alter the normal build.
The local samples contained:

| Context | Profiled frames | Guest functions | Interpretation |
|---|---:|---:|---|
| Battle | 530 | 3,879 | Ten-second battle window |
| Open world | 147 | 3,398 | Heavy profiling overhead; normalize per frame |

The comparison reduced the search to battle-only and battle-amplified work.
Static inspection rejected the largest collision and spatial candidates. The
known timing readers did not reveal a second central clock. Combat scheduler
instrumentation also found no eligible object in the sampled vtable +16 stage.

Notable battle paths:

- `sub_829D8980` is a once-per-frame high-level battle task that calls
  `sub_829341E0` and returns 1000.
- `sub_829C17E0` is a once-per-frame thin task wrapper over the large
  `sub_829C11A8` state/command path and returns 1000.
- `sub_82AB2BB8` runs in both battle and world, but calls large
  `sub_82AAF0B8` only in sampled battle state 7.
- `sub_82AB8338` is a battle-only task active for most of the captured fight.

## Rejected battle half-rate probes

The probes skipped a selected task on alternating 60 Hz frames and were enabled
only by a dedicated cvar and F11 binding. They were never proposed as XEX
patches.

The first probe compared `sub_829341E0`, `sub_829C17E0`, and `sub_829D8980`.
The player found the `sub_829C17E0` mode best: battle cadence appeared normal,
but the visible jutsu still ran quickly. This separates broad battle
state/command cadence from the remaining visual animation path, but skipping a
whole task is too broad to retain.

The refined probe kept `sub_829C17E0` as a base and tested visual candidates:

| Candidate | Observation | Decision |
|---|---|---|
| `sub_82AAF0B8` | Slowed the fight; jutsu speed remained wrong | Rejected |
| `sub_82AB8338` | No perceptible change | Rejected |
| Combined refined build | New environment artifacts were observed | Remove all probe hooks |

The battle cvar, F11 binding, half-rate wrappers, and dedicated launcher were
removed before integration. No battle timing correction is active in the
published executable.

## Retained implementation

- Runtime patch 0013 exposes guest wait attribution and live timing needed by
  F10 diagnostics.
- Runtime patch 0014 adds the reversible guest-vblank multiplier and the
  high-resolution Windows wait path.
- `naruto_60fps_experiment` keeps the validated vblank and central simulation
  clocks together behind disarmed F8 control.
- Tracy guest-function profiling is an opt-in separate build; the normal build
  does not enable it.

## Resume point

When the investigation resumes:

1. Keep the validated 120 Hz vblank and central 1/60 clock unchanged.
2. Capture or instrument the exact jutsu visual sequence, focusing on the
   animation-state/timeline writer rather than broad task scheduling.
3. Compare the same jutsu at original 30 FPS and experimental 60 FPS, including
   state duration, visual frame index, and effect lifetime.
4. Do not repeat half-rate skips of `sub_829C17E0`, `sub_82AAF0B8`, or
   `sub_82AB8338` without new evidence.
5. Require a full battle with normal gameplay cadence, character animation,
   jutsu visuals, and no environment artifacts before approving global 60 FPS.
