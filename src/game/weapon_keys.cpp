// The mouse wheel and the keys 1, 2 and 3 change weapons in play.
//
// 50 carries his weapons in holsters: the player (0x32DCFC) +0x5BC holds 7 slots of 0x1C bytes (+0 the weapon, -1 for
// none; +0xC the holster's class; +0x10 the slot in use). Slots 0 and 1 (the arms) take class 1 (pistols), 2 (the hip)
// class 2 (SMGs), 3 (the back) class 3 (rifles, shotguns, the heavy weapons); 4 is the left hand (dual-wield), 5 the
// right hand: the weapon held. A weapon's class is the weapon table's (0x2E0D70, 0x98 bytes a weapon) +0x60.
//
// The player's input (0x86400, ecx the controller) has Black: a tap (held under 0.25 s, 0x334F5C counting) calls
// 0x46C00, which steps the choice (player +0x3C94, a slot) to the next slot with a weapon (skipping 4) and shows it
// on the HUD (0x45400); 0x334F58 then counts up and past 0.25 s the player's event 10 draws the chosen weapon. Held
// longer, Black opens the weapon inventory (0x4EA90, 0x32FAE4 set). 0x334F60 is set from a latch the Black press
// leaves (the holsters' +0xE8, taken by 0x1BB40): the Black code runs only while it is set, and the draw clears it. Read from the decompiled code and memory dumps in the alley
// (2026-11-21). Tried headless with FC_GIVE_WEAPONS=7,14,1: each key and notch drew the weapon chosen (the hand's
// slot read at the next request).
//
// Here, before 0x86400 runs: a wheel notch down is the game's own tap (0x46C00), a notch up the same step backwards;
// the draw follows a tenth of a second after the last notch, so several notches pass over weapons. 1, 2 and 3 choose
// the holster of that class and draw it at once; pressed again, the next of that class (the arms hold two: pistols,
// and the grenades, molotovs and sticky bombs, which are class 1 too).
// FC_GIVE_WEAPONS=7,14 (weapon numbers) gives those once in play, to try it headless.
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "common/bulletproof.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
constexpr uint32_t kPlayer = 0x32DCFC, kHolsters = 0x5BC, kChoice = 0x3C94, kIsHero = 0x588;
constexpr uint32_t kBlackHeld = 0x334F5C, kSinceChoice = 0x334F58, kBlackPressed = 0x334F60, kInventory = 0x32FAE4;
constexpr uint32_t kBusy = 0x3297B0;  // 0x86400 leaves the weapons alone while it is set
constexpr uint32_t kWeaponTable = 0x2E0D70;
constexpr uint32_t kStepChoice = 0x46C00, kShowChoice = 0x45400, kAddWeapon = 0x469B0, kEmptyHolster = 0x1BCC0,
                   kAddAmmo = 0x1CD00;
constexpr float kDrawAfter = 0.25f;  // 0x2B83FC

uint8_t R8(uint32_t va) { return *static_cast<uint8_t*>(kernel::HostPtr(va)); }
void W8(uint32_t va, uint8_t v) { *static_cast<uint8_t*>(kernel::HostPtr(va)) = v; }
void WF(uint32_t va, float v) { std::memcpy(kernel::HostPtr(va), &v, 4); }

// the Black code's timer: the chosen weapon drawn `wait` seconds on, as after a tap
void DrawIn(float wait) {
    W8(kBlackPressed, 1);
    WF(kBlackHeld, 0);
    WF(kSinceChoice, kDrawAfter - wait + 0.001f);
}

int Weapon(uint32_t player, int slot) {
    const uint32_t a = player + kHolsters + slot * 0x1C;
    return R8(a + 0x10) ? int(kernel::R32(a)) : -1;
}
int Class(int weapon) {
    if (weapon < 0 || weapon >= 24) return 0;
    return int(kernel::R32(kernel::R32(kWeaponTable) + weapon * 0x98 + 0x60));
}

uint32_t CallThis(X86Context& ctx, uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args) {
    const uint32_t ecx = ctx.ecx;
    ctx.ecx = self;
    const uint32_t r = kernel::CallGuest(ctx, fn, args);
    ctx.ecx = ecx;
    return r;
}

