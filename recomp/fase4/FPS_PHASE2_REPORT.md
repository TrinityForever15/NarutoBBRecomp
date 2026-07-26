# FPS phase 2: controlled menu experiment

Date: 2026-07-23

## Result

The menu does not have a removable two-vblank sleep. The controlled runtime
intervention shortened the render-queue poll to one real guest vblank and was
confirmed active, but output remained near 30 FPS. The measured frame is a
serialized main/render pipeline in which both sides consume approximately one
refresh interval before handing ownership to the other.

This revises the phase-1 interpretation. The two vblanks are an effect of two
serialized pipeline legs, not one explicit request to wait for two vblanks.
There is no approved 60 FPS mode.

## Scope and safety

- The active `default.xex` was not modified.
- All intervention logic is in runtime hooks and is disabled by default.
- The experiment requires the `naruto_menu_60fps_experiment` cvar.
- F8 suspends or rearms it, and F10 controls timing telemetry.
- A 30-frame context guard enables the intervention only while the two
  world-only timing consumers observed in phase 1 are absent.
- Seeing `sub_8276E338` or `sub_8281CEF0` automatically disarms the experiment
  and clears the rearm request before gameplay can continue under the hook.
- The normal launcher is unchanged.

The diagnostic launcher is
`launchers/Play Naruto - Menu 60 FPS Experiment.bat`. It reproduces the rejected
mechanism; it is not a user-facing 60 FPS mode.

## Runtime instrumentation

The runtime now exposes a live guest-vblank pulse count and attributes the
longest wait to its guest link-register caller. Game-side hooks add:

- a full-scope probe around main-thread handoff `sub_82160E28`;
- caller/thread aggregation around wait wrapper `sub_821F8438`;
- an opt-in scope around queue poll `sub_8219F990`;
- a hook on poll helper `sub_821A1858` that preserves the original result until
  one real guest vblank passes, then returns "not waiting" for that invocation;
- an experiment summary reporting calls, forced releases, vblank count, queue
  distance, and the guest caller.

The existing `sub_821B1DD0` boundary, `VdSwap`, timebase, wait, and pacer
telemetry remains active.

## Final menu run

The final player-observed run used the opt-in launcher, stayed in the front-end
menu, and produced local log `narutobb_075.log`. The player reported that it
still looked like 30 FPS. Stable one-second samples showed:

| Scope | Wall time | Active time | Blocked time | Vblank | Interpretation |
|---|---:|---:|---:|---:|---|
| Complete render frame, thread 15 | 33.5-34.5 ms | 16.6-17.2 ms | 16.6-17.9 ms | 2.0/frame | One active render leg plus one work-event wait |
| Main handoff `sub_82160E28`, thread 6 | 32.2-33.1 ms | 16.6-17.9 ms | 15.2-15.8 ms | 1.97-2.0/frame | One active main leg plus one renderer-ready wait |
| Queue poll `sub_8219F990`, thread 15 | 14.4-17.1 ms | 14.4-17.1 ms | 0 ms | 1.0/call | The one-vblank intervention acted as designed |

The experiment reported approximately 90 queue-scope calls and 58-63 forced
releases per 30 frames. Forced releases normally occurred with a queue distance
of two, proving that the hook was executing. One `VdSwap` still occurred per
complete frame. The existing pacer remained bypassed because guest production
never reached 50 FPS.

## Event map

The two material infinite waits form a producer/consumer handoff:

| Guest caller | Thread | Average wall time | Vblank | Role |
|---|---:|---:|---:|---|
| `0x82160E4C` | 6 | 15.2-15.7 ms | about 1.0 | Main thread waits for renderer-ready event |
| `0x82161160` | 15 | 16.6-18.6 ms | about 1.0 | Render thread waits for work event |

Other measured waits in these two paths were normally microseconds. Background
threads that wait a complete frame are not the frame-production bottleneck.

## Rejected interventions

### Treating the queue poll as a two-vblank cap

Rejected. Returning from `sub_8219F990` after one real vblank reduced that
scope from the phase-1 31-33 ms observation to about 15-17 ms, but complete
frame cadence did not change. The remaining interval is real work and the
opposite side of the handoff.

### Zero-timeout renderer-ready wait

A temporary diagnostic made only the main-thread wait at guest caller
`0x82160E4C` return immediately in the menu. That wait fell to approximately
0.001 ms, but FPS did not change: the latency moved to `0x8215AF20`.

Static analysis identifies `sub_8215AEE8`/`sub_8215AF68` as recursive acquire
and release operations for guest graphics-device ownership. The object contains
an event at `+0`, owner thread at `+4`, recursion depth at `+8`, and device at
`+16`. Bypassing this ownership lock would violate renderer invariants and did
not improve cadence. The temporary bypass was removed.

## Correct next direction

1. Profile the active portions of main thread 6 and render thread 15 separately,
   beginning below `sub_82160E28`, `sub_8219F990`, and the graphics-device
   ownership region.
2. Determine whether translation overhead, command production, D3D12 submission,
   or a guest lock handoff consumes each approximately 16 ms active leg.
3. Optimize the active work or establish a correct parallel ownership handoff;
   do not bypass event or device-ownership synchronization.
4. Continue tracing the writer of simulation fields `+64/+68` in the object
   referenced by `0x833A30CC` before enabling a future 60 FPS path in gameplay.
5. Re-run the menu animation-speed test only after production reaches at least
   50 FPS and the existing pacer engages. That test was not run in phase 2
   because the intervention never produced 60 FPS.

Local logs remain untracked evidence and must not be distributed.

## Validation

- Independent Release configure, codegen, game/runtime build, and replay-tool
  build passed in `native/narutobb/out/build/verification-fps-phase2`.
- `narutobb.exe` SHA-256:
  `B056BEBE1BF6BDEC085CB85E32337708ABF63BC3E7BCBA1171656B2158BCBB18`.
- The normal launcher configuration survived the automated 30-second boot
  without a fatal error; the experiment cvar was not enabled.
- All 12 traces in the approved GPU baseline reproduced with zero mean-RGB and
  non-black-pixel delta.
- A thirteenth local trace has no public baseline. It was not counted as a pass
  and was not modified or added to the baseline.
- The 13-patch ReXGlue series applied cleanly to the pinned upstream base and
  produced tree `129ced1beda6743fcbb88986bae4c635abc99c93`.
