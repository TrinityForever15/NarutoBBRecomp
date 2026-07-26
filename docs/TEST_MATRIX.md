# Test matrix

Last updated: 2026-07-25.

Status values: **PASS** means the full stated scenario was observed;
**PARTIAL** means only part was proven; **PENDING** means a change or plan exists
without sufficient observation; **FAIL** means the defect was reproduced;
**HISTORICAL** preserves a result from an older configuration.

## Compatibility and stability

| ID | Scenario | Status | Evidence / next step |
|---|---|---|---|
| BOOT-01 | Windows executable reaches game flow | **PASS** | Post-fix active build survived 45 s and clean build survived 30 s without APPCRASH; repeat after code changes |
| BOOT-02 | Load shader storage and pipelines | **PASS** | A validated log loaded 1,329 shaders and 2,017 pipelines |
| VFS-01 | Mount user-supplied game data | **PASS locally** | Runtime mounts `recomp/fase4/assets`; public repository intentionally has no data |
| LOOP-01 | Stable guest threads/game flow | **PASS** | Backtraces and Windows runs; revalidate in a long session |
| BUILD-01 | Clean Release build | **PASS** | `verification-phase4-audit` independently configured, regenerated code and built game/runtime/replay tool |
| REG-01 | Invariants, 30 s boot, 12 GPU replays | **PASS** | Phase-4 audit boot passed and 12/12 approved baseline traces reproduced at zero numerical delta; two exploratory local captures without baselines were explicitly skipped |
| SAVE-01 | Save, restart, and load | **PENDING** | No complete recorded scenario |
| PUB-01 | Public tree contains no prohibited game content | **PASS by automated audit** | Local invariant and GitHub Actions passed for public commit `765fa82562a5c1f6085ef0f2b22bccad333c642e`; checks tracked names/extensions, large files, and known secret patterns |
| PUB-02 | Public `main` matches the sanitized local tree | **PASS** | Verified after fetching `origin/main`; local and remote trees matched and only `main` was published |

## Guest CPU and story

| ID | Scenario | Status | Evidence / next step |
|---|---|---|---|
| CPU-01 | CompareBackEnds cannot block boot | **PASS** | Hook active; one warning, boot proceeds |
| CPU-02 | No missing functions in covered flow | **PASS** | Current covered paths run without fatal dispatch |
| STORY-01 | End of Orochimaru vs. Fourth Hokage | **PENDING** | `0x8215D000-0x8215D03C` is generated and registered; replay exact scene |
| BATTLE-01 | Battle -> Dojo | **PARTIAL** | Functions added; explicit full replay still required |
| JUTSU-01 | Naruto jutsu completes | **PASS** | Manual replay completed without a new fatal address |

## Graphics and cutscenes

| ID | Scenario | Status | Evidence / next step |
|---|---|---|---|
| GPU-01 | D3D12 on RX 6650 XT | **PASS** | Adapter and feature initialization in Windows logs |
| GPU-02 | Story cutscenes render correctly | **PASS** | Player-confirmed after unclipped draw-extent fix |
| GPU-03 | D3D12 RTV path | **PASS** | Active path plus manual and replay evidence |
| GPU-04 | D3D12 ROV path | **HISTORICAL — REJECTED** | Slow and mostly black/partial on tested GPU |
| TRACE-01 | F9 capture and local replay | **PASS** | 12 local traces and replay outputs; never publish captures |
| TRACE-02 | 12-trace numerical baseline | **PASS** | Zero delta in the latest clean regression |

## Audio

