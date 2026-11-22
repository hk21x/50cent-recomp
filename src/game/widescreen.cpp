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
// A movie played as a mission starts (from the hub, or a saved game's mission select) draws into a 512x512 target of
// its own and clears only that; the display buffers are drawn only by the fitted quad, so on a frame wider than 4:3
// whatever the screen before had at the sides stayed there for the whole movie: the loading text and its grey panel,
// the mission select's map (Harry, 3440x1440, 2026-11-22; on the GPU-VM the movie's clears were all to 82233B00,
// 512x512). On the console the movie covers the screen. Every display buffer is cleared black as a movie starts.
// The in-engine cutscenes are the game's 3D, run by its scripts with cinematicMode set (0xC7190 sets 0x32FAD8; the
// CinemaLayer's bars are drawn over the frame while it is, 0x4B190): while one plays the whole frame is held at 4:3
// (app::HoldAspect), its sides black as a movie's, and given back to the aspect setting after.
//
// The HUD: the game draws it through a flat camera of the views' aspect (src/game/aspect.cpp), each piece by a vertex
// program's 2D matrix, depth testing off, at a fixed place laid out for its widescreen mode's 16:9: on a 4:3 frame the
// pieces at the sides ran off its edges, on a wider one they huddled in a 16:9 part of the middle. While the gameplay
// frame 0x86890 runs with no menu up and the HUD's own layer draws (src/game/hud.cpp), each piece to one side is moved
// sideways to keep its distance from that edge as at 16:9 (gpu::State::hud_layout); its shape is the camera's, right
// at any aspect, and one about the middle stays there. Every other layer is left whole: the menus are 2D drawn for a
// 4:3 screen, kept at that shape in the middle (GameInfo::hud_aspect), and so are the shops, drawn in the gameplay
// frame with no menu up (the gun shop's stat bars, values and arrows, the bootlegger's list): moved piece by piece
// they came apart on a 21:9 frame (Harry, 3440x1440, 2026-11-22). The pause menu is drawn in the gameplay frame too,
// so nothing moves while one is up.
#include "app/app.h"
#include "common/bulletproof.h"
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

bool g_gameplay = false;  // in the gameplay frame

// XDK functions in the xbe (config/game.toml)
constexpr uint32_t kGetBackBuffer2 = 0x24D4F0, kSetRenderTarget = 0x24D270, kClear = 0x24F2C0;

// Every display buffer cleared black, the target put back (see the movie player below)
void ClearDisplayBuffers(X86Context& ctx) {
    uint32_t rt, zs, w, h;
    gpu::Viewport vp;
    {
        std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
        const gpu::State& s = d3d::State();
        rt = s.rt.header, zs = s.zs.header, vp = s.viewport;
        w = (s.rt.size & 0xFFF) + 1, h = (s.rt.size >> 12 & 0xFFF) + 1;
    }
    // putting the target back resets the viewport to all of it: only done when that is what it was
    if (!rt || vp.x || vp.y || vp.w != w || vp.h != h) return;
    uint32_t done[3] = {};
    for (int32_t i = -1; i < 2; i++) {
        const uint32_t surface = kernel::CallGuest(ctx, kGetBackBuffer2, {uint32_t(i)});
        if (!surface) continue;
        if (surface != done[0] && surface != done[1] && surface != done[2]) {
            done[i + 1] = surface;
            kernel::CallGuest(ctx, kSetRenderTarget, {surface, 0});
            kernel::CallGuest(ctx, kClear, {0, 0, 0xF0, 0xFFFF0000u, 0x3F800000u, 0});
        }
        kernel::W32(surface, kernel::R32(surface) - 1);  // GetBackBuffer2's reference
    }
    kernel::CallGuest(ctx, kSetRenderTarget, {rt, zs});
    LOGR_FIRST(1, "bulletproof: the display buffers cleared black before a movie");
}
}  // namespace

