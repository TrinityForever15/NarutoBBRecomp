# Phase 4 — boot unblocked and guest flow reached

Date: 2026-07-19.

## Result

The narrow CompareBackEnds hook bypassed the non-essential embedded compiler.
The guest then progressed through audio initialization and local data setup into
normal update/render activity with multiple guest threads and no crash during
the recorded 40-second Linux run.

Historical backtraces initially labeled `sub_821C1468` as the main game loop.
Later frame-timing instrumentation corrected that interpretation: the function
appears briefly during initialization and is not the current frame boundary.
Use `sub_821B1DD0` for frame timing and consult `FPS_PHASE1_REPORT.md`.

## Additional indirect target

The extended path exposed `0x821C6418`, a narrow thunk, which was added to the
manifest and regenerated. The strict dispatcher later replaced the temporary
missing-function collection mode.

## State after the milestone

- guest threads and Vulkan swapchain were active;
- vblank was delivered around 60 Hz;
- the headless sandbox had no real audio device, so its SDL audio warning was
  expected and not a boot blocker;
- the next milestone was to prove real GPU command submission and then build
  and validate the Windows D3D12 path.

The final CompareBackEnds hook remains active in
`native/narutobb/src/hooks.cpp`. Historical recursive tracing hooks remain only
as reference material and are not used by the active build.
