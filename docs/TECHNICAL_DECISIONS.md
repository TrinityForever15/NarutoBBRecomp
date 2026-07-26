# Technical decision record

Last updated: 2026-07-25.

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
`5a0710954c5f400b58bbba27c8448a255cdd91e4`.

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

**Historical qualification:** D022-D024 supersede this section's assumption
that the central fixed-step substitution is required in menus and the open
world. Those contexts are variable timestep and were already correctly timed;
only the vblank multiplier changes their production cadence. The evidence and
rejected synchronization bypasses below remain valid.

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

Log `_095` and player observation confirmed 60.00 FPS with normal UI speed.
Later speed telemetry showed the menu clock was in variable mode, so the
central-step substitution was inert there. The user-facing experiment still
keeps one target for vblank, pacing, and fixed contexts so those controls
cannot disagree when a battle begins.

## D021 — Reject broad half-rate battle-task hooks

**Superseded diagnosis:** D022 later proved that the apparent jutsu-specific
problem was a fixed-step battle receiving burst frames. The rejection of broad
task skips remains active; the former visual-timeline resume direction does
not.

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
Do not resume from a jutsu-specific animation writer without new evidence.
Validate the real-time step limit and cut-in duration as described by D022-D024.

## D022 — Battle uses a fixed timestep, so its speed is the achieved frame rate

Speed telemetry in the central clock updater settled what static analysis and
Tracy sampling could not. The updater writes the raw delta at `+68` and the
scaled delta at `+64` every call, so simulated seconds divided by real seconds
is a direct speed factor. Log `_105` measured it per context:

| Context | `+72` fixed mode | FPS | Step | Speed |
|---|---|---:|---|---:|
| Menu and open world | 0 | 30 and 60 | 1/30 and 1/60 | 1.000 |
| Battle, experiment active | 1 | 60 | 1/60 | 1.01 |
| Battle, experiment suspended | 1 | 60 | 1/30 | 1.99 |

Menus and the open world use a variable timestep. They measure elapsed time
themselves, report exactly 1.000 at any frame rate, and never needed the step
substitution; the vblank multiplier alone was doing the work there.

Battle uses a fixed timestep, where speed is exactly achieved frame rate
multiplied by the step. The port does not vblank-limit that context, so it
runs near 60 FPS against a 1/30 step. **The default build was therefore running
battle at roughly twice speed, and the 60 FPS experiment had been masking that
by writing 1/60.** The accelerated battle recorded in `_097` and in FPS-16 was
misattributed to the experiment; it is the baseline behaviour.

This also explains the surviving symptom. The guest loop is not bound to the
presentation pacer and produces occasional 2-4 ms frames, each advancing the
world a full step. A cheap two-dimensional overlay such as the battle status
portrait renders fast enough to trigger those bursts, while animation-heavy and
effect-heavy jutsu stay near the pacer and look correct. The portrait cut-in
did not need a jutsu-specific timeline writer, which D021 had assumed.

## D023 — Correct a fixed-step context by real elapsed time, not by a target step

The correction grants each frame no more world time than real time has
delivered. Steady frames still receive the exact nominal step, so determinism
is preserved where the rate is stable; only frames the guest produces faster
than the cadence are shortened to the time they actually took.

Capping the step at the nominal value is wrong and was rejected after testing.
It corrects frames that are faster than the target but puts every frame slower
than the target into slow motion, which is what entering the open world at a
120 FPS target looked like before the ceiling was removed. Measured over
samples between 30 and 110 FPS, mean speed was 0.864 with a 0.282 minimum
before the fix and 1.000 with a 0.650 minimum after it.

The only remaining ceiling is a hitch guard, taken from the title's own maximum
delta at `+80` because that is the largest frame time the updater already
considers safe in its variable branch.

The step, the presentation pacer target and the guest vblank multiplier are all
derived from one target rate so they cannot disagree. Disagreement between them
was the original defect: a pacer aiming at 61.5 Hz against a 1/60 step produced
a permanent 2.5 percent overspeed in every battle.

## D024 — A correct speed factor does not approve a frame rate

`raw_speed` at 1.000 proves the clock is right. It says nothing about logic
counted in frames, which fighting games commonly use for combo windows,
invulnerability and input buffering. The 120 FPS target reached a 119.37 FPS
median at 1.000 speed and is still marked experimental for that reason. Approve
a target against combat behaviour and cut-in duration, never against how smooth
it looks or what the speed factor reports.

## D025 — Measure audio per stream before changing it

Audio scenarios stayed pending for a long time because every change was judged
by listening. The balanced downmix consumed a full cycle on that basis before
being rejected.

Per-stream telemetry is now the entry point. `XmaContext::Telemetry` accumulates
decoded frames, buffer swaps, loop rewinds, decode errors split by cause, stall
durations and guest writes to the read offset. Decoded audio seconds per real
second is the primary metric, the direct analogue of the simulation speed factor
that settled the frame rate work.

All counters are incremented under locks the audio paths already hold, and all
formatting happens on the decoder or audio worker thread. Nothing is added to
the real-time SDL callback, because periodic I/O there was already identified as
a dropout source and removed once (D016).

## D026 — Cutscene audio is an input-buffer deadlock, not an output problem

The output path is excluded by measurement. Inside a failing cutscene the queue
stays effectively full, no underrun occurs, and no waveform discontinuity is
measured at any buffer seam, while decode errors rise from 6.6% to 25.3% of
attempts. Varying the output queue depth eight-fold moved stalls and timeouts by
1%. **A different audio backend, XAudio2 or otherwise, would not address this
defect**, and that avenue is closed unless new evidence appears.

Every decode error is one case: a frame split across two input buffers whose
continuation the title has not supplied. The title keeps exactly one input
buffer valid at a time in 97% of samples, so holding the consumed buffer while
waiting deadlocks against its refill. The title responds to the resulting
`error_status = 4` by rewinding, which is the repeated fragment that is heard;
every observed guest write to the read offset was a rewind.

The frame rate mode is not a factor: 87.1 stalls per second with it suspended
against 88.0 with it active.

## D027 — A split frame must be retained across a buffer swap

Treating the starved case as a bounded wait is correct and retained behind
`naruto_xma_stall_on_missing_input`. It removes every decode error and raises
decoded frames 23% inside the scene. The wait must be bounded: an unbounded
version left seven streams silent, one for 21 seconds, because a stream ending
on a split frame never receives another buffer.

Releasing the consumed buffer without retaining the partial frame is rejected.
It does break the deadlock, taking stalls and timeouts to zero, but contexts
with no valid input rose from 0.3% to 8% of samples, streams died, and the
cutscene soft-locked at its end. Discarding the leading part of a split frame
desynchronizes the title's accounting of submitted against consumed data.

Retention of the partial frame across the swap is therefore a requirement of
any fix here, not a later optimization. Both cvars stay disabled by default
until that work exists and is validated.
