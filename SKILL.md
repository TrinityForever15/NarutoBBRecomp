---
name: xbox360-recomp-naruto
description: Use for work on statically recompiling Naruto: Rise of a Ninja or The Broken Bond from Xbox 360 to native PC with ReXGlue, XenonRecomp, or XenosRecomp. Also covers the project's guest CPU, Xenos GPU, XMA audio, timing, kernel, build, and regression techniques.
---

# Naruto Xbox 360 native recompilation knowledge base

New sessions must begin with the reading order in `AGENTS.md`. This file is the
detailed technical memory; `docs/STATUS.md` is the short operational snapshot.

## Current target and invariants

Target: *Naruto: The Broken Bond* USA/Europe multi-language release, Title ID
`55530825`.

- Active project: `native/narutobb`.
- Active local runtime: `tooling/rexglue-sdk`.
- Portable runtime source: fourteen patches under `patches/rexglue-sdk`.
- Local user-supplied data: `recomp/fase4/assets`.
- Active build: `native/narutobb/out/build/win-amd64-source`.
- Active XEX SHA-256:
  `8F70E79443E36B38E44B6A105DAD51FB8909FB615D982753CABAAC9B15F0D576`.
- Never patch that XEX in place.
- Never edit or publish generated guest C++.
- Never publish game data, keys, extracted shaders/media, screenshots, GPU
  traces, or proprietary-derived research artifacts.

## Latest result: menu 30 FPS is a serialized main/render pipeline

Phase 1 instrumentation was implemented in the runtime without modifying the
XEX. `sub_821B1DD0` is the measured frame boundary. The runtime records wall and
thread CPU time, guest waits, vblank, `VdSwap`, and exact `mftb`/`mftbu` guest
PCs. Additional probes cover `sub_8219F990`, `sub_821C1468`, and renderer-delta
writer `sub_821C0620`.

Phase-1 manual trace `narutobb_063.log` covered menu, open world, and pause:

| Context | Wall | CPU | Blocking waits | Queue polling | Vblank/frame | Swap/frame |
|---|---:|---:|---:|---:|---:|---:|
| Menu | 33.994 ms | 33.231 ms | 0.046 ms | 33.077 ms | 2.000 | 1.000 |
| Open world | 33.405 ms | 33.214 ms | 0.054 ms | 30.952 ms | 2.000 | 1.000 |
| Pause | 33.306 ms | 33.203 ms | 0.028 ms | 31.900 ms | 1.998 | 1.000 |

Phase 2 disproved the initial interpretation that this was a removable
two-vblank queue cap. A reversible menu-only runtime hook made
`sub_8219F990` return after one real guest vblank. The hook executed and reduced
that scope to roughly 14.4-17.1 ms, but complete frames stayed near 33 ms and
the player still observed 30 FPS.

The final menu run showed two serialized legs:

| Scope/thread | Wall | Active | Blocked | Material wait |
|---|---:|---:|---:|---|
| Complete render frame / 15 | 33.5-34.5 ms | 16.6-17.2 ms | 16.6-17.9 ms | caller `0x82161160` |
| Main handoff `sub_82160E28` / 6 | 32.2-33.1 ms | 16.6-17.9 ms | 15.2-15.8 ms | caller `0x82160E4C` |

The render wait is a work event and the main wait is a renderer-ready event.
A temporary zero-timeout diagnostic for `0x82160E4C` did not raise FPS; the
same interval moved to `0x8215AF20`, the recursive guest graphics-device
ownership acquire. The bypass was removed. Do not bypass these waits or the
device-ownership lock.

Active readers of former candidate `0x820E8B58`:

- menu: none;
- open world: `sub_82199B00` once/frame, `sub_8276E338` about 4.32/frame,
  `sub_8281CEF0` about 0.661/frame;
- pause: only `sub_82199B00` once/frame.

The old `1/30 -> 1/60` write made no perceptible difference and is removed. The
constant has 17 direct reads across 16 functions and is not a global fixed step.

Five timebase reads occur per frame: `0x8219FA24`, `0x821A17A8`, and
`0x821C068C` once each, plus `0x821F62B0` twice. `sub_821C0620` writes the
renderer delta at `+21576` and queue-wait accumulators at `+21616/+21620`.
The simulation delta writer is still unknown. The next candidate is the writer
of `+64/+68` in the object referenced by `0x833A30CC`, read by
`sub_8276E338` in the open world.

`sub_821C1468` is not the main game loop: it appeared only 16 times during
initialization, for microseconds, on another thread.

The next FPS work must profile and reduce the active approximately 16 ms on
both the main and render legs, or prove a correct parallel ownership handoff.
It must also identify the simulation delta writer. Validate simulation speed,
animation, physics, menu, pause, battle, and cutscenes before claiming success.
There is no approved 60 FPS mode.

