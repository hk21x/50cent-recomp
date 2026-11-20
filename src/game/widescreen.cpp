// Cutscenes at 4:3 and the HUD at its own shape on any frame.
//
// Cutscenes: the cutscene player 0x105F50 (CRI Sofdec movies, cs\*.sfd and the boot's) draws each frame as one screen
// quad from its 512x512 LIN_A8R8G8B8 texture (format byte 0x12), x 0 to 720 and y -64 to 536: the 4:3 picture
// enlarged to fill a widescreen TV's width, its top and bottom cut off. While it plays, such a quad is fitted whole
// into the frame's height and squeezed to 4:3 about the centre, with bars at the sides on a wider frame
// (gpu::State::pillar_fit), whatever the aspect setting. The movies the game plays into textures (0x20520, 256x256,
// on screens in the world) do not span the frame and are left alone. The black bars the player draws over the top and
// bottom 64 lines when the cutscene's +0x24 is set (0x10641E, through 0x121B70) would then cover the fitted picture,
// so that flag is cleared while it plays and put back after.
//
// The HUD: the game draws it through a flat camera of the views' aspect (src/game/aspect.cpp), each piece by a vertex
// program's 2D matrix, depth testing off, at a fixed place laid out for its widescreen mode's 16:9: on a 4:3 frame the
// pieces at the sides ran off its edges, on a wider one they huddled in a 16:9 part of the middle. While the gameplay
// frame 0x86890 runs, each piece to one side is moved sideways to keep its distance from that edge as at 16:9
// (gpu::State::hud_layout); its shape is the camera's, right at any aspect, and one about the middle stays there. The
// menus are 2D drawn for a 4:3 screen, kept at that shape in the middle (GameInfo::hud_aspect).
#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/d3d8.h"
#include "xdk/hle.h"

namespace {
void MoviePillar(bool on) {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    gpu::State& s = d3d::State();
    s.pillar_aspect = on ? 4.0f / 3.0f : 0.0f;
    s.pillar_format = on ? 0x12 : 0;  // D3DFMT_LIN_A8R8G8B8
    s.pillar_fit = on;
}

void HudLayout(bool on) {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    d3d::State().hud_layout = on ? 16.0f / 9.0f : 0.0f;
}
}  // namespace

// (cutscene, its view or 0, debug): plays a movie to its end or a skip
X86_EXTERN_FUNC(__imp__sub_00105F50);
X86_FUNC(sub_00105F50) {
    const uint32_t cutscene = kernel::R32(ctx.esp + 4);
    const uint8_t bars = cutscene ? kernel::G<uint8_t>(cutscene + 0x24) : 0;
    if (cutscene) kernel::G<uint8_t>(cutscene + 0x24) = 0;
    MoviePillar(true);
    static bool said = false;
    if (!said) said = true, LOGR("bulletproof: cutscenes shown whole at 4:3");
    __imp__sub_00105F50(ctx, base);
    MoviePillar(false);
    if (cutscene) kernel::G<uint8_t>(cutscene + 0x24) = bars;
}

// the gameplay frame: the world, then the HUD over it
X86_EXTERN_FUNC(__imp__sub_00086890);
X86_FUNC(sub_00086890) {
    HudLayout(true);
    __imp__sub_00086890(ctx, base);
    HudLayout(false);
}
