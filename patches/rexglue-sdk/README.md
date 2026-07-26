# ReXGlue patch series

- Upstream: <https://github.com/rexglue/rexglue-sdk.git>
- Pinned base: `2bdb97f95f154f32d281aaa08446ae007b8ca117` (local v0.8.0 base)
- Validated local head: `5a89b1adcab5d4295fd20c5c3e29b717cfcf11ad`
- Expected resulting tree: `5a0710954c5f400b58bbba27c8448a255cdd91e4`
- Local development branch: `narutobb-integration`

Patch order:

1. fix POSIX suspended-thread lost wakeup;
2. enable the USB camera exports imported by the title;
3. stabilize XMA and SDL stream recovery;
4. preserve cutscene EDRAM color targets;
5. add pacing and deterministic draw diagnostics;
6. add headless framebuffer replay support;
7. flush only opened FFmpeg contexts;
8. add optional synchronous XMA mode and decoder buffer/progress fixes;
9. add an optional balanced downmix and channel-level diagnostics;
10. revert the balanced downmix after a manual auditory regression;
11. add a passive upstream-silence summary with no real-time logging;
12. add guest frame/scope timing, waits, vblank, `VdSwap`, timebase sources,
    and correct F10 system-key delivery on Windows;
13. expose the live guest-vblank counter and guest wait caller attribution for
    controlled runtime experiments;
14. support a controlled guest-vblank rate multiplier, precise sub-refresh
    waits, and worker timing for reversible title experiments;
15. derive the Naruto presentation pacer target from a cvar so it cannot
    disagree with the simulation step of a fixed-timestep context;
16. add per-stream XMA continuity and output flow telemetry plus opt-in
    split-buffer stall/release diagnostics, accumulated outside the real-time
    audio callback.

Reconstruct from the pinned upstream base:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/bootstrap_rexglue_sdk.ps1
```

If a local checkout is already exactly at the pinned base, run
`scripts/apply_rexglue_patches.ps1` directly.

These patches are the portable source of the runtime changes. The local SDK
checkout contains equivalent subject-separated commits but is never committed
to the main repository. `git am` may produce different commit IDs because of
committer metadata; the final tree hash must match.

ReXGlue and Xenia-derived portions retain their upstream BSD 3-Clause license
and copyright notices. See `THIRD_PARTY_NOTICES.md`.
