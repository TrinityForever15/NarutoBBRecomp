# Test matrix

Last updated: 2026-07-24.

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
| BUILD-01 | Clean Release build | **PASS** | Independent build provenance and hashes recorded locally |
| REG-01 | Invariants, 30 s boot, 12 GPU replays | **PASS** | 12/12 baseline traces at zero numerical delta |
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
| FPS-13 | Menu animation speed at genuine 60 FPS | **FAIL for vblank-only experiment** | Menu reached 60-61 FPS, but player observed doubled animation speed because simulation remained fixed at 1/30 |
| FPS-14 | Guest 120 Hz discriminates quantization from real workload | **PASS for diagnostics** | Logs `_081`, `_082`, `_085`, and `_087` measured 120-123 guest vblanks/s; menu/pause reached 60-61 FPS and open world reached approximately 57-60 FPS when load allowed |
| FPS-15 | Menu 60 FPS with correct UI timing | **PASS for sampled menu session** | Player confirmed normal menu speed; log `_095` recorded 60.00 FPS with 120 Hz guest vblank and the central 1/60 clock |
| FPS-16 | Unified menu-to-world 60 FPS mode | **FAIL for full route** | `_097`: menus and open world retained correct speed, but battle animations accelerated; central clock did not change, so battle has another timing path |
| FPS-17 | Battle 60 FPS with correct animation timing | **FAIL; INVESTIGATION PAUSED** | Battle cadence improved when `sub_829C17E0` ran at half rate, but the jutsu visual remained accelerated; broad half-rate probes were removed |
| FPS-18 | Battle/world guest-function profile comparison | **PASS for diagnostics** | Local Tracy samples isolated battle-only work; counts were normalized over 530 battle and 147 heavily profiled world frames; proprietary traces remain untracked |
| FPS-19 | Half-rate visual candidates | **FAIL; REJECTED** | `sub_82AAF0B8` slowed the fight without fixing the jutsu; `sub_82AB8338` had no visible effect; the refined probe build introduced environment artifacts and was removed |

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