### F10 Win32 fix

F10 arrives as `WM_SYSKEYDOWN`, not `WM_KEYDOWN`. `window_win.cpp` now forwards
only F10 without Alt from `WM_SYSKEYDOWN/UP`; other system-key behavior remains
native. `narutobb_067.log` recorded trace enable at frame 191, disable at 236,
and normal Alt+F4 shutdown.

Full reports: `recomp/fase4/FPS_PHASE1_REPORT.md` and
`recomp/fase4/FPS_PHASE2_REPORT.md`.

## Current audio architecture and evidence

Active runtime behavior:

- maximum SDL/APU queue: 64 frames;
- resume after underrun only after a 16-frame refill;
- short fade-out/fade-in concealment around gaps;
- pending semaphore credits are retried instead of silently lost;
- sample validation and limiter remain active;
- XMA/FFmpeg state is flushed on valid clear/restart/stream transitions;
- dedicated XMA scheduling is default;
- synchronous XMA is an isolated test via
  `--use_dedicated_xma_thread=false`.

### Native crash fixed

`narutobb_043.log` booted normally, then Windows reported access violation
`rexruntime.dll+0x566F3F` around 20.6 seconds. Diagnostic symbols and disassembly
mapped it to `avcodec_flush_buffers` called from `XmaContext::ClearLocked`.

`avcodec_alloc_context3(codec)` sets `AVCodecContext::codec` before
`avcodec_open2` creates its internal state. Testing `codec` was insufficient.
The correct guard is:

```cpp
av_context_ && avcodec_is_open(av_context_)
```

Post-fix active and clean builds passed beyond the original failure point and
the 30-second automated boot.

### Scheduling comparison

Dedicated and synchronous XMA builds both passed automated smoke tests. Manual
logs `_050` and `_051` did not distinguish an audible benefit. Synchronous mode
remains diagnostic, not a preferred solution.

### Balanced downmix rejected

The optional 5.1-to-stereo matrix and per-second channel logger were tested in
`_054`. Light crackle remained, bass did not improve, the missing sound did not
return, and new dropouts made playback worse. LFE was nearly zero, center was
active with peak 0.524, and no output clipping marker appeared.

The matrix was reverted. Periodic formatting/I/O in the real-time SDL callback
was also removed because it was the most plausible source of the new dropouts.
Do not repeat this experiment without new evidence.

### Passive source-flow diagnostics

`audio_silence_diagnostics=true` changes neither XMA, volume, nor downmix. In
the callback it only accumulates total frames, all-silent frames, longest silent
run, active frames per guest channel, and channel peaks. After the SDL stream is
destroyed, normal shutdown emits:

```text
NARUTO_AUDIO_SILENCE_SUMMARY frames=... silent_frames=... silent_percent=...
max_silent_frames=... max_silent_ms=... active_frames=[...] peak=[...]
order=FL,FR,FC,LFE,BL,BR
```

The active and clean smoke tests emitted the summary without fatal errors. The
exact missing-sound scene remains pending.

## Graphics: cutscene black screens resolved

Story cutscenes previously alternated between correct frames, full black,
partial black, silhouettes, and purple composition. The failure reproduced
offline with 12 local GPU traces.

Draw bisection found a late full-screen depth-only mask that caused EDRAM color
ownership corruption. The deeper cause was the SDK changing Xenia's default
`execute_unclipped_draw_vs_on_cpu` from true to false. With CPU extent estimation
disabled, a clip-disabled 8192 scissor, pitch 640, 4x-MSAA depth draw at base
`0x2D0` claimed the entire 0x800-tile EDRAM range with wrap; ownership transfer
destroyed color RT0.

Restore and keep:

```text
execute_unclipped_draw_vs_on_cpu=true
```

All 12 replays were checked numerically, normal frames did not regress, and the
player confirmed the Story Mode black screens, silhouettes, and purple
composition were fully resolved. D3D12 RTV remains the default. ROV, alternate
stencil, and `ALWAYS` depth transfer were rejected.

Detailed history:
`recomp/fase4/HANDOFF_CUTSCENES_GPU_2026-07-19.md`.

## Guest functions and boot hooks

ReXGlue's scanner can miss indirect targets. Add only exact, narrow ranges to
both manifests, regenerate, confirm `SetFunction(address)` in
`narutobb_register.cpp`, and replay the triggering scene.

Important current range:

```toml
0x8215D000 = { end = 0x8215D03C }
```

It addresses the historical fatal call after the Orochimaru versus Fourth
Hokage sequence. Generation/registration is proven; the exact post-fix scene is
not yet replayed.

The `sub_8217AB20` hook must remain. It bypasses the Fox/Jade
`CompareBackEnds` diagnostic HLSL compiler, which returns `E_FAIL` and leaves a
null object in the recompiled environment. The production renderer does not
depend on this debug comparison.

