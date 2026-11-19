// The view's culling planes, widened past the frame's edges.
//
// A camera's view (0x1221E0 and 0x122020, thiscall on the view) takes the frame's aspect from 0x349478 (0x11AD80
// sets it with the display mode: 16:9 for a widescreen mode, else width over height) into +0xDC, then 0x121D00
// (thiscall: aspect, zoom) builds the four side planes at +0xF0..+0x12C through the eye and the far plane's corners,
// half width tan(+0xD8 * k) * far(+0xD4) * aspect * C / zoom, half height the same without the aspect; the projection
// (0x11EBF0) is built after from the same fields. So the culling matches the picture at either aspect.
//
// Harry saw things pop in at the edges of the 16:9 frame. The planes do cull (the alley's frame 8400 headless: 1701
// draws, 2078 with them twice as wide), and an object whose bounds are smaller than what it draws is cut at the edge
// it comes in by; on the console a TV's overscan hid the frame's edges. So the planes are built 1.25 times wider and
// taller (the zoom divided), the projection as it was: a few more draws a frame. FC_CULL_WIDEN=k sets another factor.
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
float GetF(uint32_t va) {
    float f;
    const uint32_t v = kernel::R32(va);
    std::memcpy(&f, &v, 4);
    return f;
}
void SetF(uint32_t va, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    kernel::W32(va, v);
}
float Widen() {
    static const float k = [] {
        const char* e = std::getenv("FC_CULL_WIDEN");
        const float f = e && *e ? float(std::atof(e)) : 1.25f;
        if (f != 1.25f) LOGR("bulletproof: the view's culling planes %.2f times wider and taller", f);
        return f;
    }();
    return k;
}
}  // namespace

// (view; aspect, zoom): the side planes
X86_EXTERN_FUNC(__imp__sub_00121D00);
X86_FUNC(sub_00121D00) {
    const float k = Widen();
    if (k != 1.0f) SetF(ctx.esp + 8, GetF(ctx.esp + 8) / k);  // the zoom divides both half extents
    __imp__sub_00121D00(ctx, base);
}
