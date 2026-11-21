// Hide the HUD (setting hide_hud, F9 in play): the HUD's layer left undrawn.
//
// The HUD is a layer of the game's layout ("HUDLayer", looked up by 0x4F350 from the HUD's layer set 0x3311A0 and
// kept at 0x32FA30): its groups (life, cash, ammo, the weapon, the pager's messages, the enemies' health, the timers).
// A layer is drawn by 0x29FA0 (ecx the layer: each of its top groups, +4 and +8, drawn by its vtable's +0xC); the
// HUD's draw (0x4B190) calls it for that one. The game's own state is not touched: its groups' showing and fading
// carry on, and the menus, the pause menu and the cutscenes' layers draw.
#include "common/bulletproof.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kHudLayer = 0x32FA30;
}

// (ecx the layer): its top groups drawn
X86_EXTERN_FUNC(__imp__sub_00029FA0);
X86_FUNC(sub_00029FA0) {
    if (fc::hide_hud && ctx.ecx && ctx.ecx == kernel::R32(kHudLayer)) {
        ctx.esp += 4;  // ret: no arguments on the stack
        return;
    }
    __imp__sub_00029FA0(ctx, base);
}
