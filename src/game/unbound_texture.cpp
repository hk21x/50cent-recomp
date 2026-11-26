// A texture stage that reads a texture it has none of passes the current colour on, as on the console.
//
// The material setup (0x115CD0, a material's four layers) gives the walls, floors and machines of the first mission's
// laundry a layer that blends toward the stage's texture by the texture factor (stage 1: COLOROP BLENDFACTORALPHA,
// ARG1 TEXTURE, ARG2 CURRENT) and binds no texture there (0x1145D0 with -1). The XDK's fixed-function combiner
// compiler (5849, its argument map 0x259290) marks an argument naming a stage's texture as missing when the stage has
// none (the device's textures, 0x25CD38), and 0x259716 then builds that stage as "current" (the first stage's: the
// diffuse colour), colour and alpha each on its own: the texture is never read. xemu draws the walls dark that way.
// The runtime's fixed-function shaders take an unbound texture as white instead, so the walls came out white (issue 1).
//
// So after the material setup a stage with no texture whose colour or alpha operation reads it is set to SELECTARG1
// of CURRENT in the deferred texture states (0x25B910, read whole at each draw), which is what the console builds; the
// same is done whenever the game binds a stage's texture (0x1145D0). The states written are put back first, when a
// texture is bound to that stage or the next material is set up, unless the game wrote them over in between.
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kTextureState = 0x25B910;  // D3D_DeferredTextureState, [4][32]
constexpr uint32_t kTextures = 0x25CD38;      // the device's bound textures, one a stage
// D3DTSS_*, D3DTOP_*, D3DTA_* (XDK numbering)
constexpr uint32_t kColorOp = 12, kColorArg0 = 13, kColorArg1 = 14, kColorArg2 = 15;
constexpr uint32_t kAlphaOp = 16, kAlphaArg0 = 17, kAlphaArg1 = 18, kAlphaArg2 = 19;
constexpr uint32_t kTopSelectArg1 = 2, kTaCurrent = 1, kTaTexture = 2;

uint32_t Tss(uint32_t stage, uint32_t state) { return kTextureState + (stage * 32 + state) * 4; }

// whether an operation reads the stage's texture: an argument it takes naming it, or a blend by the texture's alpha
// (BLENDTEXTUREALPHA 14, BLENDTEXTUREALPHAPM 16)
bool ReadsTexture(uint32_t op, uint32_t a1, uint32_t a2, uint32_t a0) {
    if (op <= 1) return false;  // DISABLE, or none
    if (op == 14 || op == 16) return true;
    auto tex = [](uint32_t a) { return (a & 0xF) == kTaTexture; };
    if (op == 2) return tex(a1);   // SELECTARG1
    if (op == 3) return tex(a2);   // SELECTARG2
    if (op == 23 || op == 24) return tex(a0) || tex(a1) || tex(a2);  // MULTIPLYADD, LERP
    return tex(a1) || tex(a2);
}

// a stage's operation set to pass "current" on: what it was, to put back
struct Saved {
    bool on = false;
    uint32_t op = 0, arg1 = 0;
};
Saved g_saved[4][2];  // stage; colour, alpha

void PassOn(uint32_t stage, int which, uint32_t op_state, uint32_t arg1_state) {
    Saved& s = g_saved[stage][which];
    s = {true, kernel::R32(Tss(stage, op_state)), kernel::R32(Tss(stage, arg1_state))};
    kernel::W32(Tss(stage, op_state), kTopSelectArg1);
    kernel::W32(Tss(stage, arg1_state), kTaCurrent);
}

void Restore(uint32_t stage) {
    static constexpr uint32_t kOp[2] = {kColorOp, kAlphaOp}, kArg1[2] = {kColorArg1, kAlphaArg1};
    for (int w = 0; w < 2; w++) {
        Saved& s = g_saved[stage][w];
        if (!s.on) continue;
        s.on = false;
        // only if the game has not set the stage itself since
        if (kernel::R32(Tss(stage, kOp[w])) != kTopSelectArg1 || kernel::R32(Tss(stage, kArg1[w])) != kTaCurrent) continue;
        kernel::W32(Tss(stage, kOp[w]), s.op);
        kernel::W32(Tss(stage, kArg1[w]), s.arg1);
    }
}

// the console's rule on a stage as it stands
void Apply(uint32_t st) {
    if (kernel::R32(kTextures + st * 4)) return;
    auto r = [&](uint32_t state) { return kernel::R32(Tss(st, state)); };
    if (ReadsTexture(r(kColorOp), r(kColorArg1), r(kColorArg2), r(kColorArg0))) PassOn(st, 0, kColorOp, kColorArg1);
    if (ReadsTexture(r(kAlphaOp), r(kAlphaArg1), r(kAlphaArg2), r(kAlphaArg0))) PassOn(st, 1, kAlphaOp, kAlphaArg1);
}
}  // namespace

// the material setup: its layers' stages
X86_EXTERN_FUNC(__imp__sub_00115CD0);
X86_FUNC(sub_00115CD0) {
    for (uint32_t st = 0; st < 4; st++) Restore(st);
    __imp__sub_00115CD0(ctx, base);
    for (uint32_t st = 0; st < 4; st++) Apply(st);
}

// (texture, stage): a texture bound to a stage, or none (-1)
X86_EXTERN_FUNC(__imp__sub_001145D0);
X86_FUNC(sub_001145D0) {
    const uint32_t st = kernel::R32(ctx.esp + 8) & 3;
    Restore(st);
    __imp__sub_001145D0(ctx, base);
    Apply(st);
}
