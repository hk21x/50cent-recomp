// CRI Sofdec's server against its own "server off" switch (crash in 0x153280 reading [0 + 0x48]).
//
// The player's state (0x14A900 returns 0x49D6A0) has eight handles from +0x6C, 0x2D4 each; a handle's +0x3C is its SFD
// handle. To stop or reset a movie, the movie thread sets the state's +0x170C (0x1554A0(1), from 0x14B190 and
// 0x14A4F0), zeroes +0x3C while it stops the SFD (0x14AE20, 0x149A10), puts it back, and clears +0x170C again. The
// server 0x1556F0 tests +0x170C once, then serves every handle (0x1556A0 -> 0x1555D0 reads +0x3C and 0x153280 its
// +0x48). On the console the server's thread outranks the movie thread on one core, so the test and the serving
// never straddle the switch. Here the threads run side by side: the server passed the test, the movie thread zeroed
// +0x3C, and the server read through 0 (3 crashes in 6 headless runs with WAV output, gdb watchpoints on 0x49D748).
//
// The stop 0x14AE20 has a third caller that does not throw the switch: 0x14B140 (from the game's 0xF8D00). Its only
// guard is the handle's "serving" flag (+0x60), which 0x1555D0 sets only after it has read +0x3C: the same race
// (1 crash in 12 runs with the switch alone). So the stop throws the switch itself when its caller has not.
//
// So setting the switch waits for a server pass already under way to finish; a pass that starts after it sees it.
// Both sides fence (the store buffer case: each side stores, then reads the other's). The wait gives up after 100 ms
// rather than risk a deadlock, should a pass ever wait on the movie thread.
//
// Frames out of order (cutscenes "flickered" at camera cuts): the game takes the current frame (0xF8C40 ->
// 0x14B960, mwPlyGetCurFrm, into the movie's +0x38), releases it (0x14B3E0), and the cutscene player 0x105F50 then
// converts it into its texture (0x114CB0 -> 0x14C500). A server pass decoding meanwhile changed what that frame showed:
// of M001.sfd's first 300 frames written, 29 and 35 (two headless runs) were out of display order against ffmpeg's
// decode of the same file, mostly a P frame three early (486, 490, 488, 489). At a camera cut that is a frame of the
// next shot, then two of the last again. So a server pass and the game's take-to-conversion exclude each other (the
// same 100 ms way out as above): 0 of 600. Locking the take alone, without the conversion after it, did not help.

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
std::atomic<int> g_serving{0};  // server passes under way (0x1556F0 has two callers, 0x1557D0 and 0x1557F0)
thread_local int t_serving = 0;  // this thread's own, which a stop from inside a pass must not wait for
std::recursive_timed_mutex g_frame;  // a server pass, or the game taking a frame

// held for a scope unless 100 ms go by (then the two run side by side, as before)
struct FrameLock {
    bool held = g_frame.try_lock_for(std::chrono::milliseconds(100));
    FrameLock() {
        if (!held) LOG_FIRST(8, "movie: the frame lock waited 100 ms, going on without it");
    }
    ~FrameLock() {
        if (held) g_frame.unlock();
    }
};
}

X86_EXTERN_FUNC(__imp__sub_001556F0);
X86_FUNC(sub_001556F0) {
    g_serving.fetch_add(1, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    t_serving++;
    {
        FrameLock lock;
        __imp__sub_001556F0(ctx, base);
    }
    t_serving--;
    g_serving.fetch_sub(1, std::memory_order_seq_cst);
}

namespace {
constexpr uint32_t kServerOff = 0x49D6A0 + 0x170C;  // the state's switch

// once the switch is on: a pass already under way finishes first
void WaitForServer() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const auto give_up = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    while (g_serving.load(std::memory_order_seq_cst) > t_serving) {
        if (std::chrono::steady_clock::now() > give_up) {
            LOG_FIRST(8, "movie: server pass still running after 100 ms, switching it off anyway");
            break;
        }
        std::this_thread::yield();
    }
}
}  // namespace

// (on): the state's +0x170C
X86_EXTERN_FUNC(__imp__sub_001554A0);
X86_FUNC(sub_001554A0) {
    const uint32_t on = kernel::R32(ctx.esp + 4);
    __imp__sub_001554A0(ctx, base);
    if (on == 1) WaitForServer();
}

// (handle): stops its SFD, zeroing +0x3C meanwhile; under the switch whoever calls it
X86_EXTERN_FUNC(__imp__sub_0014AE20);
X86_FUNC(sub_0014AE20) {
    const bool was_off = kernel::R32(kServerOff) == 1;
    if (!was_off) {
        kernel::W32(kServerOff, 1);
        WaitForServer();
    }
    __imp__sub_0014AE20(ctx, base);
    if (!was_off) {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        kernel::W32(kServerOff, 0);
    }
}

// (the movie): takes the current frame into its +0x38 and releases it; converts it first when its +0x5E4 says how.
// The cutscene player 0x105F50 converts it after (0x114CB0 with the frame at +0x38, released by then), so the lock
// is kept from here until that conversion, or the movie's next frame on this thread (not when converted already)
namespace {
thread_local bool t_frame_held = false;
}
X86_EXTERN_FUNC(__imp__sub_000F8C40);
X86_FUNC(sub_000F8C40) {
    if (t_frame_held) g_frame.unlock(), t_frame_held = false;
    const uint32_t movie = kernel::R32(ctx.esp + 4);
    FrameLock lock;
    __imp__sub_000F8C40(ctx, base);
    if (lock.held && movie && kernel::R32(movie + 0x38) && !kernel::R32(movie + 0x5E4)) {
        lock.held = false;  // released after the conversion
        t_frame_held = true;
    }
}

// (texture slot, frame): the frame converted into the texture's surface
X86_EXTERN_FUNC(__imp__sub_00114CB0);
X86_FUNC(sub_00114CB0) {
    FrameLock lock;
    __imp__sub_00114CB0(ctx, base);
    if (t_frame_held) g_frame.unlock(), t_frame_held = false;
}
