# Handoff — Story Mode cutscene black screens

Initial handoff: 2026-07-19. Final status: resolved and player-confirmed.

## Symptom

In-engine Story Mode cutscenes alternated among correct frames, full black,
nearly black, black character silhouettes, and broken purple composition. The
game continued updating and presenting during the visual failure. Battle-mode
characters and stages rendered normally.

Audio was intentionally kept outside this investigation.

## Rejected explanations and workarounds

The failure persisted with original 30 FPS timing, full resolve readback,
synchronous shader compilation, and both values of the depth-transfer comparison.

Other tested paths were worse or added unrelated artifacts:

- D3D12 ROV loaded more slowly and produced a mostly black/partial image;
- alternate stencil output produced colored horizontal lines near subtitles;
- broad bindless, tiled-memory, MRT-clamp, MSAA, gamma, and float24 variants did
  not correct the representative local captures.

D3D12 RTV, native stencil, and the normal `NOT_EQUAL` depth-transfer behavior
remain the baseline.

## Local capture and replay tooling

F9 requests one GPU trace through `GraphicsSystem::RequestFrameTrace()`. Twelve
local captures covered normal, black, partial, silhouette, and purple frames.
The capture files and rendered images contain proprietary game data and are not
published.

The project added a headless `narutobb_trace_dump` target using ReXGlue's trace
reader/player. Replay requires synchronous shader compilation; otherwise the
replay may reach swap before pipelines complete and create artificial black
output.

The tool reports numerical framebuffer metrics so approved results can be kept
as a non-proprietary public baseline.

## Deterministic draw bisection

The runtime gained diagnostic controls for:

```text
--trace_draw_limit=N
--trace_draw_skip_begin=A
--trace_draw_skip_end=B
--trace_draw_log_all=true
--trace_draw_log_state=N
```

Indices remain stable when draws are skipped. `bisect_black_draw.ps1` inventories
draws, sweeps copies/resolves, skips selected indices or ranges, and dumps state.

## Fault located

In a representative fully black frame, draw 609 was a full-screen
`kDepthOnly` rectangle. Skipping only that draw recovered the scene. Skipping the
following final resolve also revealed previous color, but the resolve itself was
innocent because EDRAM was already black.

Relevant state included pitch 640, 4x MSAA, and depth base `0x2D0`. The original
working theory was an EDRAM wrap/ownership error in the RTV render-target cache.
Nearly black later captures were propagation through render-to-texture chains
rather than separate root causes.

## Root cause

The SDK had changed Xenia's default
`execute_unclipped_draw_vs_on_cpu` from true to false. Without CPU vertex-shader
extent estimation, the cutscene depth/stencil mask used a large clip-disabled
scissor as its height. The computed depth target claimed the entire 0x800-tile
EDRAM range with wrap. Ownership transfer then copied/reinterpreted the color
target through depth storage and destroyed it.

## Fix

Restore:

```text
execute_unclipped_draw_vs_on_cpu=true
```

Shaders that cannot be interpreted retain the runtime's safe fallback.

## Validation

The local 12-trace replay set showed recovered content in the previously fully
black representative captures and no numerical regression in known-good frames.
Some isolated replays still depended on textures resolved in earlier frames,
so manual in-game validation remained essential.

The player then confirmed that Story Mode cutscene black screens, silhouettes,
and purple composition were fully resolved. This graphics issue is closed.

## Resulting tools and permanent decisions

- Keep D3D12 RTV as the default.
- Keep `execute_unclipped_draw_vs_on_cpu=true`.
- Keep GPU capture/replay and draw bisection available for future graphics bugs.
- Keep captures, screenshots, dumps, and translated proprietary shaders local.
- Do not revisit ROV, alternate stencil, or `ALWAYS` depth transfer without new
  backend evidence.

The next independent targets were the missing guest function at `0x8215D000`,
intermittent/missing audio, and genuine open-world 60 FPS timing.
