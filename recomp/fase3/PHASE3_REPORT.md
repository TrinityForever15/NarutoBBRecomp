# Phase 3 — first ReXGlue native boot

Date: 2026-07-19.

## Result

*The Broken Bond* reached its first Linux x86-64 native boot through ReXGlue
SDK v0.8.0. Recompiled PowerPC code executed far enough for the Fox/Jade engine
to initialize video and register the guest vsync interrupt callback at
`0x8219FCA8`.

## Working route

ReXGlue's CLI replaced the originally planned standalone integration:

1. initialize a native project from a locally supplied `default.xex`;
2. run ReXGlue code generation from `narutobb_manifest.toml`;
3. configure and build with CMake/Ninja and Clang.

Code generation produced 97 translation units in about 13 seconds in the
recorded environment. ReXGlue already covered the 35 operations that had
required a separate XenonRecomp patch.

## Scanner omissions

The scanner initially missed two indirect targets:

```toml
0x821FE040 = { end = 0x821FE060 }
0x822086E0 = { end = 0x822086F8 }
```

A temporary diagnostic changed invalid dispatch from fatal to a
`MISSING_GUEST_FUNC` log plus return so multiple targets could be collected in
one run. That mode was diagnostic only and was removed from the strict build.

## Runtime change

The title imports `XUsbcam*`, so the existing
`src/kernel/xboxkrnl/xboxkrnl_usbcam.cpp` implementation had to be enabled in
the SDK build.

## Historical Linux environment

The original rootless sandbox build used Clang 18+, C++23, locally extracted
libstdc++/GTK dependencies, Tracy disabled, Xvfb, and Lavapipe. These details
were necessary for that sandbox but are not the current Windows workflow.

## Handoff to phase 4

The runtime, VFS, XMA thread, Vulkan backend, SDL input, guest threads, and
shader storage initialized. The next phase supplied complete local game data,
diagnosed thread startup and the embedded debug shader compiler, and moved to a
native Windows build.
