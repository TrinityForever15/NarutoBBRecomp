# Local automation

| Script | Purpose |
|---|---|
| `bootstrap_rexglue_sdk.ps1` | Clone the pinned upstream SDK, initialize submodules, and apply the local patch series |
| `apply_rexglue_patches.ps1` | Validate the checkout and recreate the modified branch from the twelve patches |
| `build_clean_windows.ps1` | Configure an independent build, run codegen, build game/runtime/replay tool, and record provenance/hashes |
| `test_project_invariants.ps1` | Check required docs, local links, English policy, manifests, public-content policy, local XEX hash, and generated registration |
| `test_boot.ps1` | Start only the test-owned process, wait 30 seconds, terminate by PID, and validate its log |
| `test_gpu_replay.ps1` | Replay the twelve local traces, compare public numeric baselines, and write local JSON/CSV results |
| `run_regression.ps1` | Run invariants, boot, and replay in sequence |

Examples from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_clean_windows.ps1
powershell -ExecutionPolicy Bypass -File scripts/run_regression.ps1 `
  -BuildDir native/narutobb/out/build/verification-clean
```

Results are written under the ignored `artifacts/test-results` directory. Local
game data and GPU traces are prerequisites for the full regression and must
never be distributed.

The graphics baseline contains only approved numerical metrics. Never update it
automatically to turn a failed test into a passing one; visual approval and an
explicit decision are required first.