bool fc::HudLayoutWanted() { return g_gameplay && !fc::MenuUp(); }

// (cutscene, its view or 0, debug): plays a movie to its end or a skip
X86_EXTERN_FUNC(__imp__sub_00105F50);
X86_FUNC(sub_00105F50) {
    const uint32_t cutscene = kernel::R32(ctx.esp + 4);
    const uint8_t bars = cutscene ? kernel::G<uint8_t>(cutscene + 0x24) : 0;
    if (cutscene) kernel::G<uint8_t>(cutscene + 0x24) = 0;
    ClearDisplayBuffers(ctx);
    MoviePillar(true);
    static bool said = false;
    if (!said) said = true, LOGR("bulletproof: cutscenes shown whole at 4:3");
    __imp__sub_00105F50(ctx, base);
    MoviePillar(false);
    if (cutscene) kernel::G<uint8_t>(cutscene + 0x24) = bars;
}

// The game's clear (0x122350: colour, then whether the target, depth and stencil): a few frames at boot and as a
// mission is left are cleared dark grey (FF333333), the loading screen's, before the movie that plays meanwhile draws
// to another target. On a frame wider than the movie that grey stayed at its sides for the whole movie (GPU-VM,
// 1920x800, Quit from the pause menu); on the console nothing shows past the picture. Cleared black instead.
// A movie's clear (its every frame, black) is made to cover its whole target: a movie played as a saved game's mission
// starts draws to a new 720x480 target, and the picture of the mission select before it stayed at the movie's sides
// (Harry, 3440x1440, 2026-11-22), the clear covering the screen's viewport alone; the console's movie covers it all.
X86_EXTERN_FUNC(__imp__sub_00122350);
X86_FUNC(sub_00122350) {
    if (kernel::R32(ctx.esp + 4) == 0xFF333333u) kernel::W32(ctx.esp + 4, 0xFF000000u);
    // a movie's clear covers its whole target (see above)
    std::unique_lock<std::recursive_mutex> lock(d3d::Lock());
    gpu::State& s = d3d::State();
    const gpu::Viewport kept = s.viewport;
    const bool whole = s.pillar_fit && s.rt.size;
    if (whole) {
        const uint32_t w = (s.rt.size & 0xFFF) + 1, h = (s.rt.size >> 12 & 0xFFF) + 1;
        if (kept.x || kept.y || kept.w != w || kept.h != h)
            LOGR_FIRST(4, "bulletproof: a movie's clear widened from %u %u %ux%u to its whole %ux%u target", kept.x,
                       kept.y, kept.w, kept.h, w, h);
        s.viewport.x = s.viewport.y = 0;
        s.viewport.w = w;
        s.viewport.h = h;
    }
    lock.unlock();
    __imp__sub_00122350(ctx, base);
    if (whole) {
        lock.lock();
        s.viewport = kept;
    }
}

constexpr uint32_t kCinematicMode = 0x32FAD8;  // byte

// the gameplay frame: the world, then the HUD over it
X86_EXTERN_FUNC(__imp__sub_00086890);
X86_FUNC(sub_00086890) {
    // an in-engine cutscene (the scripts' cinematicMode, 0x32FAD8, set by 0xC7190): the whole frame at 4:3, its sides
    // black, as a movie is; the 3D follows (src/game/aspect.cpp) from its next view
    const bool cinematic = kernel::G<uint8_t>(kCinematicMode) != 0;
    static bool was = false;
    if (cinematic != was) {
        was = cinematic;
        app::HoldAspect(cinematic ? 4.0f / 3.0f : 0.0f);
        LOGR("bulletproof: in-engine cutscene %s", cinematic ? "begins: the frame at 4:3" : "ends");
    }
    g_gameplay = true;
    __imp__sub_00086890(ctx, base);
    g_gameplay = false;
    fc::AchievementFrame();  // the unlock counts (src/game/achievement_checks.cpp)
}