## Reproducible ReXGlue integration

Pinned upstream base:
`2bdb97f95f154f32d281aaa08446ae007b8ca117`.

Expected final tree after fourteen patches:
`5144c7af01ce1483a5c59cbde7e419517f5a062e`.

Validated local head after the phase-3 vblank experiment:
`6fda9628c05eafb0b6628aae02bfdc7a264f51f2`.

The local commit hash may change when patches are reapplied because committer
metadata changes; the tree hash is the portable integrity check.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/bootstrap_rexglue_sdk.ps1
powershell -ExecutionPolicy Bypass -File scripts/build_clean_windows.ps1
powershell -ExecutionPolicy Bypass -File scripts/run_regression.ps1 `
  -BuildDir native/narutobb/out/build/verification-clean
```

The SDK checkout is a separate Git repository and must never be added to the
main repository.

## GPU replay workflow

F9 requests one local `.xtr` capture. `narutobb_trace_dump` replays a trace
headlessly and writes a framebuffer plus `TRACE_DUMP_SUMMARY`. Replay must use
synchronous shader compilation or it can reach swap before pipelines finish and
produce artificially black output.

Draw diagnostics include deterministic draw indices, range/index skipping,
state dumps, shader hashes, and numerical mean/non-black pixel summaries.
Capture files and rendered images are proprietary-derived local evidence and
must never be committed.

The public baseline contains only numeric results for the approved 12 captures.
Do not update it automatically to make a failure pass.

## Build and runtime milestones

### Phase 1 — CPU translation

The original XenonRecomp route identified and implemented 35 missing VMX/scalar
instructions. The resulting translation reported zero unknown instructions.
This path is historical because ReXGlue codegen now covers the required
instructions directly.

### Phase 2 — shader translation

The original study translated 8,156 Xenos shaders after four XenosRecomp fixes:
Texture1D support, vertex fetch-slot normalization, sampler fallbacks, and
generic sequential Vulkan locations plus shared-constant layout correction.
Extracted/translated shader bodies are proprietary and are not part of the
public repository.

### Phase 3 — first native boot

ReXGlue v0.8.0 generated 97 translation units and produced the first Linux
x86-64 native boot. Required runtime work included enabling existing
`xboxkrnl_usbcam.cpp`. Report: `recomp/fase3/PHASE3_REPORT.md`.

### Phase 4 — boot to playable Windows

A POSIX lost-wakeup in suspended thread startup was fixed by publishing
`suspend_count_ = 1` atomically with the suspended state. The CompareBackEnds
hook then moved the guest beyond boot. Windows x64 D3D12 rendering, input,
cutscene correctness, replay tooling, and automated regression followed.

Chronology: `recomp/fase4/PHASE4_WINDOWS_REPORT.md`.

## Confirmed title facts

| Item | Value |
|---|---|
| Title ID | `55530825` |
| Entry point | `0x821EF970` |
| Image base | `0x82000000` |
| XEX | retail XEX2, basic compression |
| Generated ReXGlue units | 97 |
| Engine | Ubisoft Fox/Jade lineage, unrelated to Kojima Productions' Fox Engine |
| Audio | Xbox 360 XMA decoded by the ReXGlue/FFmpeg path |

Do not publish decryption keys or title data extracted from the game. ReXGlue
must operate on files supplied locally by the user.

## Diagnostic routing

### FPS phase 3: vblank quantization and central simulation clock

The reversible guest-vblank multiplier proved that the default 30 FPS cadence
is quantized by guest vblank. With the guest clock at 120-123 pulses/s, menu and
pause produced 60-61 complete guest frames/s; open world produced approximately
57-60 FPS when scene cost allowed. The runtime worker needs the Windows
high-resolution wait path at intervals below the normal 60 Hz period. F8 and
context guards must restore multiplier one.

The renderer delta written by `sub_821C0620` follows real production cadence,
but simulation does not. A temporary codegen diagnostic watched exact dynamic
addresses `simulation_clock+64/+68` from the object at `0x833A30CC`. Log `_087`
identified the authoritative once-per-frame stores:

```text
0x82BC9000 -> simulation_clock+68 = 0x3D088889 (1/30)
0x82BC9008 -> simulation_clock+64 = 0x3D088889 (1/30)
```

Both belong to `sub_82BC8FA8`. In its fixed mode, field `+76` is the configured
step copied to `+68`; `+64` and internal tick counters are derived from it. The
world experiment therefore captures the original `+76`, supplies
`0x3C888889` (1/60) before calling the original function, and restores the
captured value when the experiment is suspended or guarded. Do not merely
overwrite `+64/+68` after the function because that would leave internal ticks
at 30 Hz. The broad generated store instrumentation was removed after the
writer was proven so performance measurements are not contaminated.

