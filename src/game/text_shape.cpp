// The letters' width on any frame. A text node (0x53680 one line, 0x55140 a box of lines) projects its place to the
// target's pixels (0x121AC0), lays its letters out in pixels there, and hands them and the pictures in the line over as records
// (0x123100: x at +0, the width at +0x20, 0x30 bytes each). So a letter is as wide as its pixels: on a
// 4:3 console 1/640 of the screen's width each, square, but here always 1/720 (the widescreen mode, aspect_live)
// of whatever shape the frame has: 8/9 as wide at 4:3, too wide at 16:9 and more so beyond. With the 4:3 cameras
// (src/game/aspect.cpp) the node is drawn as on a target of square pixels, 480 high and the frame's shape (640x480 at
// 4:3, the console's): its place comes in those pixels, it wraps and clips in them, and what it draws is scaled back
// to the 720 on the way out.
#include "app/app.h"
#include "common/bulletproof.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kWidth = 0x349460, kHeight = 0x349464;  // the display mode's
float g_scale = 1;  // the target's pixels in one of the node's, while a text node draws

float Scale() {
    if (fc::WideCameras()) return 1;
    const uint32_t w = kernel::R32(kWidth), h = kernel::R32(kHeight);
    const float aspect = app::FrameAspect();
    return w && h && aspect > 0 ? float(w) / (float(h) * aspect) : 1;
}

template <class F>
void WithScale(F draw) {
    const float was = g_scale;
    g_scale = Scale();
    draw();
    g_scale = was;
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00053680);
X86_FUNC(sub_00053680) {
    WithScale([&] { __imp__sub_00053680(ctx, base); });
}
X86_EXTERN_FUNC(__imp__sub_00055140);
X86_FUNC(sub_00055140) {
    WithScale([&] { __imp__sub_00055140(ctx, base); });
}

// (view; point, out): a point's place in pixels
X86_EXTERN_FUNC(__imp__sub_00121AC0);
X86_FUNC(sub_00121AC0) {
    const uint32_t out = kernel::R32(ctx.esp + 8);
    __imp__sub_00121AC0(ctx, base);
    if (g_scale != 1) kernel::G<float>(out) /= g_scale;
}

// (view; left, top, right, bottom): the clip's edges read (pointers) and set
X86_EXTERN_FUNC(__imp__sub_00107210);
X86_FUNC(sub_00107210) {
    const uint32_t left = kernel::R32(ctx.esp + 4), right = kernel::R32(ctx.esp + 12);
    __imp__sub_00107210(ctx, base);
    if (g_scale != 1) {
        kernel::G<float>(left) /= g_scale;
        kernel::G<float>(right) /= g_scale;
    }
}
X86_EXTERN_FUNC(__imp__sub_001071C0);
X86_FUNC(sub_001071C0) {
    if (g_scale != 1) {
        kernel::G<float>(ctx.esp + 4) *= g_scale;
        kernel::G<float>(ctx.esp + 12) *= g_scale;
    }
    __imp__sub_001071C0(ctx, base);
}

// (records, count, ..): letters and pictures drawn
X86_EXTERN_FUNC(__imp__sub_00123100);
X86_FUNC(sub_00123100) {
    if (g_scale != 1) {
        const uint32_t records = kernel::R32(ctx.esp + 4), count = kernel::R32(ctx.esp + 8);
        for (uint32_t i = 0; i < count; i++) {
            kernel::G<float>(records + i * 0x30) *= g_scale;
            kernel::G<float>(records + i * 0x30 + 0x20) *= g_scale;
        }
    }
    __imp__sub_00123100(ctx, base);
}
