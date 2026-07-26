# Naruto: The Broken Bond — native PC recompilation

Experimental research project for statically recompiling *Naruto: The Broken
Bond* through ReXGlue. It currently boots into portions of gameplay on Windows,
but it is not yet validated for a complete playthrough.

> [!WARNING]
> **Research preview:** this repository is engineering research, not a usable or
> supported PC port. Do not promote it as playable until at least one complete
> campaign or another extensive, continuous gameplay sequence has been finished
> and documented on the native build.

> [!IMPORTANT]
> This repository contains **no game files, executables, video, audio, fonts,
> shaders extracted from the game, or other Ubisoft/Microsoft assets**. You must
> own a legitimate copy of the game and provide the required files locally.

## Project status

A maintainer-run Windows x64 build reaches portions of gameplay with D3D12
rendering, input, GPU capture/replay, and basic audio. This is not evidence of a
complete or stable playthrough. The following items are still open:

- validate the end of the Orochimaru battle after adding guest function
  `0x8215D000`;
- finish the cutscene audio fix: the mechanism is measured and understood, but
  intermittency and repeated fragments remain;
- validate combat timing at 60 FPS, and decide whether higher targets are
  viable at all;
- improve scene-dependent performance and validate battles, cutscenes, and a
  long continuous playthrough.

### Frame rate

Simulation speed is now measured directly, as simulated seconds per real
second. That measurement found that **battle is a fixed-timestep context which
the port does not frame-limit**, so the default build had been running battles
at roughly twice speed. Granting each frame no more world time than real time
has delivered corrects this, and sampled battle measures a 1.000 speed median at
60 FPS. Menus and the open world use a variable timestep and were already
correct at any rate.

A 60 FPS mode exists and arms itself at boot. The simulation step, the
presentation pacer target and the guest vblank multiplier all derive from one
target rate, so they cannot disagree. A 120 target reaches a 119.37 FPS median
at correct speed and is marked experimental, because a correct speed factor
says nothing about logic counted in frames, which fighting games commonly use
for combo windows, invulnerability and input buffering. No full battle has been
validated against that.

### Audio

Cutscene audio is measured per stream rather than judged by listening. Inside a
reproducible failing scene, decode errors rise from 6.6% to 25.3% of attempts
while the output queue, underruns and waveform discontinuities stay unchanged,
which excludes the output path. Every error is a frame split across two input
buffers whose continuation the title has not supplied, and the title keeps one
input buffer valid at a time, so holding the consumed buffer deadlocks against
its refill. Varying the output queue depth eight-fold moved the result by 1%,
so a different audio backend would not address this.

Treating that case as a bounded wait removes every decode error and largely
removes the hiss. Retaining the partial frame across a buffer swap is the
remaining work; releasing the buffer without retaining it soft-locks the scene
and is rejected.

See [the current status](docs/STATUS.md), [test matrix](docs/TEST_MATRIX.md),
[technical decisions](docs/TECHNICAL_DECISIONS.md),
[phase-4 FPS report](recomp/fase4/FPS_PHASE4_REPORT.md), and
[audio plan](recomp/fase4/AUDIO_PLAN.md) before changing code.

## What has been independently verified?

At present, **no unaffiliated third party has independently verified the
runtime or completed a long native gameplay sequence**. The independently
hosted verification is deliberately narrow:

- GitHub Actions runs `scripts/test_project_invariants.ps1` on a clean hosted
  runner, checking public-file hygiene, required documentation, manifest
  synchronization, generated registration when present, file sizes, and known
  secret patterns.

Booting Windows builds, reaching portions of gameplay, the cutscene fix, GPU
replays, and audio observations are maintainer-run local results. They are
recorded in the status reports and test matrix, but they must not be described
as independent verification. AI-agent statements are never accepted as test
evidence.

## AI assistance disclosure

This project has made **heavy use of AI coding agents**. Agents have contributed
to:

- drafting, translating, restructuring, and checking documentation;
- proposing debugging hypotheses and narrowing investigation candidates;
- implementing code, runtime patches, diagnostics, and build scripts;
- reviewing diffs and auditing repository hygiene, licensing, and public scope;
- automating builds, regression checks, GitHub workflows, and release tasks.

