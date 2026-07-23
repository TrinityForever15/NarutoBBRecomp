# Naruto: The Broken Bond — native PC recompilation

Experimental research project for statically recompiling *Naruto: The Broken
Bond* through ReXGlue. It currently boots into portions of gameplay on Windows,
but it is not yet validated for a complete playthrough.

> [!IMPORTANT]
> This repository contains **no game files, executables, video, audio, fonts,
> shaders extracted from the game, or other Ubisoft/Microsoft assets**. You must
> own a legitimate copy of the game and provide the required files locally.

## Project status

The Windows x64 build boots into normal gameplay and has working D3D12
rendering, input, GPU capture/replay, and basic audio. The following items are
still open:

- validate the end of the Orochimaru battle after adding guest function
  `0x8215D000`;
- capture and fix intermittent/missing audio in the exact affected scenes;
- unlock the open world from its active 30 FPS render-queue synchronization
  while preserving simulation, animation, physics, menus, and cutscenes.

There is no approved open-world 60 FPS mode yet. The old `1/30 -> 1/60`
constant experiment did not work and has been removed.

See [the current status](docs/STATUS.md), [test matrix](docs/TEST_MATRIX.md),
and [technical decisions](docs/TECHNICAL_DECISIONS.md) before changing code.

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
