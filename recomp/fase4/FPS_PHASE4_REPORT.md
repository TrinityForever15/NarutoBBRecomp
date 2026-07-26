# FPS phase 4: per-context timing and the fixed-step correction

Date: 2026-07-25

## Result

Direct simulation-speed telemetry resolved the battle timing problem that phase
3 had attributed to a second animation timeline. Menus and the open world use a
variable timestep and remain correctly timed at different frame rates. Battle
uses the same central clock in fixed mode, advances one configured step per
produced frame, and is not vblank-limited by the port.

The default configuration was therefore already running battle near twice
speed: approximately 60 produced frames per second multiplied by a 1/30 fixed
step. The earlier 60 FPS experiment supplied a 1/60 step and masked this
baseline defect; it did not create the accelerated battle.

The retained correction grants a fixed-step frame no more simulation time than
real time delivered for that frame. Sampled battle now measures a 1.000 median
speed factor at a 60 FPS target. A 120 FPS target reached a 119.37 FPS median at
1.000 speed, but remains experimental because frame-counted combat behavior has
not been validated.

## Safety and scope

- The active `default.xex` was not modified.
- Generated guest C++ was not edited manually.
- The correction is implemented in the game-project hook for the original
  `sub_82BC8FA8` clock updater.
- The presentation target is a runtime cvar exported as portable ReXGlue patch
  0015; patch 0014 still owns the reversible guest-vblank multiplier.
- Local logs, Tracy captures, game data and other proprietary-derived artifacts
  remain untracked.
- Broad half-rate battle hooks remain removed and rejected.

## Decisive speed measurement

The updater writes the raw simulation delta at clock field `+68` and the scaled
delta at `+64` on every call. Summing those deltas and dividing by real elapsed
time gives a direct speed factor:

```text
raw_speed = simulated raw seconds / real seconds
```

A value of 1.000 is real-time speed, 2.000 is double speed and 0.500 is half
speed. Log `_105` classified the contexts:

| Context | `+72` fixed mode | FPS | Step | Raw speed |
|---|---:|---:|---|---:|
| Menu and open world | 0 | 30 and 60 | variable | 1.000 |
| Battle, experiment active | 1 | about 60 | 1/60 | 1.01 |
| Battle, experiment suspended | 1 | about 60 | 1/30 | 1.99 |

This measurement supersedes the phase-3 conclusion that battle had a second
jutsu-specific timing source. Static analysis also found one call site,
`sub_827F73F0`, and one clock instance at `0x833A30CC`.

## Clock structure and context changes

Relevant fields in the central clock are:

| Offset | Role |
|---:|---|
| `+40`, `+48` | primary accumulators |
| `+56` | delta in ticks |
| `+64`, `+68` | scaled and raw simulation deltas |
| `+72` | fixed-mode flag |
| `+76` | configured fixed step |
| `+80` | title maximum-delta clamp |
| `+92`, `+96` | time scales |
| `+104`, `+112` | secondary accumulators |
| `+136` | absolute timestamp |

Field `+76` is not constant for the whole session. Logs `_095`, `_097` and
`_098` observed `0x3D088889`; logs `_100` through `_103` observed
`0x3C87FCB9`. Capturing the original value only when the clock pointer changes
is wrong because the pointer is stable while the title reconfigures the step.

The hook now distinguishes a title-written value from its own last replacement,
tracks each title change, and restores the most recent original value. Values
outside a plausible 1-100 ms range are left untouched.

## Real-time fixed-step limit

The guest loop can produce occasional 2-4 ms frames, especially while drawing a
cheap two-dimensional status portrait. Before the correction, each burst frame
still advanced battle by a full nominal step and produced an instantaneous
speed factor between roughly 4x and 12x.

The correction accumulates real elapsed time as simulation credit. A fixed-step
frame receives the lesser of available credit and the title's own maximum delta,
with a small positive floor so step-driven state machines cannot stall. Stable
frames continue receiving the exact nominal step.

A simpler nominal ceiling was tested and rejected. It corrected frames faster
than the target but forced every slower frame into slow motion. Across samples
between 30 and 110 FPS, mean speed was 0.864 with a 0.282 minimum before removing
that ceiling and 1.000 with a 0.650 minimum afterward.

## One target for simulation, pacing and vblank

`naruto_target_frame_rate`, runtime `naruto_pacing_target_hz`, and the guest
vblank multiplier are supplied from one launcher target. This removes the prior
61.5 Hz presentation target against a 1/60 step, which caused a permanent 2.5%
battle overspeed.

The 60 FPS launcher arms the experiment after boot. F8 still suspends the
vblank/target intervention and restores original timing for A/B tests. The
real-time fixed-step correction remains active because the twice-speed battle
bug also exists with the experiment suspended.

Manual evidence:

- `_109`: 62 sampled battle windows, raw-speed median 1.000, minimum 0.928 and
  maximum 1.245, with no key press required;
- `_111`: 120 FPS target, achieved median 119.37 FPS and raw-speed median 1.000;
- sampled menus and open world stayed at a 1.000 speed factor in variable mode.

## Integration validation

The independent Release build at
`native/narutobb/out/build/verification-phase4-audit` completed configure,
codegen, all 97 guest translation units, game/runtime linking and replay-tool
linking.

```text
narutobb.exe             60E97AFD5CA38F016DA06BDE509CFDBCFBD2218DA911C30BF04288E6552473A4
rexruntime.dll           13D81713BBA4E494C29CE64CAAE3E9EB7DB5F3A17976B995348337B1BDFECC6A
narutobb_trace_dump.exe  44FBB1F8C2A67A51063F7EE490D3C02057AA4BBDC0941F21E6513E9FDF2E6B84
```

Project invariants and the controlled 30-second boot passed. All 12 approved
GPU traces reproduced with zero mean-RGB and non-black-pixel delta. Two other
local captures had no public baseline and were explicitly skipped as
non-comparable; they were not promoted into the baseline.

## Status-jutsu cut-in

The portrait cut-in did not require a dedicated animation-timeline writer. It
made rendering temporarily cheap enough for the unbound guest loop to emit
burst frames, and those frames exposed the fixed-step defect. The real-time
limit corrects that source.

The cut-in's complete duration has not been compared at 30 and 60 FPS. The
bounded Tracy launchers and `analyze_cutin.py` exist for that measurement; they
are evidence tools and do not modify the runtime or XEX.

## Approval boundary

A 1.000 speed factor proves the central clock is correct. It does not prove
that logic counted in frames is unchanged. Before approving global 60 FPS:

1. compare status-jutsu cut-in duration at 30 and 60 FPS;
2. complete a full battle while checking combo windows, invulnerability, input
   buffering, animation and physics;
3. profile scene-dependent dips as a smoothness issue;
4. repeat the clean build and 12-trace GPU regression after further code changes;
5. keep 120 FPS experimental until the same combat checks pass at that target.

Phase-3 half-rate probes, event-wait bypasses, graphics-device ownership
bypasses and the `0x820E8B58` write remain rejected.
