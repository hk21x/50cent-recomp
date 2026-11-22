// Hide the HUD (setting hide_hud, F9 in play): the HUD's layer left undrawn.
//
// The HUD is a layer of the game's layout ("HUDLayer", looked up by 0x4F350 from the HUD's layer set 0x3311A0 and
// kept at 0x32FA30): its groups (life, cash, ammo, the weapon, the pager's messages, the enemies' health, the timers).
// A layer is drawn by 0x29FA0 (ecx the layer: each of its top groups, +4 and +8, drawn by its vtable's +0xC); the
// HUD's draw (0x4B190) calls it for that one. The game's own state is not touched: its groups' showing and fading
// carry on, and the menus, the pause menu and the cutscenes' layers draw.
//
// The fade to black (and from it): the fade layer ("fadeLayer", 0x336E30; its group "fadeGrp", 0x336E34, set up by
// 0x8E220) is one black quad laid out for the 4:3 / 16:9 screen, so on a wider frame it darkened only the middle while
// the world stayed lit at the sides (Harry: quitting a mission, 2026-11-22; on the GPU-VM from the pause menu's Quit
// at 1920x800). While that layer draws, the group's x scale (its matrix, +0x10, identity) is made 8 times wider about
// the middle, so the quad covers any frame's width, then put back.
#include "common/bulletproof.h"
#include <mutex>

#include "kernel/kernel.h"
#include "xdk/d3d8.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kHudLayer = 0x32FA30, kFadeLayer = 0x336E30, kFadeGroup = 0x336E34;

// Sharp text (GameInfo::sharpen_2d): the game draws its menus and HUD through cameras of its own, so the renderers do
// not see them as 2D; while a layer (or the name keyboard, 0x59330) draws, gpu::State::flat_2d says so, and their P8
// pictures are enlarged sharp with their palettes. Nested layers put back what they found.
bool Flat2d(bool on) {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    const bool was = d3d::State().flat_2d;
    d3d::State().flat_2d = on;
    return was;
}
}

// (ecx the layer): its top groups drawn
X86_EXTERN_FUNC(__imp__sub_00029FA0);
X86_FUNC(sub_00029FA0) {
    if (fc::hide_hud && ctx.ecx && ctx.ecx == kernel::R32(kHudLayer)) {
        ctx.esp += 4;  // ret: no arguments on the stack
        return;
    }
    const bool was = Flat2d(true);
    const uint32_t group = kernel::R32(kFadeGroup);
    if (ctx.ecx && group && ctx.ecx == kernel::R32(kFadeLayer)) {
        float& sx = kernel::G<float>(group + 0x10);
        const float kept = sx;
        sx = kept * 8;
        __imp__sub_00029FA0(ctx, base);
        sx = kept;
    } else {
        __imp__sub_00029FA0(ctx, base);
    }
    Flat2d(was);
}

// the name keyboard's keys (ecx the keyboard; one argument)
X86_EXTERN_FUNC(__imp__sub_00059330);
X86_FUNC(sub_00059330) {
    const bool was = Flat2d(true);
    __imp__sub_00059330(ctx, base);
    Flat2d(was);
}
