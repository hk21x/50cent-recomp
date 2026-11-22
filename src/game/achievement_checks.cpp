// The achievements no single memory condition shows (src/common/achievement_set.cpp lists them with "0=1"):
//
// - the story's end: the flow's PlayCreditsMovie (Lua 0x26400), with the difficulty the story was played on: the float the game sets from its table at 0x2FE010 into
//   the profile's +0x3DC (0x323618): Playa 0.05, Hustler 0.2, Thug 0.375, Gangsta 0.5, G-Unit Soldier 0.8.
// - what the player owns: the unlocks are one byte an item, the profile's at 0x323498 (copied in at checkpoints,
//   0x34B60 through 0x811B0) and those since then at 0x32DDB8; an item is owned when either is set. Their numbers
//   (0x810A0): disarms from 290 (14 normal and 11 Big Rig), Grizz medals from 370 (10). The disarm shop (0xE5AB0)
//   sets one after the price is paid (0x561F0); a medal pickup (0xC50B0) sets 370 + the mission.
//   Counted each gameplay frame; like a memory condition (core/achievements.cpp), one is awarded only once its count
//   has been seen short of its target, so a save that already has it awards nothing.
// - weapons: items 350.. are not owned but offered: the hub (0x84080) sets them from inventory.lua's WeaponsUnlocked
//   as the story reaches each mission. The gun shop (0xE3590) pays, then 0x31D70(weapon) adds one to the stash's
//   count of it, 20 shorts at 0x3254FC (0x325320 + 0x1DC; restored with a save by 0x27C30), which the hub also fills
//   with the weapons carried in (0x31E30). Armed Up: that call from the shop (returning to 0xE38DD). Full Arsenal:
//   all 20 owned at once, in the stash or in a holster (the player's +0x5BC, six of 0x1C bytes, the weapon at +0).
//
// PlayCreditsMovie is called once in all the scripts: flow.lua's step 56 (GameFlow[56], after m9_3 at 54 and the
// S041 movie at 55), so it needs no level check. The level loaded is not m9_3 when the story continues from the
// autosave m9_3's end writes.
// FC_FLOW_START=N (a test): a profile loaded (0xD1590 case 0) is applied by 0x27C30, which copies the save's buffer
// (its flow step at 0x32ACE0, profile +0x224) into the live profile and the flow; N written there first makes the
// loaded game start at step N: 55 for the S041 movie and then the credits.
#include <cstdlib>
#include <cstring>

#include "core/achievements.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

#include "common/bulletproof.h"

namespace fc {
namespace {

constexpr uint32_t kFlowStart = 0x32ACE0;     // the loaded save's flow step (GameFlow, from 1)
constexpr uint32_t kDifficulty = 0x323618;    // float: the damage the player takes
constexpr uint32_t kOwnedSaved = 0x323498;    // the profile's unlocks, one byte an item
constexpr uint32_t kOwnedNew = 0x32DDB8;      // the unlocks since the last checkpoint
constexpr uint32_t kStash = 0x3254FC;         // the hub stash: a short count for each of the 20 weapons
constexpr uint32_t kPlayer = 0x32DCFC, kHolsters = 0x5BC, kHolsterSize = 0x1C;
constexpr int kWeapons = 20;
constexpr uint32_t kShopBought = 0xE38DD;     // the gun shop's call to 0x31D70 returns here

float Difficulty() {
    const uint32_t v = kernel::R32(kDifficulty);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

int WeaponsOwned() {
    bool owned[kWeapons] = {};
    for (int w = 0; w < kWeapons; w++) owned[w] = kernel::G<uint16_t>(kStash + 2 * w) != 0;
    const uint32_t player = kernel::R32(kPlayer);
    if (player)
        for (uint32_t h = 0; h < 6; h++) {
            const int32_t w = int32_t(kernel::R32(player + kHolsters + h * kHolsterSize));
            if (w >= 0 && w < kWeapons) owned[w] = true;
        }
    int n = 0;
    for (bool o : owned) n += o;
    return n;
}

int Owned(uint32_t first, uint32_t count) {
    if (first == 0) return WeaponsOwned();
    int n = 0;
    for (uint32_t i = first; i < first + count; i++)
        if (kernel::G<uint8_t>(kOwnedSaved + i) || kernel::G<uint8_t>(kOwnedNew + i)) n++;
    return n;
}

struct Count {
    const char* id;
    uint32_t first, count;  // the items counted (first 0: the weapons owned)
    int target;
    bool armed = false;  // seen short of the target since start
    int seen = -1;       // the last count
};
Count g_counts[] = {
    {"first_medal", 370, 10, 1},  {"all_medals", 370, 10, 10},   {"first_disarm", 290, 25, 1},
    {"ten_disarms", 290, 25, 10}, {"all_disarms", 290, 25, 25},  {"all_weapons", 0, 0, kWeapons},
};

}  // namespace

void AchievementFrame() {
    for (Count& c : g_counts) {
        const int n = Owned(c.first, c.count);
        if (c.first == 0 && n != c.seen) LOGR("bulletproof: weapons owned %d of 20", n);
        c.seen = n;
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

// a weapon into the stash (0x31D70, this 0x325320): from the gun shop, a purchase
X86_EXTERN_FUNC(__imp__sub_00031D70);
X86_FUNC(sub_00031D70) {
    if (kernel::R32(ctx.esp) == fc::kShopBought) {
        LOGR("bulletproof: weapon %u bought", kernel::R32(ctx.esp + 4));
        achievements::Award("first_weapon");
    }
    __imp__sub_00031D70(ctx, base);
}

// applying a loaded profile (0x27C30): FC_FLOW_START, above
X86_EXTERN_FUNC(__imp__sub_00027C30);
X86_FUNC(sub_00027C30) {
    static const char* start = std::getenv("FC_FLOW_START");
    static bool done = false;
    if (!done && start && *start) {
        done = true;
        LOGR("bulletproof: FC_FLOW_START %s (the save's %u)", start, kernel::R32(fc::kFlowStart));
        kernel::W32(fc::kFlowStart, uint32_t(std::atoi(start)));
    }
    __imp__sub_00027C30(ctx, base);
}
