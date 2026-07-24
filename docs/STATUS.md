# Current project status

Last updated: 2026-07-24.

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
- ReXGlue bootstrap from pinned upstream commit plus fourteen ordered patches.
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

## Reproducibility evidence

The clean Release build in
`native/narutobb/out/build/verification-audio-silence-summary` completed without
reusing the active cache. Its automated run verified project invariants, a
30-second boot without fatal errors, and zero numerical delta across the 12 GPU
replays in the baseline. A separate normal shutdown emitted
`NARUTO_AUDIO_SILENCE_SUMMARY` only after the SDL stream was destroyed.

The local SDK branch `narutobb-integration` contains fourteen subject-separated
commits. Pinned upstream base:
`2bdb97f95f154f32d281aaa08446ae007b8ca117`; expected final tree:
`5144c7af01ce1483a5c59cbde7e419517f5a062e`.

These automated results do not approve perceived audio quality, Story Mode
progression, save/load, or correct 60 FPS simulation.

The independent phase-2 FPS Release build at
`native/narutobb/out/build/verification-fps-phase2` also completed codegen,
game/runtime, and replay-tool compilation. Its normal 30-second boot passed,
and the exact 12-trace GPU baseline reproduced with zero numerical delta. One
additional local trace has no public baseline and is intentionally excluded
from that approved regression set rather than treated as a visual pass.

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
`NARUTO_AUDIO_SILENCE_SUMMARY` during normal shutdown. The exact scene where a
sound disappears still needs to be captured with this mode.

### P1 — end of the Orochimaru battle

The previous fatal call to unregistered guest address `0x8215D000` has a narrow
function range in both manifests and in generated registration output. The exact
post-fix scene has not yet been replayed, so the test remains pending.

### P1 — genuine open-world 60 FPS

Phase 3 proved that the 30 FPS cadence is vblank-quantized rather than a hard
32 ms workload. A reversible runtime multiplier delivered a measured 120-123
guest vblanks/s. Menu and pause immediately produced 60-61 complete guest
frames/s, while the open world produced approximately 57-60 FPS when scene cost
allowed it. Returning the multiplier to one immediately restored 30 FPS.

The main renderer-ready wait is called from `0x82160E4C`; the render work-event
wait is called from `0x82161160`. A temporary zero-timeout diagnostic for the
main wait did not change FPS and moved the latency to `0x8215AF20`, which is the
recursive guest graphics-device ownership acquire. That bypass was removed.
Event and device-ownership synchronization must not be bypassed.

The former `0x820E8B58` candidate is not a global frame cap: static analysis
found 17 reads in 16 functions, and the manual `1/30 -> 1/60` test made no
perceptible difference. The write toggle was removed.

The renderer delta at `sub_821C0620` dynamically changed from approximately
33.4 ms to 16.7 ms. The simulation did not: dynamic store tracing identified
`sub_82BC8FA8`, specifically guest PCs `0x82BC9000/0x82BC9008`, writing
`0.033333` to fields `+68/+64` once per produced frame. This directly explains
the player-confirmed doubled animation speed at 60 FPS. The broad generated
store instrumentation was removed after identifying the writer.

The current opt-in world experiment now substitutes 1/60 in the clock's fixed
step field `+76` before `sub_82BC8FA8` derives deltas and internal ticks. It
captures and restores the original value when F8 is suspended or the guarded
world context ends. Codegen, a full source build, the 15-second default boot
(`narutobb_088.log`), and project invariants passed. In manual log `_089`, the
player confirmed 60 FPS with normal animation speed. The same run dipped as low
as 41 FPS and contained a sustained 43-52 FPS interval: render-frame time rose
from approximately 16.6 ms to 19-23.4 ms while the 120-123 Hz vblank worker
remained healthy. With the expensive timing trace disabled in log `_090`, the
player observed a minimum near 56 FPS. That log also exposed repeated automatic
guard transitions caused by intermittent world-consumer markers. The guard now
validates the initial world context for 30 frames and then latches until F8,
instead of reverting during pause or temporary marker gaps. Build, default boot
log `_091`, and invariants passed. Runtime log `_092` validated the latch: one
activation at 120 Hz, one 1/60 clock substitution, no guarded transition, and
no pacer bypass for the remainder of the recorded session. The player's final
assessment reported a minimum around 55-56 FPS with instability no longer very
perceptible. The same 1/60 central-clock correction is now shared by the clean
menu 120 Hz experiment. The player confirmed correct menu speed, and log `_095`
recorded exactly 60.00 FPS with one 120 Hz activation and the 1/60 clock.

A unified `naruto_60fps_experiment` mode and launcher now combine the validated
menu and world behavior. They start disarmed; F8 enables 120 Hz guest vblank and
the 1/60 simulation clock across menus, pause, transitions, and gameplay, while
a second F8 restores the captured original timing. Build, default boot log
`_096`, and invariants passed. End-to-end manual validation of the unified
launcher is pending.
Unified manual log `_097` confirmed correct menus and open world, but battle
animations ran too fast. The battle did not switch the central clock object:
the same object remained under the 1/60 substitution for the whole route. This
proves that battle animation has an additional fixed-step or frame-count timing
path.

Local Tracy captures then compared a ten-second battle sample (530 profiled
frames, 3,879 guest functions) with an open-world sample (147 heavily profiled
frames, 3,398 guest functions). Counts were normalized per frame because the
instrumented world capture was much slower. The comparison rejected collision,
spatial, scheduler, and known central-clock readers as the missing animation
clock. A reversible half-rate probe found that task `sub_829C17E0` made the
battle cadence look correct, but the on-screen jutsu remained accelerated.
That task is therefore evidence for a separate battle state/command cadence,
not a complete fix. Halving state-7 routine `sub_82AAF0B8` slowed the fight
without correcting the jutsu, while halving task `sub_82AB8338` had no visible
effect. The refined probe build also produced new environment artifacts.

All battle half-rate hooks and the F11 launcher were removed. The retained
unified experiment is still suitable for menus and sampled open-world play,
but **global 60 FPS is not approved** because battles remain incorrectly timed.
Detailed evidence and rejected paths are in `recomp/fase4/FPS_PHASE3_REPORT.md`.

### P2 — English voice track

A historical cutscene could wait indefinitely for an English voice track while
the Japanese track progressed. Re-test after the active audio-flow work.

### P2 — memory warning

`BaseHeap::Release failed because address is not a region start` has appeared
historically without a visible failure. Correlate it only if it reappears near a
crash or audio loss.

## Recommended next session

1. Run the normal launcher and replay the end of the Orochimaru battle.
2. Record audio behavior without using pause as an immediate workaround.
3. Repeat a scene where pause/resume previously changed music or removed noise.
4. Save the generated log number and close normally.
5. Replay the known missing-sound scene with the audio-flow diagnostics launcher,
   without changing language, volume, or graphics settings.
6. Close normally and correlate the approximate failure time with
   `NARUTO_AUDIO_SILENCE_SUMMARY`.
7. When FPS work resumes, keep the validated 120 Hz vblank plus central 1/60
   clock for menu/open-world tests, but do not treat it as a global mode.
8. Resume from the jutsu-specific visual timeline or animation-state writer.
   Do not repeat broad half-rate skips of `sub_829C17E0`, `sub_82AAF0B8`, or
   `sub_82AB8338`, and do not bypass event/device-ownership synchronization.

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
