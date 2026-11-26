// A texture copied from one the CPU made is copied before the game writes the next picture there.
//
// The game copies one of its textures into another in one place (0x114A30, (from, to) as indices into its texture
// table; the pager's pictures in the menus): CopyRects (0x24D540) from the source's surface, then it releases both.
// On the console CopyRects reads the source's memory when the GPU runs it, later than the call: the game's own pictures
// are ready by then. The runtime's threaded renderer runs it later still, after the game may already have written the
// next picture into the source from the CPU, so the pager's background showed the wrong picture (flat, or another
// one's); with the renderer on the game's thread (XBE_RENDER_THREAD=0) it was right, and xemu draws it right.
//
// So after the copy the game waits for the GPU as the XDK lets a title do (D3D_KickOffAndWaitForIdle, 0x251AA0): the
// copy has run before the game goes on. It copies seldom: 4 times in a session to a level's play (at boot and as
// the level loads), none a frame.
#include <cstdint>

#include "kernel/kernel.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_00114A30);
X86_FUNC(sub_00114A30) {
    __imp__sub_00114A30(ctx, base);
    const uint32_t eax = ctx.eax;
    kernel::CallGuest(ctx, 0x251AA0, {});  // D3D_KickOffAndWaitForIdle
    ctx.eax = eax;
}
