# Current project status

Last updated: 2026-07-25.

This is the short operational snapshot. Detailed history lives in `SKILL.md`
and the reports under `recomp/fase4`.

## Objective

Run *Naruto: The Broken Bond* natively on Windows with enough stability to
complete Story Mode, correct audio, and a 60 FPS open world without breaking
menus, battles, physics, animation, or cutscenes.

## Maturity classification

**Research preview.** The native build reaches portions of gameplay, but no
complete campaign or other extensive continuous sequence has been validated.
The project must not be presented as a usable PC port until that threshold is
met and recorded in the test matrix.

No unaffiliated third party has independently verified the runtime claims. The
GitHub-hosted public-content workflow is independently reproducible, while the
current build, gameplay, graphics, audio, and timing results are maintainer-run
local observations. The project has also used AI coding agents heavily for
documentation, hypotheses, implementation, review, and automation; AI output is
not accepted as verification evidence.

## Active configuration

| Item | Source of truth |
|---|---|
| Game project | `native/narutobb` |
| Runtime source | local `tooling/rexglue-sdk` checkout |
| Portable runtime changes | `patches/rexglue-sdk` |
| Local game data | `recomp/fase4/assets` |
| Launcher build | `native/narutobb/out/build/win-amd64-source` |
| Original XEX SHA-256 | `8F70E79443E36B38E44B6A105DAD51FB8909FB615D982753CABAAC9B15F0D576` |

No build or game data is distributed through the public repository.

## Public repository

The sanitized public tree is available at
<https://github.com/TrinityForever15/NarutoBBRecomp>. The `main` branch was
published on 2026-07-23 from a new root commit so proprietary artifacts from
the former local history are not reachable from the public repository. The
public-content workflow passed for commit
`765fa82562a5c1f6085ef0f2b22bccad333c642e`.

## Maintainer-verified capabilities

- Native Windows x64 build with Clang, CMake, and Ninja.
- ReXGlue bootstrap from pinned upstream commit plus sixteen ordered patches.
- D3D12 initialization and correct RTV rendering on an RX 6650 XT.
- Runtime mounting of a complete, user-supplied game-data directory.
- Guest threads and normal game flow after boot.
- Keyboard/Start and XInput controller support.
- CompareBackEnds debug-compiler hook prevents the historical boot loop.
- Cutscene black screens fixed by enabling CPU evaluation of the unclipped draw
  extent; manual in-game confirmation is complete.
- GPU frame capture, deterministic replay, draw bisection, and a 12-trace
  numerical regression baseline.
- Guest function `0x8215D000-0x8215D03C` generated and registered for the
  historical end-of-Orochimaru crash.
- Safe XMA clear: `avcodec_flush_buffers` is called only for an opened codec
  context, fixing the native crash observed around 20.6 seconds after boot.
- Passive audio-flow diagnostics collect counters without logging or allocating
  in the real-time callback.
- Simulation speed is measured directly as simulated seconds per real second,
  reported per context as `NARUTO_SIM_CADENCE`.
- Battle was found to be a fixed-timestep context that the port does not
  vblank-limit, so the default build had been running it near twice speed. The
  real-time step limit corrects that and is active without any key press.
- Frame rate mode arms itself at boot, and the simulation step, presentation
  pacer target and guest vblank multiplier all derive from one target rate.
  Sampled battle measured a 1.000 speed median at 60, and 120 reached a
  119.37 FPS median at 1.000 speed.

## Remaining work on timing

- No full battle has been validated against frame-counted combat timing. Combo
  windows, invulnerability and input buffering are unmeasured, and a correct
  speed factor does not cover them. The 120 target stays experimental for this
  reason.
- The status-jutsu portrait cut-in was explained by burst frames rather than a
  dedicated timeline writer, and the burst source is corrected, but its duration
  was never compared at 30 against 60. Capture launchers and
  `recomp/fase4/analyze_cutin.py` are staged for that measurement.
- Frame rate dips below the target remain unprofiled. They no longer affect
  simulation speed, so they are a smoothness issue rather than a correctness
  one.
