# Native PC recompilation feasibility report

Date: 2026-07-19. Initial tools: XenonRecomp and XenonAnalyse.

## Result

The project was technically viable. The initial investigation successfully
parsed a user-owned XGD2 dump, analyzed the retail XEX, located compiler helper
functions, found 460 switch tables without analysis errors, and translated the
guest PowerPC code into approximately 197 MB of C++.

No original game executable, decrypted image, generated guest C++, or extracted
shader/media content belongs in the public repository.

## CPU translation

The initial XenonRecomp run exposed 35 unsupported VMX/scalar operations with
about 10,300 occurrences. The most frequent were `vslh`, `vsubshs`, `vsrah`,
`vspltish`, and `vaddsws`. The historical
`xenonrecomp_35_instructions.patch` added the missing cases and two helpers for
saturating float-to-u32 conversion and vector byte shifting.

After the patch:

- the translator reported zero unrecognized instructions;
- the affected generated units compiled with Clang;
- helper tests covered NaN, saturation, shift-zero, overflow, and rotation
  edge cases.

This path later became historical because ReXGlue's own code generator covered
the required instruction set directly.

## Shader translation

The title's local shader database contained 8,156 compiled Xenos shaders. The
initial XenosRecomp-to-DXC success rate was about 83%. Four fixes brought the
local validation rate to 100%:

1. Texture1D support with `SampleLevel` for vertex-stage fetches;
2. normalization of vertex fetch slots 16-31 to reflection slots 0-15;
3. `sN` descriptor fallbacks for convention-bound samplers;
4. generic sequential Vulkan locations and a shared-constant layout correction
   from `+256/c16` to `+320/c20`.

The translated shaders and original Ubisoft HLSL found in the database are
copyrighted game content and are intentionally excluded. Only the tool patches
and technical findings are public.

## Conclusion

The study proved that CPU translation and shader translation were not the main
blockers. The long-term work moved to the runtime: Xbox kernel behavior, GPU
render-target semantics, XMA audio, input, threading, guest-function discovery,
and title-specific timing.

The active project now uses ReXGlue rather than the original standalone
XenonRecomp/XenosRecomp output. See `../docs/STATUS.md` and `SKILL.md` for the
current state.
