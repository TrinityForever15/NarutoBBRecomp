# Current project status

Last updated: 2026-07-23.

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
- ReXGlue bootstrap from pinned upstream commit plus twelve ordered patches.
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

The local SDK branch `narutobb-integration` contains twelve subject-separated
commits. Pinned upstream base:
`2bdb97f95f154f32d281aaa08446ae007b8ca117`; expected final tree:
`62e97f17f8e6cfb4d73905c5158aef1d8d292151`.

These automated results do not approve perceived audio quality, Story Mode
progression, save/load, or correct 60 FPS simulation.

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

Phase 1 timing diagnostics are complete. Menu, open world, and pause all stay
near 33.3 ms per frame. Explicit guest kernel waits account for only
0.028-0.054 ms, while `sub_8219F990` actively polls render-queue progress for
roughly 31-33 ms. Each frame contains one `VdSwap` and an average of two guest
vblanks. This is an active synchronization cap, not useful-work saturation.

The former `0x820E8B58` candidate is not a global frame cap: static analysis
found 17 reads in 16 functions, and the manual `1/30 -> 1/60` test made no
perceptible difference. The write toggle was removed.

The renderer delta writer is known (`sub_821C0620`), but the simulation delta
writer is not. The next candidate is the writer of fields `+64/+68` in the
object referenced through `0x833A30CC`, consumed by `sub_8276E338` in the open
world. The next intervention must be a reversible runtime experiment that lets
render-queue progress advance once per vblank while separately measuring
simulation speed. **60 FPS is not approved.**

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
7. For FPS work, instrument writes to `+64/+68` in the object referenced by
   `0x833A30CC`.
8. Only then test a reversible per-vblank render-queue advance in the runtime.

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