| ID | Scenario | Status | Evidence / next step |
|---|---|---|---|
| AUDIO-01 | SDL 6-channel guest to stereo device | **PASS** | 48 kHz stereo output initialized |
| AUDIO-02 | Long gameplay/cutscene without crackle/dropouts | **PENDING** | Light crackle remained in `_054`; repeat baseline with passive diagnostics |
| AUDIO-03 | Pause/resume preserves correct music | **PENDING** | Replay known scene after current XMA clear changes |
| AUDIO-04 | Scene/music transition has no stale samples | **PENDING** | Exercise multiple transitions |
| AUDIO-05 | Underrun recovery avoids click bursts | **PENDING** | Refill/fades exist; correlate sound with underrun markers |
| AUDIO-06 | Strong effects do not clip | **PENDING** | Center peak 0.524 and no output clipping in `_054`; correlate light crackle with source silence/underrun |
| AUDIO-07 | English voice cutscene progresses | **PENDING** | Historical indefinite wait; compare English/Japanese same scene |
| AUDIO-08 | Clear unopened XMA context safely | **PASS** | `avcodec_is_open` fix and post-failure-point boots |
| AUDIO-09 | Synchronous XMA boots | **AUTOMATED PASS** | Active and clean smoke tests logged synchronous mode |
| AUDIO-10 | Synchronous XMA improves perceived audio | **NO BENEFIT PROVEN** | Dedicated and synchronous tests were not audibly distinguished |
| AUDIO-11 | Balanced stereo downmix | **FAIL — REJECTED** | Worse dropouts, no missing sound recovered; reverted |
| AUDIO-12 | Passive upstream silence summary | **AUTOMATED PASS; SCENE PENDING** | Summary emitted after normal shutdown; capture exact missing-sound scene |
| AUDIO-13 | Classify cutscene audio defects by measurement | **PASS for diagnostics** | `_112`: decode errors rise from 6.6% to 25.3% of attempts inside the scene while output queue, underruns and seam discontinuities stay unchanged, so the output path is not implicated |
| AUDIO-14 | Identify the decode error | **PASS for diagnostics** | `_113`: 100% of errors are a split frame whose next input buffer the guest had not supplied, and 100% of those are starved. No malformed header occurred |
| AUDIO-15 | Treat a missing next input buffer as a stall | **PARTIAL** | `_114`: decode errors eliminated and decoded frames up 23% in scene; maintainer reported hiss largely gone. An unbounded stall left seven streams silent, one for 21 s, so the wait is now bounded |
| AUDIO-16 | Bounded stall keeps ending streams alive | **PARTIAL** | `_115`: no permanent silences; contexts ending normally show 7-12 timeouts per 50 s. Some contexts still cycle through stall and timeout several times per second |
| AUDIO-17 | Frame rate mode is not an audio factor | **PASS** | `_116`: same scene with the experiment suspended gave 87.1 stalls and 9.2 timeouts per second against 88.0 and 9.8 active |
| AUDIO-18 | Cutscene audio fully correct | **FAIL; INVESTIGATION OPEN** | Intermittency and repeated fragments remain. Two distinct pathologies identified; the affected context set changes between runs |
| AUDIO-19 | Output queue depth affects the defect | **FAIL — RULED OUT** | `_117`/`_118`: 128 against 16 frames moved stalls 1%, timeouts 1%, frames 0%, with zero underruns in both. A different audio backend would not address this |
| AUDIO-20 | Identify the stall mechanism | **PASS for diagnostics** | The title keeps one input buffer valid in 97% of samples, so holding the consumed buffer deadlocks against the guest refill. Every observed guest write to the read offset was a rewind |
| AUDIO-21 | Release the buffer without retaining the split frame | **FAIL — REJECTED** | `_119`: stalls and timeouts fell to zero, but contexts with no valid input rose from 0.3% to 8%, streams died and the cutscene soft-locked at its end. Discarding the partial frame desynchronizes the title's data accounting |

## FPS and timing

