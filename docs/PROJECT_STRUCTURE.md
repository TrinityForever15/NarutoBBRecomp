# Project structure and source-of-truth map

Last updated: 2026-07-24.

This guide explains where work belongs, which copies are authoritative, and
which local artifacts must never be published.

## Top-level layout

```text
Naruto project/
├── README.md                   Public entry point
├── AGENTS.md                   Mandatory workflow for contributors/agents
├── SKILL.md                    Detailed technical knowledge base
├── CONTRIBUTING.md             Contribution and legal-content rules
├── LICENSE                     License for original repository material
├── THIRD_PARTY_NOTICES.md      Upstream license and attribution notes
├── docs/                       Status, decisions, tests, legal and research
├── launchers/                  Interactive Windows shortcuts
├── native/narutobb/            Active native game project
├── patches/rexglue-sdk/        Portable ReXGlue patch series
├── recomp/                     Recompilation tools and historical reports
├── scripts/                    Bootstrap/build/test automation
├── tests/                      Non-proprietary numerical baselines
├── tooling/rexglue-sdk/        Local modified SDK checkout (ignored)
└── local-research/             Private/proprietary-derived research (ignored)
```

## Source-of-truth rules

1. The compiled game project is `native/narutobb`.
2. The active runtime is built from the local `tooling/rexglue-sdk` checkout.
3. Portable runtime changes are the ordered patches in `patches/rexglue-sdk`.
4. The active game manifest is `native/narutobb/narutobb_manifest.toml`.
5. `recomp/fase4/narutobb_manifest.toml` mirrors the same explicit function
   ranges but uses phase-relative paths.
6. `native/narutobb/generated/default` is disposable codegen output and must
   never be edited or published.
7. Game-specific fixes belong in `native/narutobb/src`.
8. Reusable CPU, GPU, audio, kernel, or UI fixes belong in ReXGlue and must be
   exported back into the patch series.
9. `native/narutobb/out`, logs, captures, builds, and `artifacts` are disposable.
10. Game data under `recomp/fase4/assets` and private research under
    `local-research` remain local and never enter Git.

## Public repository files

| Path | Responsibility |
|---|---|
| `README.md` | Public overview, prerequisites, build/run/test path, legal warning |
| `AGENTS.md` | Required reading order and engineering constraints |
| `SKILL.md` | Current technical facts, diagnostics, addresses, and history |
| `docs/STATUS.md` | Short operational snapshot and next session |
| `docs/TECHNICAL_DECISIONS.md` | Active architecture and rejected approaches |
| `docs/TEST_MATRIX.md` | Evidence-backed validation state |
| `docs/LEGAL.md` | Content policy and user-supplied-data model |
| `CONTRIBUTING.md` | Contribution workflow and review expectations |
| `SECURITY.md` | Private vulnerability-reporting guidance |

## Active native project

### Configuration

| Path | Role |
|---|---|
| `native/narutobb/CMakeLists.txt` | Builds the game executable and trace replay tool |
| `native/narutobb/CMakePresets.json` | Windows/Linux and architecture build presets |
| `native/narutobb/narutobb_manifest.toml` | Active XEX input and explicit guest-function ranges |
| `native/narutobb/generated/rexglue.cmake` | Generated integration entry point that is safe to version |

### Game-specific source

| Path | Role |
|---|---|
| `src/main.cpp` | Native application entry point |
| `src/narutobb_app.h` | Runtime configuration, data path, F3/F8/F9/F10 bindings |
| `src/hooks.cpp` | Frame/timing probes and CompareBackEnds hook |
| `src/frame_stats.h` | Shared frame-statistics interface |
| `src/usbcam_stubs.cpp` | Conditional compatibility for an older binary SDK |
| `tools/trace_dump_main.cpp` | Headless `.xtr` replay entry point |

### Generated and build output

- `native/narutobb/generated/default`: roughly 97 generated C++ translation
  units plus registration files. Regenerate; never edit or publish.
- `native/narutobb/out`: executables, runtime libraries, CMake state, and logs.
  Recreate; never publish.

## ReXGlue runtime

`tooling/rexglue-sdk` is a separate local Git checkout and is ignored by the
main repository. The modified branch is `narutobb-integration`. Relevant areas:

| Area | Runtime paths |
|---|---|
| Audio | `include/rex/audio`, `src/audio` |
| XMA | `include/rex/audio/xma`, `src/audio/xma_*` |
| Graphics | `include/rex/graphics`, `src/graphics` |
| Timing | `include/rex/diagnostics/guest_timing.h`, `src/system/guest_timing.cpp` |
| Kernel/threads | `src/kernel`, `src/core/threading_*`, `src/system/xthread.cpp` |
| Input/window | `src/input`, `src/ui/window_win.cpp` |
| Code generation | `src/codegen` |

The patch series currently covers POSIX thread startup, USB camera exports,
XMA/SDL stability, cutscene EDRAM correctness, pacing/draw diagnostics,
headless replay, safe FFmpeg flush, synchronous-XMA comparison, an explicitly
reverted downmix experiment, passive audio-flow summaries, and guest timing/F10
diagnostics. The latest timing patch also exposes a live guest-vblank counter
and attributes waits to guest callers for controlled runtime experiments.

