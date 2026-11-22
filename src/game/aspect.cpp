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
constexpr uint32_t kAspect = 0x349478, kWorldView = 0x334CF0;

// (the view being set up) While a menu is up, every view but the world's keeps the game's own 16:9: the menus' text
// is laid out through their views, and at the frame's shape some of it came out that much wider (2.389 / 1.778 at
// 21:9) about its anchor: the arcade mode's weapon names ran off the left of their panel, the mission select's
// scores off its right, the Story or Arcade question past its box (Harry, 3440x1440, 2026-11-22). The menus are kept
// 4:3 in the middle anyway (GameInfo::hud_aspect); the world behind the pause menu keeps the frame's shape.
void SetAspect(uint32_t view) {
    const float frame = app::FrameAspect();
    const float a = fc::MenuUp() && view != kWorldView ? 16.0f / 9.0f : frame;
    static bool said_menu = false;
    if (a != frame && !said_menu) {
        said_menu = true;
        LOGR("bulletproof: the menus' views kept at 16:9");
    }
    uint32_t v;
    std::memcpy(&v, &a, 4);
    kernel::W32(kAspect, v);
    static float said = 0;
    if (frame != said) {
        said = frame;
        LOGR("bulletproof: the 3D views at %.3f:1", frame);
    }
}
}  // namespace

// Field of view (setting fov_percent, 50 to 150): the world's view, 0x334CF0 (the gameplay frame 0x86890 sets it up
// for its camera 0x334E20; the HUD's and menus' views are others), has its vertical field +0xD8 (radians, 0.585 from
// the constructor 0x121F10, which no one changes after) widened while it is set up, by either setup (0x1221E0, and
// 0x122020, which the frame's world takes): the projection (0x11EBF0, half the angle: D3DX's PerspectiveFovLH) and
// the culling planes (0x121D00) both read it. Headless at 130%: its projection's x scale 1.437 for 1.868. The tangent of the half angle is
// scaled, so the picture is that much wider and taller; the scopes' zoom divides it as before.
constexpr uint32_t kViewFov = 0xD8;

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
    SetAspect(ctx.ecx);
    WithFov(ctx, [&] { __imp__sub_001221E0(ctx, base); });
}

// (view; camera): the same, with the camera's fog and clear colour (0x107199 and its neighbours)
X86_EXTERN_FUNC(__imp__sub_00122020);
X86_FUNC(sub_00122020) {
    SetAspect(ctx.ecx);
    WithFov(ctx, [&] { __imp__sub_00122020(ctx, base); });
}
