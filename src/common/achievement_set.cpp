// Bulletproof's achievements: conditions in RetroAchievements' syntax on default.xbe's memory, or "0=1" for those
// src/game/achievement_checks.cpp awards from its hooks (the credits, the unlock counts). How each address was
// found: .local/progress_re.md.
//
// The story (flow\flow.lua in Ark\Scripts\scripts.ark) loads each level through LoadMission (0x268D0), whose 0x348D0
// keeps its number at 0x32DD94: "mX_Y" is X*10+Y, the hub 0. The missions run m0 (Queens), m2 (Safe House), m1
// (Junkyard), m3, m4, m5, m6, m7, m8, m9 (Vessel), so one is done when the next one's first level loads; the last when
// the credits after it start. The cash a character carries is at +0x3C28 of it, the player's at 0x32DCFC.
#include <deque>
#include <string>
#include <vector>

#include "common/bulletproof.h"

namespace fc {
namespace {

struct Mission {
    const char* id;
    const char* name;
    uint32_t next;  // the level number of the next mission's first level (0: the last mission)
};
constexpr Mission kMissions[10] = {
    {"queens", "Queens", 20},           {"safe_house", "Safe House", 10},
    {"junkyard", "Junkyard", 30},       {"chinatown", "Chinatown", 40},
    {"disused_subway", "Disused Subway", 50}, {"projects", "Projects", 60},
    {"docklands", "Docklands", 70},     {"city_morgue", "City Morgue", 80},
    {"little_italy", "Little Italy", 90}, {"vessel", "Vessel", 0},
};

// strings that stay where they are for the set's life (a deque never moves what it holds)
const char* Keep(std::string s) {
    static std::deque<std::string> kept;
    return kept.emplace_back(std::move(s)).c_str();
}

// the level just became `level` (its number one frame before was another)
const char* Entered(uint32_t level) {
    return Keep("d0xX32dd94!=" + std::to_string(level) + "_0xX32dd94=" + std::to_string(level));
}
// the player carries at least `cash`
const char* Carries(uint32_t cash) { return Keep("I:0xX32dcfc_0xX3c28>=" + std::to_string(cash)); }

constexpr const char* kAwarded = "0=1";  // never met: awarded by a hook

}  // namespace

std::vector<achievements::Def> AchievementSet() {
    std::vector<achievements::Def> set;
    for (const Mission& m : kMissions) {
        const bool last = m.next == 0;
        set.push_back({m.id, m.name,
                       Keep(std::string(last ? "Finish the story: complete " : "Complete the ") + m.name +
                            (last ? "." : " mission.")),
                       last ? 25u : 10u, last ? kAwarded : Entered(m.next)});
    }
    set.push_back({"finish_gangsta", "Hard Road", "Finish the story on Gangsta or G-Unit Soldier.", 50, kAwarded});
    set.push_back({"finish_g_unit", "Soldier Through", "Finish the story on G-Unit Soldier.", 100, kAwarded});
    set.push_back({"first_medal", "Decorated", "Pick up a Grizz medal.", 10, kAwarded});
    set.push_back({"all_medals", "Full Rack", "Collect all 10 Grizz medals.", 50, kAwarded});
    set.push_back({"first_disarm", "New Move", "Buy a disarm.", 10, kAwarded});
    set.push_back({"ten_disarms", "Close Quarters", "Own 10 disarms.", 25, kAwarded});
    set.push_back({"all_disarms", "Every Answer", "Own all 25 disarms.", 50, kAwarded});
    set.push_back({"first_weapon", "Armed Up", "Buy a weapon.", 10, kAwarded});
    set.push_back({"all_weapons", "Full Arsenal", "Own all 20 weapons at once.", 50, kAwarded});
    set.push_back({"cash_10k", "Pocket Money", "Carry $10,000.", 10, Carries(10000)});
    set.push_back({"cash_50k", "Stacking Up", "Carry $50,000.", 25, Carries(50000)});
    set.push_back({"cash_100k", "Six Figures", "Carry $100,000.", 50, Carries(100000)});
    return set;
}

}  // namespace fc
