# FPS phase 1 — identifying the 30 FPS mechanism

Date: 2026-07-23.

## Conclusion

The roughly 33.3 ms frame is neither continuous useful work nor a guest thread
sleeping in kernel waits. The cap is active render-queue synchronization: the
frame thread remains runnable and polls until queue state advances. Each stable
frame contains one `VdSwap` and an average of two guest vblank pulses.

This is a synchronization cap, not evidence of insufficient hardware
performance. Near-100% CPU occupancy is the busy-wait itself.

No byte of `default.xex` was changed.

## Instrumentation

When the trace is enabled, the runtime records:

- wall time and thread CPU time per frame;
- object/multiple/signal-and-wait/delay durations and counts;
- guest vblank pulses and `VdSwap` calls;
- exact PCs for generated `mftb`/`mftbu` reads;
- scopes for `sub_821C1468` and `sub_8219F990`.

Game hooks use `sub_821B1DD0` as the frame boundary and report fields written by
`sub_821C0620` at `+21576`, `+21616`, and `+21620` in renderer state.

Primary markers:

```text
NARUTO_FRAME_TIMING
NARUTO_LOOP_PROBE
NARUTO_GPU_WAIT_PROBE
NARUTO_DELTA_WRITER
NARUTO_WAIT_TOP
NARUTO_TIME_SOURCE
NARUTO_TIMING_CONSUMER
```

## Manual three-context capture

Local source: `narutobb_063.log`. The player moved through menu, open world, and
pause in that order. Stable windows away from transitions produced:

| Context | Frames | Mean wall | Mean CPU | Blocking waits | Written delta | Queue wait | Vblank/frame | `VdSwap`/frame |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Menu | 568 | 33.994 ms | 33.231 ms | 0.046 ms | 34.001 ms | 33.077 ms | 2.000 | 1.000 |
| Open world | 1,193 | 33.405 ms | 33.214 ms | 0.054 ms | 33.413 ms | 30.952 ms | 2.000 | 1.000 |
| Pause | 488 | 33.306 ms | 33.203 ms | 0.028 ms | 33.312 ms | 31.900 ms | 1.998 | 1.000 |

Explicit kernel waits are roughly 0.1% of the frame. Individual vblank counts
vary from one to three because the threads are asynchronous, but all three
contexts converge on a mean of two.

## Readers of `0x820E8B58`

| Context | Function | Calls/frame |
|---|---|---:|
| Menu | none of the 16 candidates | 0 |
| Open world | `sub_82199B00` | 1.000 |
| Open world | `sub_8276E338` | 4.320 |
| Open world | `sub_8281CEF0` | 0.661 |
| Pause | `sub_82199B00` | 1.000 |

The value is context-dependent and is not the global frame cap. The old write
experiment is closed unless new evidence appears.

## Mechanism location

`sub_8219F990` is called about three times per frame. Two calls are nearly
instantaneous; one long call occupies most of the frame. It polls through
`sub_821A1858` until render-queue/ring state advances and reads the timebase at
guest PCs `0x8219FA24` and `0x821A17A8`.

The observed sequence is:

1. submit one `VdSwap`;
2. poll render-queue progress in guest code;
3. approximately two 60 Hz guest vblanks pass;
4. begin the next frame near 33.3 ms.

No kernel call simply waits for two vblanks. The equivalent cap is implemented
by guest polling of runtime-visible queue progress.

## Dynamic delta hypothesis

Five timebase reads occur per frame:

- `0x8219FA24`: once;
- `0x821A17A8`: once;
- `0x821C068C`: once;
- `0x821F62B0`: twice.

`sub_821C0620` is the renderer-delta writer, but that does not prove the
simulation delta. `sub_8276E338`, active only in the open world, reads `+64/+68`
from an object reached through global `0x833A30CC`. Instrumenting the writer of
those fields is the next priority.

`sub_821C1468` appeared only 16 times during initialization, for microseconds,
on another thread. It is not the frame loop.

## F10 correction

Win32 delivers F10 as `WM_SYSKEYDOWN`. The runtime previously forwarded only
`WM_KEYDOWN`. It now forwards `WM_SYSKEYDOWN/UP` only for F10 without Alt, so
other system keys keep normal Windows behavior.

`narutobb_067.log` confirmed trace enable at frame 191, disable at frame 236,
and normal Alt+F4 shutdown.

## Next experiment

Implement a reversible runtime cvar that changes the queue progress observed by
`sub_8219F990` so one guest frame may advance per vblank. Retain delta telemetry
and independently validate simulation, animation, physics, menu, pause, battle,
and cutscene timing. No 60 FPS claim is valid before those checks.

## Integration evidence

- SDK local head: `c91f2b53a1018b779ed3b5d9d201412719cb73ca`;
- expected patched tree: `62e97f17f8e6cfb4d73905c5158aef1d8d292151`;
- all twelve patches reproduced the expected tree from the pinned upstream base;
- project invariants and a 30-second controlled boot passed;
- all twelve numeric GPU baselines had zero delta;
- one additional local capture lacked a baseline and was treated as
  non-comparable, not as a visual regression.
