# Archived Xenia compatibility context

This note preserves only the parts of an earlier external-research report that
remain useful to this native recompilation project. It is not the source of
truth for current Xenia compatibility and should not override local runtime
evidence.

## Why the comparison mattered

Historical Xenia reports for *The Broken Bond* described drawing corruption,
English-voice progression problems, and XMA decoder sensitivity. Those symptoms
helped identify relevant subsystems, but emulator configuration flags are not
drop-in fixes for a native ReXGlue build.

Useful conceptual parallels:

- memory-page and upload-range workarounds point toward GPU memory coherence and
  aliasing;
- alternative XMA decoder behavior suggests comparing decoder scheduling and
  buffer transitions;
- RTV/ROV differences highlight the Xbox 360 EDRAM ownership and format problem;
- readback and shader-compilation modes can distinguish synchronization failures
  from missing draw content.

## What local evidence superseded

The native project now has stronger title-specific evidence:

- the Story Mode black-screen bug was reproduced in deterministic GPU traces
  and fixed by restoring CPU estimation of unclipped draw extents;
- the opening native crash was mapped to unsafe FFmpeg flush of an unopened XMA
  context;
- synchronous XMA did not produce a proven auditory advantage;
- the open-world 30 FPS cadence was ultimately proven to be guest-vblank
  quantization; the earlier active render-queue observation was synchronization
  evidence, not an irreducible workload.

Therefore, future work should follow `docs/STATUS.md`,
`docs/TECHNICAL_DECISIONS.md`, and the phase reports rather than copy an Xenia
configuration into ReXGlue.

## Primary upstream references

- Xenia: <https://github.com/xenia-project/xenia>
- Xenia Canary: <https://github.com/xenia-canary/xenia-canary>
- ReXGlue SDK: <https://github.com/rexglue/rexglue-sdk>
- XenonRecomp: <https://github.com/hedge-dev/XenonRecomp>
- XenosRecomp: <https://github.com/hedge-dev/XenosRecomp>

The original Portuguese research dump, including unstable internal citation
markers, is retained only in the ignored `local-research` directory and is not
part of the public documentation set.
