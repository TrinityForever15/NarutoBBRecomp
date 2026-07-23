# Phase 4 — root-cause diagnosis of the early boot hang

Date: 2026-07-19.

## Vblank hypothesis rejected

The guest callback at `0x8219FCA8` was being dispatched at approximately
60 Hz. Register-read instrumentation observed the guest ISR reading interrupt
status on every vsync. The scheduler warnings were a side effect of successful
callback dispatch in the constrained sandbox, not proof of a missing callback.

## Bug 1: suspended-thread lost wakeup

`PosixCondition<Thread>::ThreadStartRoutine` published a suspended state and
notified the creator before setting `suspend_count_ = 1`. If the creator called
`Resume()` in that window, it observed a count of zero and returned. The new
thread then set the count and slept forever.

The failure was timing-sensitive. In affected boots the GPU Commands thread did
not start, and shader-storage initialization waited forever on a fence.

The fix publishes the initial suspend count in the same critical section as the
state transition. It benefits all ReXGlue titles and is patch 1 in the portable
series.

## Bug 2: embedded DX9 shader compiler returns `E_FAIL`

After thread startup became deterministic, the guest main thread repeatedly
faulted while dereferencing a null compiler result in `sub_8217AB20`.

The complete chain was:

1. `sub_8217AB20` requests compilation of a small bootstrap HLSL shader;
2. its wrapper reaches the embedded Xbox/DX9 shader compiler;
3. the compiler returns `0x8000FFFF` (`E_FAIL`);
4. the out object remains null;
5. the guest dereferences it and the runtime repeatedly resumes the same fault.

Recursive hooks showed the failure was created inside the compiler's parser and
code generator, not by a simple missing kernel dependency. Repairing the
embedded compiler would require reverse-engineering a large virtual-dispatch
parser.

The identifying debug path was `ShaderDumpxe:\CompareBackEnds`. This code is a
Fox/Jade diagnostic tool for comparing shader backends, not the production
renderer. The correct project-specific solution is a narrow hook that skips
`sub_8217AB20`, initializes the small object state expected by its caller, and
returns successfully.

## Reusable diagnostic techniques

- Run the game as a child of GDB when host ptrace policy blocks attach.
- Convert host fault addresses back to the mapped guest range before searching
  generated code.
- Override weak generated guest functions with narrow hooks in `src/hooks.cpp`.
- Do not regenerate all guest C++ when changing only hooks or runtime code.
- Treat malformed engine debug paths separately from required game-data paths.

The follow-up result is in `PHASE4_BOOT_REPORT.md`.
