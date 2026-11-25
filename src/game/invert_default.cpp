// The right stick's up looks up: Invert Y Axis (Options, Camera; the option byte +0xA, 0x32346E) starts off.
//
// The options' defaults (0x259D0, thiscall: the options, which page's; 0 all, 2 the camera's) are set at boot and by the
// Camera page's Default, with Invert Y Axis on: stick up looks down. Only that default is changed: the page switches it
// as before, and a profile keeps what it was saved with.
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kInvertY = 0xA;
}

X86_EXTERN_FUNC(__imp__sub_000259D0);
X86_FUNC(sub_000259D0) {
    const uint32_t options = ctx.ecx, page = kernel::R32(ctx.esp + 4);
    __imp__sub_000259D0(ctx, base);
    if (options && (page == 0 || page == 2)) kernel::G<uint8_t>(options + kInvertY) = 0;
}