// the choice moved to `slot`, shown, and drawn `wait` seconds on
void Choose(X86Context& ctx, uint32_t player, int slot, float wait) {
    kernel::W32(player + kChoice, uint32_t(slot));
    CallThis(ctx, kShowChoice, player, {});
    DrawIn(wait);
}

void GiveTestWeapons(X86Context& ctx, uint32_t player) {
    static bool done = false;
    static const char* list = std::getenv("FC_GIVE_WEAPONS");
    if (done || !list || !*list) return;
    done = true;
    std::stringstream ss(list);
    std::string w;
    while (std::getline(ss, w, ',')) {
        const int weapon = std::atoi(w.c_str());
        const uint32_t holster = CallThis(ctx, kEmptyHolster, player + kHolsters, {uint32_t(Class(weapon))});
        if (holster == 0xFFFFFFFF) continue;
        CallThis(ctx, kAddWeapon, player, {uint32_t(weapon), holster});
        // and 100 rounds, as the Lua AddWeapon's third argument (0x7E7A4): the weapon's ammo type (+0x58)
        const uint32_t ammo_type = kernel::R32(kernel::R32(kWeaponTable) + weapon * 0x98 + 0x58);
        CallThis(ctx, kAddAmmo, player + kHolsters, {ammo_type, 100});
        LOGR("bulletproof: weapon %d given (holster %u)", weapon, holster);
    }
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00086400);
X86_FUNC(sub_00086400) {
    const uint32_t self = ctx.ecx;
    const int notches = fc::TakeWeaponWheel();
    // the Game actions weapon_1 to weapon_3 (keys 1, 2, 3 unless changed on the Controls page) as each goes down, and
    // FC_PAD_CMD's KEY1 to KEY3
    int key = fc::TakeWeaponKey();
    static bool was[3] = {};
    for (int i = 0; i < 3; i++) {
        const bool held = input::ActionHeld(i);
        if (held && !was[i]) key = i + 1;
        was[i] = held;
    }
    const uint32_t player = kernel::R32(kPlayer);
    if (player && R8(player + kIsHero) && !kernel::R32(kBusy) && !kernel::R32(kInventory) && !fc::MenuUp()) {
        GiveTestWeapons(ctx, player);
        int held = 0;  // slots with a weapon (the game's step needs two)
        for (int s : {0, 1, 2, 3, 5}) held += Weapon(player, s) != -1;
        if (held >= 2 && notches < 0) {  // down: the game's own step, a notch each
            for (int n = 0; n < -notches && n < 5; n++) CallThis(ctx, kStepChoice, player, {});
            DrawIn(0.1f);
            LOGR("bulletproof: wheel down %d: slot %d", -notches, int(kernel::R32(player + kChoice)));
        } else if (held >= 2 && notches > 0) {  // up: the step backwards
            int slot = int(kernel::R32(player + kChoice));
            if (slot < 0 || slot > 5) slot = 5;
            for (int n = 0; n < notches && n < 5; n++)
                do slot = (slot + 5) % 6;
                while (slot == 4 || Weapon(player, slot) == -1);
            Choose(ctx, player, slot, 0.1f);
            LOGR("bulletproof: wheel up %d: slot %d", notches, slot);
        }
        if (key >= 1 && key <= 3) {
            const bool in_hand = Class(Weapon(player, 5)) == key;
            // the holsters of that class from the one after the last chosen: the arms' two (pistols and the thrown
            // weapons, class 1 too) take turns, the weapon put away going where the drawn one was
            static int last = -1;
            int pick = -1;
            for (int n = 1; n <= 4 && pick < 0; n++) {
                const int s = (last + n + 4) % 4;
                if (Weapon(player, s) != -1 && Class(Weapon(player, s)) == key) pick = s;
            }
            if (pick >= 0) last = pick;
            if (pick >= 0) Choose(ctx, player, pick, 0);
            LOGR("bulletproof: key %d: slot %d%s", key, pick, pick < 0 ? (in_hand ? " (in the hand)" : " (none)") : "");
        }
    }
    ctx.ecx = self;
    __imp__sub_00086400(ctx, base);
}