The maintainer selects which hypotheses to pursue, approves changes, operates
the local game build, and records observed results. AI-generated or AI-reviewed
output may be wrong and should be reviewed like any other untrusted
contribution. Reproducible logs, automated checks, and explicit human
observation—not agent confidence—support project claims.

## Focused review request

Reviewers are not expected to assess the entire project. A useful first review
would focus on one of these three high-value files:

1. [`native/narutobb/src/hooks.cpp`](native/narutobb/src/hooks.cpp) —
   game-specific hooks, timing instrumentation, and assumptions at the
   guest/runtime boundary;
2. [`patches/rexglue-sdk/0003-fix-audio-stabilize-XMA-and-SDL-stream-recovery.patch`](patches/rexglue-sdk/0003-fix-audio-stabilize-XMA-and-SDL-stream-recovery.patch)
   — XMA/SDL queueing, recovery, and real-time audio safety;
3. [`scripts/test_project_invariants.ps1`](scripts/test_project_invariants.ps1)
   — the public-content, manifest, secret, and repository-safety audit.

Please report concrete correctness, safety, race-condition, or reproducibility
issues in one of those files rather than attempting a broad approval of the
whole port.

## Repository layout

```text
native/narutobb/        Game-specific native project and hooks
patches/rexglue-sdk/    Portable patch series applied to upstream ReXGlue
recomp/                 Recompilation tools and historical engineering reports
scripts/                Bootstrap, clean build, invariant, and regression scripts
launchers/              Interactive Windows launch and diagnostic shortcuts
tests/                   Versioned non-proprietary regression baselines
docs/                    Status, architecture, decisions, tests, and legal notes
```

The local ReXGlue checkout, generated C++, builds, logs, traces, and extracted
game data are intentionally excluded from Git. See the
[project structure guide](docs/PROJECT_STRUCTURE.md) for the source-of-truth
rules.

## Quick start

### Prerequisites

- Windows 10/11 x64;
- Git;
- CMake and Ninja;
- LLVM/Clang;
- Visual Studio Build Tools with a Windows SDK;
- a legally obtained copy of *Naruto: The Broken Bond*.

### Prepare local game data

Extract your own game into `recomp/fase4/assets/`. The runtime expects at least
`default.xex` and the data files from the same copy. Do not commit or share this
directory.

The active `default.xex` must remain unmodified. The known supported dump has
SHA-256:

```text
8F70E79443E36B38E44B6A105DAD51FB8909FB615D982753CABAAC9B15F0D576
```

### Build

```powershell
powershell -ExecutionPolicy Bypass -File scripts/bootstrap_rexglue_sdk.ps1
powershell -ExecutionPolicy Bypass -File scripts/build_clean_windows.ps1
```

The bootstrap script clones the pinned upstream ReXGlue commit and applies the
ordered patch series. The SDK checkout is local and is never committed to this
repository.

### Run

Use [`launchers/Play Naruto - PC.bat`](launchers/Play%20Naruto%20-%20PC.bat).
Diagnostic launchers isolate audio scheduling, audio-flow counters, and guest
timing so their results are not mixed.

### Test

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test_project_invariants.ps1
powershell -ExecutionPolicy Bypass -File scripts/run_regression.ps1 `
  -BuildDir native/narutobb/out/build/verification-clean
```

Automated tests verify repository invariants, a controlled boot, and GPU replay
metrics. They do not approve audio quality, story progression, or correct 60 FPS
simulation; those require the documented manual scenarios.

## Contributing

Start with [CONTRIBUTING.md](CONTRIBUTING.md) and read the required documents in
the order defined by [AGENTS.md](AGENTS.md). All new documentation, code
comments, issue templates, commit messages, and pull-request descriptions must
be written in English.

Never submit game data or generated code derived from the XEX. If a change
requires proprietary input, describe a reproducible procedure that contributors
can run against their own legal copy.

## Legal

This is an unofficial fan engineering project. It is not affiliated with,
endorsed by, or sponsored by Ubisoft, Microsoft, Viz Media, Shueisha, Studio
Pierrot, or the creators and rightsholders of *Naruto*.

The repository's original code is available under the
[BSD 3-Clause License](LICENSE). That license does not grant rights to the game,
its assets, trademarks, or third-party code. See [docs/LEGAL.md](docs/LEGAL.md)
and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