## Recompilation and historical material

| Path | Role |
|---|---|
| `recomp/FEASIBILITY_REPORT.md` | Initial CPU/shader viability study |
| `recomp/fase3/PHASE3_REPORT.md` | First native Linux boot |
| `recomp/fase4/PHASE4_REPORT.md` | Complete local data extraction and first phase-4 boot |
| `recomp/fase4/PHASE4_ROOT_CAUSE_REPORT.md` | Lost-wakeup and embedded compiler diagnosis |
| `recomp/fase4/PHASE4_BOOT_REPORT.md` | CompareBackEnds bypass and guest-loop milestone |
| `recomp/fase4/PHASE4_WINDOWS_REPORT.md` | Chronological Windows engineering record |
| `recomp/fase4/HANDOFF_CUTSCENES_GPU_2026-07-19.md` | Cutscene GPU diagnosis and fix |
| `recomp/fase4/FPS_PHASE1_REPORT.md` | Initial 30 FPS timing and queue evidence |
| `recomp/fase4/FPS_PHASE2_REPORT.md` | Menu intervention and serialized main/render diagnosis |
| `recomp/fase4/FPS_PHASE3_REPORT.md` | 120 Hz/1/60 result, battle profiling, and rejected battle-task probes |
| `recomp/fase4/xdvdfs_extract_all.py` | Extracts data from a user-owned disc image locally |
| `recomp/fase4/bisect_black_draw.ps1` | Local GPU-trace draw bisection |

Historical patches for XenonRecomp/XenosRecomp remain available as engineering
references. Generated shader archives and original Ubisoft shader sources are
explicitly excluded.

## Launchers

| File | Purpose |
|---|---|
| `launchers/Play Naruto - PC.bat` | Normal baseline |
| `launchers/Play Naruto - Timing Diagnostics.bat` | Guest timing trace; does not enable 60 FPS |
| `launchers/Play Naruto - Menu 60 FPS Experiment.bat` | Opt-in reproduction of the rejected one-vblank queue intervention; not an approved 60 FPS mode |
| `launchers/Play Naruto - Menu 120 Hz Vblank Experiment.bat` | Menu-only 120 Hz plus central 1/60 timing experiment |
| `launchers/Play Naruto - Open World 120 Hz Vblank Experiment.bat` | Guarded open-world 120 Hz plus central 1/60 timing experiment |
| `launchers/Play Naruto - 60 FPS Experiment.bat` | Unified opt-in 120 Hz guest-vblank plus 1/60 simulation-clock mode; F8 enables/restores it across menus and gameplay |
| `launchers/Play Naruto - 60 FPS Tracy Profile.bat` | Separate profiling build; local traces only |
| `launchers/Capture Naruto Battle Profile - 10 Seconds.bat` | Capture a bounded local battle Tracy sample |
| `launchers/Capture Naruto Open World Profile - 10 Seconds.bat` | Capture a bounded local open-world Tracy sample |
| `launchers/Play Naruto - Audio Flow Diagnostics.bat` | Passive six-channel silence/activity summary |
| `launchers/Play Naruto - Synchronous XMA Test.bat` | Controlled XMA scheduling comparison |
| `launchers/Rebuild Diagnostics.bat` | Codegen, configure, game/runtime and trace tool |
| `launchers/Run GPU Replay Diagnostics.bat` | Replays the local 12-trace set |
| `launchers/Run Draw Bisection.bat` | Runs local draw bisection scenarios |

All launchers resolve the repository root as their parent directory.

## Automation

| Script | Purpose |
|---|---|
| `bootstrap_rexglue_sdk.ps1` | Clone pinned upstream SDK and apply patches |
| `apply_rexglue_patches.ps1` | Validate and apply only the known patch series |
| `build_clean_windows.ps1` | Independent Release configure/codegen/build with provenance |
| `test_project_invariants.ps1` | Docs, links, content policy, manifests, and local XEX hash |
| `test_boot.ps1` | PID-scoped controlled boot |
| `test_gpu_replay.ps1` | Replay local traces against public numeric baselines |
| `run_regression.ps1` | Orchestrate invariants, boot, and replay |

## Where to investigate

| Symptom | Start here |
|---|---|
| Boot failure | latest local log, `narutobb_app.h`, `hooks.cpp`, runtime UI |
| Missing guest address | fatal log and both manifests |
| Orochimaru scene | registered `0x8215D000` range and exact scene replay |
| Crackle/missing audio | SDL driver, XMA context/decoder, audio summary markers |
| Open world at 30 FPS | `hooks.cpp`, guest timing, `sub_82160E28`, `sub_8219F990`, FPS phase-1 and phase-2 reports |
| Cutscene corruption | draw extent estimator, render-target cache, local trace replay |
| Input | `narutobb_app.h`, runtime input and Win32 key dispatch |
| Missing files | VFS log and local `recomp/fase4/assets` directory |

## Safe-to-ignore local output

Generated guest C++, build trees, old logs, XTR/BMP/CSV outputs, local SDK
third-party code, and extracted game media are not source. Do not spend review
time on them and never add them to Git.
