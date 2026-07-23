// Historical phase-4 recursive diagnostic hooks.
#include "generated/default/narutobb_init.h"
#include <rex/logging.h>
#include <cstring>
#include <string>

#define TRACE_WRAP(name)                                              \
  REX_EXTERN(__imp__##name);                                          \
  extern "C" __attribute__((noinline)) REX_FUNC(name) {               \
    uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32, a5 = ctx.r5.u32;       \
    REXLOG_ERROR("TRC> " #name " r3={:08X} r4={:08X} r5={:08X}",      \
                 a3, a4, a5);                                         \
    __imp__##name(ctx, base);                                         \
    REXLOG_ERROR("TRC< " #name " ret={:08X}", ctx.r3.u32);            \
  }

// Embedded DX9 shader compiler wrapper.
REX_EXTERN(__imp__sub_823C9D30);
extern "C" __attribute__((noinline)) REX_FUNC(sub_823C9D30) {
  uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32;
  std::string preview;
  if (r3 && r3 >= 0x82000000 && r3 < 0x83430000) {
    const char* p = reinterpret_cast<const char*>(base + r3);
    preview.assign(p, strnlen(p, 64));
  }
  REXLOG_ERROR("SHADERC in r3={:08X} r4={:08X} src='{}'", r3, r4, preview);
  __imp__sub_823C9D30(ctx, base);
  REXLOG_ERROR("SHADERC out r3={:08X}", ctx.r3.u32);
}

// Internal compiler call tree.
TRACE_WRAP(sub_823C9AC8)
TRACE_WRAP(sub_823C9A50)
TRACE_WRAP(sub_823C9580)
// Children of sub_823C9580.
TRACE_WRAP(sub_8222E170)
TRACE_WRAP(sub_8222F580)
TRACE_WRAP(sub_823C9260)
TRACE_WRAP(sub_823E5618)
TRACE_WRAP(sub_8248C388)
TRACE_WRAP(sub_824BC7E8)

// Level 2: children of sub_823C9260.
TRACE_WRAP(sub_82195FB0)
TRACE_WRAP(sub_82248F48)
TRACE_WRAP(sub_823C8F48)
TRACE_WRAP(sub_823C8FF0)
TRACE_WRAP(sub_823E16B8)
TRACE_WRAP(sub_823E1770)
TRACE_WRAP(sub_823E2858)
TRACE_WRAP(sub_823E5650)
TRACE_WRAP(sub_823ED890)
TRACE_WRAP(sub_823EF7D0)
TRACE_WRAP(sub_824037E8)
TRACE_WRAP(sub_824B4DD8)
TRACE_WRAP(sub_824B78E8)
TRACE_WRAP(sub_824B7930)
TRACE_WRAP(sub_824B8378)
TRACE_WRAP(sub_824B84F8)
TRACE_WRAP(sub_824B8500)

// Level 3: sub_82402878 and its children.
TRACE_WRAP(sub_82402878)
TRACE_WRAP(sub_821F61F8)
TRACE_WRAP(sub_821F6290)
TRACE_WRAP(sub_8222BEA0)
TRACE_WRAP(sub_8222C008)
TRACE_WRAP(sub_823C90D8)
TRACE_WRAP(sub_823C9D28)
TRACE_WRAP(sub_823E6090)
TRACE_WRAP(sub_823E84B8)
TRACE_WRAP(sub_823ED810)
TRACE_WRAP(sub_823ED850)
TRACE_WRAP(sub_823EF1D0)
TRACE_WRAP(sub_823EF320)
TRACE_WRAP(sub_823EF478)
TRACE_WRAP(sub_823EF848)
TRACE_WRAP(sub_823EFC78)
TRACE_WRAP(sub_823EFD08)
TRACE_WRAP(sub_823FF7C0)
TRACE_WRAP(sub_82400540)
