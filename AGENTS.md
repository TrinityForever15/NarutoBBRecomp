# Mandatory instructions for contributors and coding agents

Goal: complete the missing work needed to run *Naruto: The Broken Bond*
natively on PC with stable gameplay, correct audio, and a correctly timed
60 FPS open world.

## Before changing anything

Read these files completely and in this order:

1. `README.md`
2. `docs/STATUS.md`
3. `docs/PROJECT_STRUCTURE.md`
4. `docs/TECHNICAL_DECISIONS.md`
5. `docs/TEST_MATRIX.md`
6. `SKILL.md`

Then read the area-specific report linked by those documents. Continue from the
recorded evidence; do not restart the investigation from scratch.

## Active sources

- Game project: `native/narutobb`
- Modified runtime checkout: `tooling/rexglue-sdk`
- Portable runtime changes: `patches/rexglue-sdk`
- Local game data: `recomp/fase4/assets`
- Launcher build: `native/narutobb/out/build/win-amd64-source`

## Engineering rules

- Never patch the active `default.xex` without evidence and explicit approval.
- Never edit `native/narutobb/generated` manually; run code generation.
- Preserve unrelated local changes.
- Keep both guest-function manifests synchronized.
- Diagnose audio, graphics, guest functions, and FPS independently.
- Do not repeat rejected approaches from `docs/TECHNICAL_DECISIONS.md` without
  new evidence.
- Validate changes in proportion to risk and record evidence or logs.
- Do not commit the local SDK checkout; update its commits and re-export the
  patch series.
- Run invariants for integration changes and, when applicable, a clean build
  and regression suite.
- Never commit or distribute copyrighted game data, extracted shaders, fonts,
  screenshots, video, audio, XEX files, generated guest C++, GPU traces, or
  decryption material.
- All new documentation, code comments, issue templates, commit messages, and
  pull-request descriptions must be written in English.

## After working

- Update `docs/STATUS.md` with the real state and remaining work.
- Update `docs/TEST_MATRIX.md`; never mark a scenario as passed unless the full
  scenario was observed.
- Update `docs/TECHNICAL_DECISIONS.md` when a decision changes or an approach is
  rejected.
- Update `docs/PROJECT_STRUCTURE.md` when paths or responsibilities change.
- Record detailed technical discoveries in `SKILL.md` or the relevant phase
  report.
