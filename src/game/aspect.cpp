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
#include <algorithm>
#include <cmath>
#include <cstring>

#include "app/app.h"
#include "common/bulletproof.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kAspect = 0x349478;

void SetAspect() {
    fc::UpdateAspectHold();  // src/game/widescreen.cpp
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

// Field of view (setting fov_percent, 50 to 150): the world's view, 0x334CF0 (the gameplay frame 0x86890 sets it up
// for its camera 0x334E20; the HUD's and menus' views are others), has its vertical field +0xD8 (radians, 0.585 from
// the constructor 0x121F10, which no one changes after) widened while it is set up, by either setup (0x1221E0, and
// 0x122020, which the frame's world takes): the projection (0x11EBF0, half the angle: D3DX's PerspectiveFovLH) and
// the culling planes (0x121D00) both read it. Headless at 130%: its projection's x scale 1.437 for 1.868. The tangent of the half angle is
// scaled, so the picture is that much wider and taller; the scopes' zoom divides it as before.
constexpr uint32_t kWorldView = 0x334CF0, kViewFov = 0xD8;

float FovScale() {
    const uint32_t pct = std::clamp<uint32_t>(fc::fov_percent, 50, 150);
    static uint32_t said = 100;
    if (pct != said) {
        said = pct;
        LOGR("bulletproof: field of view %u%% of the game's", pct);
    }
    return float(pct) / 100.0f;
}

// a view's setup with the world's field of view widened for it (put back after)
template <class F>
void WithFov(X86Context& ctx, F setup) {
    const uint32_t view = ctx.ecx;
    const float k = view == kWorldView ? FovScale() : 1.0f;
    if (k == 1.0f) {
        setup();
        return;
    }
    const uint32_t was = kernel::R32(view + kViewFov);
    float fov;
    std::memcpy(&fov, &was, 4);
    const float wide = 2.0f * std::atan(std::tan(fov / 2.0f) * k);
    uint32_t v;
    std::memcpy(&v, &wide, 4);
    kernel::W32(view + kViewFov, v);
    setup();
    kernel::W32(view + kViewFov, was);
}

// (view; camera): a view set up for a camera (the game's scenes)
X86_EXTERN_FUNC(__imp__sub_001221E0);
X86_FUNC(sub_001221E0) {
    SetAspect();
    WithFov(ctx, [&] { __imp__sub_001221E0(ctx, base); });
}

// (view; camera): the same, with the camera's fog and clear colour (0x107199 and its neighbours)
X86_EXTERN_FUNC(__imp__sub_00122020);
X86_FUNC(sub_00122020) {
    SetAspect();
    WithFov(ctx, [&] { __imp__sub_00122020(ctx, base); });
}

// The game's two sets of cameras. A view's projection is its aspect's (above) times 0.4 over its camera's width and 0.3
// over its height (+0x250 and +0x254; 0x1221E0, 0x122020). On a 4:3 console the world and the HUD both take 0.4 by 0.31;
// in its widescreen mode the world takes 0.45028 (0x86982 and 0x56B73, by the option byte 0x323488) and the HUD 0.3 by
// 0.3 (where 0x11AB00 says widescreen), whose 0.3 high stays for the world. With aspect_live the game is always in its
// widescreen mode, so a 4:3 frame was drawn with those cameras: thin by 0.45028/0.4 * 0.31/0.3 = 1.163 (the alley, xemu
// 0.8.139 set to 4:3: x 2.490557 y 3.213622; here x 2.212452 y 3.320743). A 4:3 frame is drawn with the 4:3 cameras.
// The aspect alone makes a wide frame wide, so the console's wider camera draws a 2.001:1 picture on a 16:9 screen, 12.6%
// thin (xemu set to widescreen: x 1.659339 y 3.320743, the same here with console_widescreen); without that setting a wide
// frame has the 4:3 cameras too, and shows more at the sides. The option byte and the display mode's flags are left
// alone: the movies and the options read them too.
bool fc::WideCameras() { return fc::console_widescreen && app::FrameAspect() > 4.0f / 3.0f + 1e-3f; }

// whether the display mode is widescreen; 0x17384 and 0x25A5F set the options by it
X86_EXTERN_FUNC(__imp__sub_0011AB00);
X86_FUNC(sub_0011AB00) {
    const uint32_t ret = kernel::R32(ctx.esp);
    __imp__sub_0011AB00(ctx, base);
    if (ret == 0x17389 || ret == 0x25A64 || fc::WideCameras()) return;
    ctx.eax &= ~0xFFu;
}

// (view; width): a camera's width set, its height kept
X86_EXTERN_FUNC(__imp__sub_001072C0);
X86_FUNC(sub_001072C0) {
    constexpr uint32_t kWide = 0x3EE68B1A, kNormal = 0x3ECCCCCD;  // 0.45028, 0.4
    constexpr uint32_t kWideHeight = 0x3E99999A, kHeight = 0x3E9EB852;  // 0.3, 0.31
    const uint32_t width = kernel::R32(ctx.esp + 4);
    if ((width == kWide || width == kNormal) && !fc::WideCameras()) {
        kernel::W32(ctx.esp + 4, kNormal);
        // the widescreen HUD's height, left by a frame before the setting changed
        if (kernel::R32(ctx.ecx + 0x254) == kWideHeight) kernel::W32(ctx.ecx + 0x254, kHeight);
    }
    __imp__sub_001072C0(ctx, base);
}
