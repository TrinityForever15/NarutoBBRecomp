# Contributing

Thank you for helping improve the native PC recompilation of
*Naruto: The Broken Bond*.

## Before you start

1. Read `README.md`, `docs/STATUS.md`, `docs/PROJECT_STRUCTURE.md`,
   `docs/TECHNICAL_DECISIONS.md`, `docs/TEST_MATRIX.md`, and `SKILL.md` in that
   order.
2. Search existing issues and decisions before proposing a previously rejected
   approach.
3. Keep one pull request focused on one subsystem: audio, graphics, guest CPU,
   FPS/timing, build, or documentation.

## No game content

Do not upload or commit any material copied or generated from the game,
including:

- ISO images, XEX executables, DLC, saves, title updates, or encryption keys;
- audio banks, music, voices, video, fonts, textures, models, or archives;
- extracted or translated proprietary shaders;
- screenshots, frame captures, GPU traces, crash dumps containing game data;
- generated C++ produced from the proprietary executable.

Contributions must work with files supplied locally by a contributor who owns a
legitimate copy. Prefer scripts, hashes, addresses, narrow patches, and
reproduction instructions over bundled inputs or outputs.

## Language

English is required for all new documentation, code comments, issues, commit
messages, and pull requests. Existing identifiers or exact log strings may be
quoted unchanged when needed for accuracy.

## Development workflow

- Put game-specific hooks in `native/narutobb/src`.
- Put reusable runtime fixes in the local `tooling/rexglue-sdk` checkout, commit
  them there by subject, then export and update `patches/rexglue-sdk`.
- Add a missing guest function only after confirming its start and end. Update
  both manifests and regenerate the code.
- Do not modify `default.xex` or generated C++ in place.
- Preserve the default D3D12 RTV path and the cutscene draw-extent fix unless
  new evidence justifies a decision change.

## AI-assisted contributions

This repository already contains substantial work drafted, implemented,
reviewed, or automated with AI coding agents. If AI materially contributes to a
pull request, disclose which parts involved AI and whether its role was
documentation, hypothesis generation, implementation, review, or automation.

The contributor remains responsible for every submitted line. AI output is not
test evidence, and an AI review does not replace a focused human review or the
validation required below.

## Focused review

Do not try to approve the entire project in one pass. New reviewers should
prefer one focused review of `native/narutobb/src/hooks.cpp`,
`patches/rexglue-sdk/0003-fix-audio-stabilize-XMA-and-SDL-stream-recovery.patch`,
or `scripts/test_project_invariants.ps1`, and report concrete findings with a
narrow reproduction or proposed test.

## Validation

At minimum, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test_project_invariants.ps1
```

Runtime or integration changes should also run a clean build and the regression
suite. Manual validation must record the build hash, launcher, exact scene,
language, expected/observed behavior, log identifier, and whether the result was
automatic, visual, auditory, or player-confirmed.

## Pull requests

Explain the problem, evidence, change, validation, known limits, and any updated
documentation. Include the AI-assistance disclosure described above when it
applies. By contributing, you agree that your original contribution is provided
under the repository's BSD 3-Clause License and that you have the right to
submit it.
