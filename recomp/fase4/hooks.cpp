// Historical phase-4 hook: skip only the embedded debug shader compiler.
// (CompareBackEnds) que retorna E_FAIL e travava o boot.
#include "generated/default/narutobb_init.h"
#include <rex/logging.h>

REX_EXTERN(__imp__sub_8217AB20);
extern "C" __attribute__((noinline)) REX_FUNC(sub_8217AB20) {
  uint32_t self = ctx.r3.u32;
  static int once = 0;
  if (!once++) REXLOG_WARN("SKIP sub_8217AB20 (shader debug CompareBackEnds)");
  for (uint32_t off : {0u, 4u, 8u, 40u, 44u, 48u})
    *(volatile uint32_t*)(base + self + off) = 0;
  ctx.r3.u32 = self;
}
