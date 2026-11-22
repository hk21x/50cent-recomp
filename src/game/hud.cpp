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
//
// The loading screen ("loadScreenLayer", kept at 0x340F60 by 0xD36A0): the flow loads a mission while its opening
// movie plays (LoadMission, PlayMovie, LoadScreen), and the loading layer goes on drawing under the movie. On the
// console the movie covers the whole screen; here it is fitted at 4:3 in the middle of a wider frame, so the loading
// text's ends and its grey panel showed at the movie's sides (Harry, 3440x1440, a cutscene after the hub,
// 2026-11-22). Starting a mission from a saved game's mission select, the menu went on drawing under the movie the
// same way. While a movie is on screen no layer is drawn: on the console the movie covers them all.
//
// The mission failed screen (StartDeathDialogue, 0x5C660: the PDA's group "missionFail", its picture
// shell\wallp\fail.tex, Restart and Quit): a picture laid out for 16:9, drawn over the world and the HUD, which showed
// at its sides on a wider frame (Harry, 3440x1440, 2026-11-22). Once play is paused for it (0x1FDC0 sets 0x3297B0;
// the reset that hides missionFail clears it) the frame is cleared black, by the game's own clear (0x122350), just
// before the PDA's layer ([0x3297B8]) draws, so the picture has black sides as a menu does.
#include "common/bulletproof.h"
#include <mutex>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/d3d8.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kHudLayer = 0x32FA30, kFadeLayer = 0x336E30, kFadeGroup = 0x336E34, kLoadLayer = 0x340F60;
constexpr uint32_t kMenuLayer = 0x3297B8, kPaused = 0x3297B0;  // the PDA's layer; a byte, set while play is paused

// the mission failed screen (StartDeathDialogue, 0x5C660): shown, and play since paused for it
bool g_fail = false, g_fail_paused = false;

// the fail screen up, play paused for it (its picture whole); over once play goes on (Restart or Quit)
bool FailScreenUp() {
    if (!g_fail) return false;
    const bool paused = kernel::G<uint8_t>(kPaused) != 0;
    if (paused) g_fail_paused = true;
    else if (g_fail_paused) g_fail = g_fail_paused = false;
    return g_fail && paused;
}

// Sharp text (GameInfo::sharpen_2d): the game draws its menus and HUD through cameras of its own, so the renderers do
// not see them as 2D; while a layer (or the name keyboard, 0x59330) draws, gpu::State::flat_2d says so, and their P8
// pictures are enlarged sharp with their palettes. Nested layers put back what they found.
bool Flat2d(bool on) {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    const bool was = d3d::State().flat_2d;
    d3d::State().flat_2d = on;
    return was;
}

// a movie on screen: the cutscene player fits its picture (src/game/widescreen.cpp)
bool MoviePlaying() {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    return d3d::State().pillar_fit;
}

float HudLayout(float aspect) {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    const float was = d3d::State().hud_layout;
    d3d::State().hud_layout = aspect;
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
    // no layer under a movie (the loading screen, the menu the movie was started from): left undrawn
    if (ctx.ecx && MoviePlaying()) {
        LOGR_FIRST(8, "bulletproof: layer %08X left undrawn under a movie (loading %08X, HUD %08X)", ctx.ecx,
                   kernel::R32(kLoadLayer), kernel::R32(kHudLayer));
        ctx.esp += 4;
        return;
    }
    // the mission failed screen: the frame cleared black under the PDA's layer, so its picture (laid out for 16:9)
    // has black sides, not the world and the HUD
    if (ctx.ecx && ctx.ecx == kernel::R32(kMenuLayer) && FailScreenUp()) {
        LOGR_FIRST(1, "bulletproof: the mission failed screen drawn on black");
        kernel::CallGuest(ctx, 0x122350, {0xFF000000u, 1, 0, 0});
    }
    const bool was = Flat2d(true);
    // the HUD's own layer alone is laid out to the frame's edges (src/game/widescreen.cpp)
    const bool layout = ctx.ecx && ctx.ecx == kernel::R32(kHudLayer) && fc::HudLayoutWanted();
    const float kept_layout = HudLayout(layout ? 16.0f / 9.0f : 0.0f);
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
    HudLayout(kept_layout);
    Flat2d(was);
}

// the name keyboard's keys (ecx the keyboard; one argument)
X86_EXTERN_FUNC(__imp__sub_00059330);
X86_FUNC(sub_00059330) {
    const bool was = Flat2d(true);
    __imp__sub_00059330(ctx, base);
    Flat2d(was);
}

// StartDeathDialogue (Lua): the mission failed screen, its picture shell\wallp\fail.tex in the PDA's "missionFail"
X86_EXTERN_FUNC(__imp__sub_0005C660);
X86_FUNC(sub_0005C660) {
    g_fail = true;
    g_fail_paused = false;
    LOGR("bulletproof: the mission failed screen");
    __imp__sub_0005C660(ctx, base);
}