- The current timing/audio integration passed a clean Release build, controlled
  30-second boot and all 12 approved GPU baselines. Repeat them after any code
  change and before release.

## Reproducibility evidence

The clean Release build in
`native/narutobb/out/build/verification-audio-silence-summary` completed without
reusing the active cache. Its automated run verified project invariants, a
30-second boot without fatal errors, and zero numerical delta across the 12 GPU
replays in the baseline. A separate normal shutdown emitted
`NARUTO_AUDIO_SILENCE_SUMMARY` only after the SDL stream was destroyed.

The local SDK branch `narutobb-integration` contains sixteen subject-separated
commits. Pinned upstream base:
`2bdb97f95f154f32d281aaa08446ae007b8ca117`; expected final tree:
`5a0710954c5f400b58bbba27c8448a255cdd91e4`.

These automated results do not approve perceived audio quality, Story Mode
progression, save/load, or correct 60 FPS simulation.

The independent phase-2 FPS Release build at
`native/narutobb/out/build/verification-fps-phase2` also completed codegen,
game/runtime, and replay-tool compilation. Its normal 30-second boot passed,
and the exact 12-trace GPU baseline reproduced with zero numerical delta. One
additional local trace has no public baseline and is intentionally excluded
from that approved regression set rather than treated as a visual pass.

The current phase-4 audit build at
`native/narutobb/out/build/verification-phase4-audit` completed independent
configure, codegen, game/runtime compilation and replay-tool compilation. Its
30-second controlled boot passed, and all 12 approved GPU traces reproduced
with zero numerical delta. Two additional local captures have no approved
public baseline and were reported as non-comparable rather than promoted or
counted as failures.

## Priority work

### P1 — intermittent or missing audio

The current runtime includes a maximum queue of 64 frames, a 16-frame refill
after underrun, short concealment fades, safe semaphore-credit recovery, sample
validation/limiting, and XMA/FFmpeg state cleanup. Dedicated XMA scheduling is
the default; synchronous XMA is available only as a controlled comparison.

The optional balanced downmix experiment was rejected. In manual log `_054`,
it did not restore the missing sound or improve bass, the light crackle remained,
and new dropouts made the result worse. LFE was nearly silent, the center channel
was active with a measured peak of 0.524, and no digital output clipping was
reported. The experiment and its real-time periodic logger were removed.

The active `audio_silence_diagnostics` mode only accumulates six-channel activity,
silence, and peak counters in memory. It writes one
`NARUTO_AUDIO_SILENCE_SUMMARY` during normal shutdown.

A reproducible failing cutscene has now been captured with per-stream
instrumentation, which closes the long-standing gap of judging audio by
listening alone. The mechanism is understood and the output path is excluded:

- Decode errors rise from 6.6% to 25.3% of attempts inside the scene while the
  output queue, underruns and seam discontinuities are unchanged.
- 100% of those errors are a frame split across two input buffers whose
  continuation the title has not supplied yet.
- The title keeps one input buffer valid at a time in 97% of samples, so
  holding the consumed buffer deadlocks against its refill.
- Output queue depth is irrelevant: an eight-fold change moved stalls by 1%.
  A different audio backend would not address this defect.

Treating the starved case as a bounded wait
(`naruto_xma_stall_on_missing_input`, opt-in) removes every decode error, raises
decoded frames 23% in the scene, and largely removes the hiss. Intermittency and
repeated fragments remain.

Releasing the consumed buffer without retaining the partial frame is rejected:
it removes the deadlock but soft-locks the cutscene, because discarding data
desynchronizes the title's accounting. **Partial-frame retention across a buffer
swap is the remaining work**, and it is a requirement rather than a refinement.
Detail and evidence: `recomp/fase4/AUDIO_PLAN.md`, `AUDIO-13` to `AUDIO-21`.

### P1 — end of the Orochimaru battle

The previous fatal call to unregistered guest address `0x8215D000` has a narrow
function range in both manifests and in generated registration output. The exact
post-fix scene has not yet been replayed, so the test remains pending.

### P1 — correctly timed 60 FPS and combat validation

