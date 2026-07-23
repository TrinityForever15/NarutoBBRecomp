# Resume prompt for a coding assistant

Use the block below only with an assistant that does not automatically read
`AGENTS.md`.

```text
You are working on the native PC recompilation of Naruto: The Broken Bond.

Before proposing or making a change, read these files completely and in order:
1. README.md
2. docs/STATUS.md
3. docs/PROJECT_STRUCTURE.md
4. docs/TECHNICAL_DECISIONS.md
5. docs/TEST_MATRIX.md
6. SKILL.md

Then read the area-specific report linked by those documents. Continue from
recorded evidence and do not repeat rejected approaches without new evidence.

Mandatory rules:
- Active project: native/narutobb
- Active modified runtime: tooling/rexglue-sdk
- Portable runtime changes: patches/rexglue-sdk
- Local game data: recomp/fase4/assets
- Do not modify the active default.xex without evidence and explicit approval.
- Do not edit native/narutobb/generated manually; use code generation.
- Preserve unrelated local changes.
- Keep audio, graphics, guest CPU, and FPS diagnostics separate.
- Never commit or distribute proprietary game content or proprietary-derived
  artifacts.
- Write all new public documentation and collaboration text in English.
- Validate changes in proportion to risk.

Before implementation, summarize the current state, controlling files,
decisions that constrain the solution, and validation plan.

After work, update docs/STATUS.md and docs/TEST_MATRIX.md, plus decisions,
project structure, SKILL.md, or a phase report when applicable.

Current task:
[PASTE TASK HERE]
```