| ID | Scenario | Status | Evidence / next step |
|---|---|---|---|
| FPS-01 | Content already produced near 60 FPS is paced | **HISTORICAL PASS** | Approximately 59.7-60.3 FPS in pacer tests |
| FPS-02 | Original 30 FPS content is not mislabeled | **PASS** | Pacer bypassed at approximately 30.85 FPS |
| FPS-03 | Diagnostic launcher boots with original XEX | **AUTOMATED PASS** | No timing write; 30 s boot without fatal |
| FPS-04 | Old `0x820E8B58` toggle doubles open-world FPS | **FAIL — REJECTED** | No perceptible change; write removed |
| FPS-07 | Identify active constant consumers by context | **PASS for diagnostics** | Menu none; world `82199B00`, `8276E338`, `8281CEF0`; pause only `82199B00` |
| FPS-08 | Classify the 33.3 ms frame | **PASS for diagnostics; PHASE-1 INTERPRETATION REVISED** | Menu is two serialized approximately 16 ms active main/render legs plus their event handoff, not a removable two-vblank sleep |
| FPS-09 | Identify simulation delta writer | **PASS for diagnostics** | Log `_087` identified `sub_82BC8FA8`: PCs `82BC9000/82BC9008` write fixed 1/30 to world-clock `+68/+64` once per frame |
| FPS-10 | F10 starts/stops trace without breaking Alt+F4 | **PASS** | Frames 191-236 traced; normal Alt+F4 shutdown |
| FPS-11 | Genuine open-world 60 FPS with correct simulation | **PARTIAL; SAMPLED WORLD TIMING PASS** | `_089` confirmed correct animation speed; `_092` kept 120 Hz/1/60 latched with no pacer bypass; player observed a 55-56 FPS minimum with little perceptible instability |
| FPS-12 | Menu one-vblank queue-release experiment | **FAIL — REJECTED** | Hook forced releases after one real vblank, but output stayed near 30 FPS; removing the next wait only moved latency to device ownership |
| FPS-13 | Menu animation speed at genuine 60 FPS | **HISTORICAL — INTERPRETATION SUPERSEDED** | The vblank-only run was perceived as doubled speed, but later direct telemetry classified menus as variable timestep at 1.000 speed. Keep the observation as history; do not use it as proof that menus require a fixed-step substitution |
| FPS-14 | Guest 120 Hz discriminates quantization from real workload | **PASS for diagnostics** | Logs `_081`, `_082`, `_085`, and `_087` measured 120-123 guest vblanks/s; menu/pause reached 60-61 FPS and open world reached approximately 57-60 FPS when load allowed |
| FPS-15 | Menu 60 FPS with correct UI timing | **PASS for sampled menu session** | Player confirmed normal menu speed; log `_095` recorded 60.00 FPS with 120 Hz guest vblank and the central 1/60 clock |
| FPS-16 | Unified menu-to-world 60 FPS mode | **FAIL for full route; CAUSE REATTRIBUTED** | `_097` accelerated battle was read as an experiment defect. Speed telemetry later proved battle runs on a fixed timestep and was already near twice speed in the default build, so the experiment was masking a baseline bug rather than causing one. See FPS-20 |
| FPS-17 | Battle 60 FPS with correct animation timing | **FAIL; SUPERSEDED BY FPS-20** | Broad half-rate probes were removed. The premise that battle needed a jutsu-specific animation writer did not survive telemetry |
| FPS-18 | Battle/world guest-function profile comparison | **PASS for diagnostics** | Local Tracy samples isolated battle-only work; counts were normalized over 530 battle and 147 heavily profiled world frames; proprietary traces remain untracked |
| FPS-19 | Half-rate visual candidates | **FAIL; REJECTED** | `sub_82AAF0B8` slowed the fight without fixing the jutsu; `sub_82AB8338` had no visible effect; the refined probe build introduced environment artifacts and was removed |
| FPS-20 | Classify simulation speed per context | **PASS for diagnostics** | `_105` measured simulated seconds per real second directly. Menu and open world are variable timestep at 1.000; battle is fixed timestep and measured 1.99 with the experiment suspended, proving the default build ran battle near twice speed |
| FPS-21 | Battle at correct speed in the default build | **PASS for sampled battle** | `_109` recorded a 1.000 median over 62 battle samples, minimum 0.928 and maximum 1.245, with the real-time step limit active and no key press. Player confirmed in game. Not a full-battle approval: combo windows, invulnerability and cut-in duration were not measured |
| FPS-22 | Frame rates below the target keep correct speed | **PASS for sampled world entry** | Capping the step at nominal caused slow motion entering the open world at a 120 target. Over samples between 30 and 110 FPS, mean speed was 0.864 with a 0.282 minimum before the fix and 1.000 with a 0.650 minimum in `_111` after it |
| FPS-23 | 120 FPS target | **PARTIAL; EXPERIMENTAL** | `_111` reached a 119.37 FPS median at 1.000 speed with vblank multiplier 4, and the player reported the game could not always hold 120. Speed correctness does not approve frame-counted combat timing, which remains unmeasured |
| FPS-24 | Battle status-jutsu portrait cut-in duration | **PENDING** | Explained by burst frames rather than a dedicated timeline writer, and the burst source is corrected, but cut-in duration was never measured at 30 against 60. Tracy capture launchers and `recomp/fase4/analyze_cutin.py` are staged for this |

## Public-release validation template

```text
ID:
Date:
Build/SHA:
Launcher and arguments:
Scene and voice language:
Steps:
Expected:
Observed:
Log identifier:
Relevant markers:
Result: PASS / PARTIAL / PENDING / FAIL
Evidence type: automated / visual / auditory / player-confirmed
Notes:
```

A successful compile, window creation, or short boot never approves audio,
story progression, save/load, or 60 FPS simulation.