Phase 3 proved that the open-world 30 FPS cadence is guest-vblank quantization,
not a hard 32 ms workload. Raising the guest-visible vblank rate from 60 to
120 Hz produced 60-61 FPS in menus and pause and approximately 57-60 FPS in the
open world when scene cost allowed it. The wait and device-ownership bypasses,
the `0x820E8B58` write, and broad half-rate battle-task hooks remain rejected.

Phase 4 direct speed telemetry corrected the battle diagnosis. The single
`sub_82BC8FA8` clock runs in variable mode for menus/open world and fixed mode
for battle. Variable contexts measure real elapsed time and remain at a 1.000
speed factor at either 30 or 60 FPS. Battle instead advances by one configured
step per produced frame, while the port does not vblank-limit that context.
With the frame-rate experiment suspended, log `_105` measured battle near
60 FPS against a 1/30 step and a 1.99 speed factor. The accelerated battle was
a baseline defect that the earlier 1/60 experiment had masked.

The active correction limits a fixed-step frame to the real time delivered for
that frame. Stable frames retain the exact nominal step; only frames produced
too quickly are shortened. A nominal ceiling was rejected because it caused
slow motion whenever achieved FPS fell below the target. The only upper bound
is the title's own maximum delta at clock field `+80`.

The frame-rate target, presentation pacer and guest-vblank multiplier now
derive from one target value. The 60 FPS launcher arms this mode after boot,
and F8 remains a reversible suspension control. Log `_109` measured a 1.000
median over 62 sampled battle windows without a key press. Log `_111` reached
a 119.37 FPS median at 1.000 speed with a 120 FPS target, but that target stays
experimental because frame-counted combat behavior is unmeasured.

The status-jutsu portrait cut-in was explained by 2-4 ms burst frames rather
than a separate animation timeline. The real-time limit removes that source,
but the overlay duration has not been compared at 30 and 60 FPS. No complete
battle has yet validated combo windows, invulnerability, input buffering,
animation, physics and cut-in duration together. Those manual scenarios, plus
a clean build and the 12-trace regression, are the remaining approval boundary.

Current report: `recomp/fase4/FPS_PHASE4_REPORT.md`. Phase 1 through phase 3
remain historical evidence and must be read with their supersession notices.

### P2 — English voice track

A historical cutscene could wait indefinitely for an English voice track while
the Japanese track progressed. Re-test after the active audio-flow work.

### P2 — memory warning

`BaseHeap::Release failed because address is not a region start` has appeared
historically without a visible failure. Correlate it only if it reappears near a
crash or audio loss.

## Recommended next session

1. Run the normal launcher and replay the end of the Orochimaru battle.
2. Implement XMA partial-frame retention across a buffer swap; keep both audio
   interventions opt-in until the same cutscene exits normally without dead or
   repeating streams.
3. Re-run the reproducible cutscene with F7 markers and compare per-stream
   telemetry against the bounded-stall baseline.
4. Measure the same status-jutsu portrait cut-in at 30 and 60 FPS with the
   staged capture launchers and `analyze_cutin.py`.
5. Validate one complete battle at 60 FPS, including combo windows,
   invulnerability, input buffering, animation, physics and cut-in duration.
6. Repeat the clean build and full 12-trace regression after any further code
   change and before release.

## Invariants

- The active `default.xex` remains original.
- D3D12 uses RTV; ROV was slower and visually incorrect on the tested GPU.
- `execute_unclipped_draw_vs_on_cpu` remains enabled.
- The CompareBackEnds hook remains while the embedded Xbox compiler returns
  `E_FAIL`.
- `native/narutobb/generated/default` is regenerated, never edited manually.
- Guest functions enter the manifests only after their exact range is confirmed.
- Audio and 60 FPS experiments remain separate.
- Proprietary game content and proprietary-derived artifacts never enter Git.

## Recording a result

Always record the date, build/hash, launcher and arguments, exact scene and voice
language, expected and observed behavior, whether pause/F10 was used, XMA mode,
log identifier, first relevant marker, and whether the result was automatic,
visual, auditory, or player-confirmed.
