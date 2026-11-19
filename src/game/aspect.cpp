// The aspect setting applied at once (GameInfo::aspect_live): 4:3, 16:9 or the screen's own shape, in play too.
//
// The game picks its display mode once, at boot (0x16FA8 tries 720x480 widescreen first, through 0x11AD80), and
// 0x11AD80 sets the views' aspect 0x349478: 16:9 for a widescreen mode, else width over height (720/480 = 1.5 in the
// 4:3 mode, not 4:3). Every view takes it again each time it is set up: 0x1221E0 and 0x122020 (thiscall: the view,
// its camera) copy it into the view's +0xDC, from which both the projection (0x11EBF0) and the side culling planes
// (0x121D00, src/game/view_cull.cpp) are built; nothing else reads it. With aspect_live the console always reports a
// widescreen TV, so the game stays in its 720x480 widescreen mode, and here the aspect is the frame's shape as the
// setting has it now (app::FrameAspect) each time a view is set up: a change shows on the next frame. The height
// of the view is the game's; a wider frame shows more at the sides.
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kAspect = 0x349478;

void SetAspect() {
    const float a = app::FrameAspect();
    uint32_t v;
    std::memcpy(&v, &a, 4);
    kernel::W32(kAspect, v);
    static float said = 0;
    if (a != said) {
        said = a;
        LOGR("bulletproof: the 3D views at %.3f:1", a);
    }
}
}  // namespace

// (view; camera): a view set up for a camera (the game's scenes)
X86_EXTERN_FUNC(__imp__sub_001221E0);
X86_FUNC(sub_001221E0) {
    SetAspect();
    __imp__sub_001221E0(ctx, base);
}

// (view; camera): the same, with the camera's fog and clear colour (0x107199 and its neighbours)
X86_EXTERN_FUNC(__imp__sub_00122020);
X86_FUNC(sub_00122020) {
    SetAspect();
    __imp__sub_00122020(ctx, base);
}