Manual log `_089` confirmed that this combination reaches 60 FPS without the
previous 2x animation speed. Its low interval was real render-side budget
pressure rather than vblank-worker failure: complete render frames rose from
approximately 16.6 ms to 19-23.4 ms while the worker maintained 120-123 Hz with
zero coalesced intervals. The diagnostic launcher had auto-enabled the costly
per-scope F10 trace during that run; repeat with tracing disabled before using
the low interval as a clean performance baseline.

In clean manual log `_090`, the player observed a minimum near 56 FPS. The log
also showed repeated `active -> guarded -> active` transitions because the two
world-consumer bits can disappear for longer than 30 frames during ordinary
context changes. They are suitable for initial validation, not continuous
ownership. After 30 stable world frames, latch the experiment until F8; retain
the disarmed boot and restore captured clock state on explicit suspension.
Log `_092` validated that policy: the experiment activated once, kept 120 Hz
and the 1/60 clock without a guarded transition, and the pacer remained enabled
after entering near 58.8 FPS for the rest of the recorded run. The player saw a
minimum around 55-56 FPS and described the remaining instability as barely
perceptible. The menu 120 Hz mode must use the same central 1/60 substitution;
vblank-only menu operation is the already-rejected 2x-speed configuration.
Player-confirmed log `_095` reached exactly 60.00 FPS with normal menu speed.
The unified `naruto_60fps_experiment` starts disarmed and makes F8 change both
clocks atomically across menus, pause, transitions, and gameplay; never expose
either half as the user-facing 60 FPS mode.

Unified manual log `_097` invalidated the assumption that the central clock is
sufficient for every context. Menus and open world stayed correctly timed, but
battle animations accelerated. No `clock-restored` or new `clock=...` marker
appeared, so battle retained the corrected central clock object and must use an
additional fixed step or frame-count path. Do not weaken the proven world/menu
correction; capture battle-exclusive timing consumers first.

### Battle timing follow-up and pause point (2026-07-24)

Two local Tracy captures provide the current resume point. The battle sample
contains 530 profiled frames and 3,879 guest functions; the open-world sample
contains 147 profiled frames and 3,398 guest functions. Profiling overhead made
the world sample much slower, so compare call counts per frame and active versus
inactive functions rather than raw totals. The `.tracy` files are local,
proprietary-derived evidence and must never enter Git.

The battle capture and static inspection established the following negative
evidence:

- `sub_8276E338` shifted between scheduler call sites in battle, but its total
  cadence did not duplicate; it is not the extra animation step.
- The combat `sub_827E4F58` branch and following `sub_827E4EC0` virtual stage
  did not expose an eligible +16 target in the sampled fight.
- `sub_82765D48`, `sub_8285B1F8`, `sub_8285B9A8`, and `sub_8285BF98` are
  collision/spatial or relative-position work, not animation clocks.
- The central `sub_82BC8FA8` object and 1/60 substitution remained unchanged;
  known `+64/+68` consumers therefore cannot explain the remaining 2x visual.

A reversible diagnostic skipped selected once-per-frame tasks on alternating
60 Hz frames. Log `_102` records the tested sequence. Half-rating
`sub_829C17E0` (a thin wrapper over large state/command task `sub_829C11A8`)
made the battle itself appear correctly paced, but the on-screen jutsu remained
accelerated. This is useful separation evidence, not an acceptable fix.
Half-rating battle state-7 routine `sub_82AAF0B8` slowed the fight and did not
correct the jutsu. Half-rating battle-only task `sub_82AB8338` produced no
visible change. The refined probe build also showed new environment artifacts.

All battle half-rate hooks, the F11 binding, its cvar, and its launcher were
removed. When work resumes, instrument the jutsu-specific visual animation
timeline or its writer. Do not repeat broad task skipping, do not alter the
validated menu/open-world central clock, and do not claim global 60 FPS until a
full battle keeps both gameplay cadence and every visible jutsu at normal speed.

| Symptom | First evidence and code |
|---|---|
| Missing guest address | fatal log, both manifests, generated registration |
| Boot loop | CompareBackEnds hook and runtime thread state |
| Crackle/dropout | `NARUTO_AUDIO_UNDERRUN`, XMA mode, passive summary |
| Wrong/stale music | XMA clear and scene transition |
| Cutscene corruption | local replay, draw extent, EDRAM ownership |
| 30 FPS open world | `sub_8219F990`, guest timing, FPS phase-1 report |
| F10 failure | Win32 `WM_SYSKEY*` forwarding |
| Save/load | exact profile path and full restart scenario |

Keep audio, graphics, guest-function, and FPS experiments isolated so a result
cannot be attributed to the wrong subsystem.
