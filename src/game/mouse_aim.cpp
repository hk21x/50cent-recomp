// The mouse turns the camera directly, as in a PC shooter, not through the right stick (GameInfo::mouse_direct).
//
// The third person camera's update 0x94000 (thiscall: the camera, its angles {pitch, yaw} in radians at 0x322FE4,
// the step) reads the right stick (channels 2 and 3 of the pad, 0x11F1F0), flips each by the game's invert options
// (0x32346D the x axis, 0x32346E the y), and turns the angles by velocities that ease in and out (+0x1C8 pitch, +0x1CC
// yaw, with acceleration curves and a cap while aiming); it then clamps pitch to the view's limits (different while
// crouched and aiming) and stores both. Yaw grows to the right (0.065 rad a step at full right), pitch downwards
// (-0.87 looks up at the sky, 1.05 down at the ground; captured headless). The y invert byte is set at a first boot,
// and with it stick up looks down, as a plane's; the mouse keeps the PC's way whatever the game's options (up looks up;
// the settings' mouse invert flips it). Its movement is added to the angles before the update, so the game's own
// clamp holds; the stick is left to the controller. Not called in cutscenes and menus, so the mouse does nothing there.
// A jump eases the pitch to a jump's and a landing back to the pitch before it (0x955C0, the camera's events: +0x264
// set, +0x258 the pitch eased to, +0x25C the step, +0x254 the pitch kept); the update stops easing once the right
// stick moves up or down, but the mouse is not the stick, so after a landing the easing pulled the view back against
// the mouse for seconds, as if inverted (Harry, 2026-11-23). The mouse moving up or down stops it as the stick does.
// Headless (no mouse): FC_MOUSE_TEST=dx,dy,from,to plays that many counts a call for calls from..to.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr float kRadiansPerCount = 0.0008f;  // 0.0458 degrees, as Agent Under Fire's: 36 degrees an inch at 800 counts an inch

float GetF(uint32_t va) {
    const uint32_t v = kernel::R32(va);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
void SetF(uint32_t va, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    kernel::W32(va, v);
}

// the mouse's counts since the last call, times the sensitivity, up positive
bool Counts(float& x, float& y) {
    static const struct Test {
        float dx = 0, dy = 0;
        uint32_t from = 0, to = 0;
    } test = [] {
        Test t;
        if (const char* e = std::getenv("FC_MOUSE_TEST")) std::sscanf(e, "%f,%f,%u,%u", &t.dx, &t.dy, &t.from, &t.to);
        return t;
    }();
    static uint32_t call = 0;
    call++;
    if (test.to) {
        const bool on = call >= test.from && call <= test.to;
        if (call == test.from || call == test.to + 1) LOGR("bulletproof: mouse test, call %u", call);
        x = on ? test.dx : 0, y = on ? test.dy : 0;
        return on;
    }
    using clock = std::chrono::steady_clock;
    static auto last = clock::now();
    const auto now = clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - last).count(), 0.001f, 0.1f);
    last = now;
    if (!input::TakeMouse(x, y)) return false;  // not looking: the window in the background, the settings open
    x *= 2400 * dt, y *= 2400 * dt;             // TakeMouse's rate (2400 counts a second at 100% is 1) back to counts
    return x != 0 || y != 0;
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00094000);
X86_FUNC(sub_00094000) {
    float x, y;
    const uint32_t angles = kernel::R32(ctx.esp + 4);
    if (angles && Counts(x, y)) {
        const uint32_t camera = ctx.ecx;
        if (y != 0 && camera && kernel::G<uint8_t>(camera + 0x264)) {
            kernel::G<uint8_t>(camera + 0x264) = 0;
            SetF(camera + 0x25C, 0);
            LOGR_FIRST(4, "bulletproof: the mouse stops the camera's pitch easing (a jump's or a landing's)");
        }
        SetF(angles, GetF(angles) - y * kRadiansPerCount);
        SetF(angles + 4, GetF(angles + 4) + x * kRadiansPerCount);
        static bool said = false;
        if (!said) {
            said = true;
            LOGR("bulletproof: the mouse turns the camera directly");
        }
    }
    __imp__sub_00094000(ctx, base);
    static const bool testing = std::getenv("FC_MOUSE_TEST") != nullptr;
    static uint32_t calls = 0;
    if (testing && angles && calls++ % 30 == 0) LOGR("bulletproof: camera pitch %.4f yaw %.4f", GetF(angles), GetF(angles + 4));
}
