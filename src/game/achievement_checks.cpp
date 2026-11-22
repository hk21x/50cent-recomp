// The achievements no single memory condition shows (src/common/achievement_set.cpp lists them with "0=1"):
//
// - the story's end: the flow's PlayCreditsMovie (Lua 0x26400), with the difficulty the story was played on: the float the game sets from its table at 0x2FE010 into
//   the profile's +0x3DC (0x323618): Playa 0.05, Hustler 0.2, Thug 0.375, Gangsta 0.5, G-Unit Soldier 0.8.
// - what the player owns: the unlocks are one byte an item, the profile's at 0x323498 (copied in at checkpoints,
//   0x34B60 through 0x811B0) and those since then at 0x32DDB8; an item is owned when either is set. Their numbers
//   (0x810A0): disarms from 290 (14 normal and 11 Big Rig), weapons from 350 (20), Grizz medals from 370 (10).
//   Counted each gameplay frame; like a memory condition (core/achievements.cpp), one is awarded only once its count
//   has been seen short of its target, so a save that already has it awards nothing.
//
// PlayCreditsMovie is called once in all the scripts: flow.lua's step 56 (GameFlow[56], after m9_3 at 54 and the
// S041 movie at 55), so it needs no level check. The level loaded is not m9_3 when the story continues from the
// autosave m9_3's end writes (the flow index the profile load copies 0x323460 -> 0x337070 at 0xD1590).
// FC_FLOW_START=N (a test) puts step N there after a profile loads, so Continue starts at it: 55 for the credits.
#include <cstdlib>
#include <cstring>

#include "core/achievements.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

#include "common/bulletproof.h"

namespace fc {
namespace {

constexpr uint32_t kFlowStart = 0x337070;     // the step Continue starts at (GameFlow, from 1)
constexpr uint32_t kProfileSlot = 0x342AF0;   // -1 with no profile loaded
constexpr uint32_t kDifficulty = 0x323618;    // float: the damage the player takes
constexpr uint32_t kOwnedSaved = 0x323498;    // the profile's unlocks, one byte an item
constexpr uint32_t kOwnedNew = 0x32DDB8;      // the unlocks since the last checkpoint

float Difficulty() {
    const uint32_t v = kernel::R32(kDifficulty);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

int Owned(uint32_t first, uint32_t count) {
    int n = 0;
    for (uint32_t i = first; i < first + count; i++)
        if (kernel::G<uint8_t>(kOwnedSaved + i) || kernel::G<uint8_t>(kOwnedNew + i)) n++;
    return n;
}

struct Count {
    const char* id;
    uint32_t first, count;  // the items counted
    int target;
    bool armed = false;  // seen short of the target since start
};
Count g_counts[] = {
    {"first_medal", 370, 10, 1},  {"all_medals", 370, 10, 10},   {"first_disarm", 290, 25, 1},
    {"ten_disarms", 290, 25, 10}, {"all_disarms", 290, 25, 25},  {"first_weapon", 350, 20, 1},
    {"all_weapons", 350, 20, 20},
};

}  // namespace

void AchievementFrame() {
    for (Count& c : g_counts) {
        const int n = Owned(c.first, c.count);
        if (n < c.target) {
            c.armed = true;
        } else if (c.armed) {
            c.armed = false;
            achievements::Award(c.id);
        }
    }
}

}  // namespace fc

// PlayCreditsMovie (Lua): the story's end, after Vessel
X86_EXTERN_FUNC(__imp__sub_00026400);
X86_FUNC(sub_00026400) {
    const float d = fc::Difficulty();
    LOGR("bulletproof: the story finished (difficulty %.3f)", d);
    achievements::Award("vessel");
    if (d >= 0.5f - 1e-3f) achievements::Award("finish_gangsta");
    if (d >= 0.8f - 1e-3f) achievements::Award("finish_g_unit");
    __imp__sub_00026400(ctx, base);
}

// the profile load (0xD1590): FC_FLOW_START, above
X86_EXTERN_FUNC(__imp__sub_000D1590);
X86_FUNC(sub_000D1590) {
    __imp__sub_000D1590(ctx, base);
    static const char* start = std::getenv("FC_FLOW_START");
    static bool done = false;
    if (!done && start && *start && kernel::R32(fc::kProfileSlot) == 0) {
        done = true;
        LOGR("bulletproof: FC_FLOW_START %s (the profile's %u)", start, kernel::R32(fc::kFlowStart));
        kernel::W32(fc::kFlowStart, uint32_t(std::atoi(start)));
    }
}
