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
#include <atomic>
#include <chrono>
#include <thread>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
std::atomic<int> g_serving{0};  // server passes under way (0x1556F0 has two callers, 0x1557D0 and 0x1557F0)
thread_local int t_serving = 0;  // this thread's own, which a stop from inside a pass must not wait for
}

X86_EXTERN_FUNC(__imp__sub_001556F0);
X86_FUNC(sub_001556F0) {
    g_serving.fetch_add(1, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    t_serving++;
    __imp__sub_001556F0(ctx, base);
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
