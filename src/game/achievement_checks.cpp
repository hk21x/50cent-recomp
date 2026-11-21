// The achievements no single memory condition shows (src/common/achievement_set.cpp lists them with "0=1"):
//
// - the story's end: the flow's PlayCreditsMovie (Lua 0x26400) after Vessel's last level (m9_3, level number 93 at
//   0x32DD94), with the difficulty the story was played on: the float the game sets from its table at 0x2FE010 into
//   the profile's +0x3DC (0x323618): Playa 0.05, Hustler 0.2, Thug 0.375, Gangsta 0.5, G-Unit Soldier 0.8.
// - what the player owns: the unlocks are one byte an item, the profile's at 0x323498 (copied in at checkpoints,
//   0x34B60 through 0x811B0) and those since then at 0x32DDB8; an item is owned when either is set. Their numbers
//   (0x810A0): disarms from 290 (14 normal and 11 Big Rig), weapons from 350 (20), Grizz medals from 370 (10).
//   Counted each gameplay frame; like a memory condition (core/achievements.cpp), one is awarded only once its count
//   has been seen short of its target, so a save that already has it awards nothing.
#include <cstring>

#include "core/achievements.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

#include "common/bulletproof.h"

namespace fc {
namespace {

constexpr uint32_t kLevel = 0x32DD94;         // the level loaded: mission * 10 + part
constexpr uint32_t kVesselLast = 93;          // m9_3
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
    if (kernel::R32(fc::kLevel) == fc::kVesselLast) {
        const float d = fc::Difficulty();
        LOGR("bulletproof: the story finished (difficulty %.3f)", d);
        achievements::Award("vessel");
        if (d >= 0.5f - 1e-3f) achievements::Award("finish_gangsta");
        if (d >= 0.8f - 1e-3f) achievements::Award("finish_g_unit");
    }
    __imp__sub_00026400(ctx, base);
}
