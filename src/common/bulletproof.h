// 50 Cent: Bulletproof's own settings and hooks, shared by src/common and src/game
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "core/achievements.h"
#include "core/input.h"

namespace fc {
extern bool modern_buttons;  // the prompts show today's Xbox pad (src/common/buttons.cpp)
// FNV-1a of buttons_Xbox.xtf's 512x64 P8 bytes as the disc has them
constexpr uint64_t kButtonsDisc = 0xD9B9754E92AA4661ull;  // at 0x81BD6D80 in the title menu, 2026-11-11
bool ButtonPictures(uint32_t header, uint32_t palette);  // GameInfo::texture_bound
std::array<uint16_t, 2> Keys(int control);               // GameInfo::default_keys
void Init();                                             // GameInfo::init
void TestPad(int port, input::Pad& pad);                 // FC_PAD_CMD's presses (src/common/test_pad.cpp)
void Pad(int port, input::Pad& pad);                     // GameInfo::pad: TestPad, then the mouse in the menus
extern uint32_t fov_percent;                             // the world's field of view, % (src/game/aspect.cpp)
extern bool console_widescreen;                          // the console's widescreen cameras on a wide frame (src/game/aspect.cpp)
bool WideCameras();                                      // the game's widescreen cameras are used (src/game/aspect.cpp)
extern bool hide_hud;                                    // the HUD left undrawn (src/game/hud.cpp; F9)
// the weapon keys (src/game/weapon_keys.cpp): the wheel's notches in play (up positive) and 1, 2 or 3 pressed, each
// taken once by the player's input
void WeaponWheel(int notches);
void WeaponKey(int key);
int TakeWeaponWheel();
int TakeWeaponKey();
bool MenuUp();                                           // a menu is up (the front end or the pause menu)
bool PointerForced();  // the pointer from FC_POINTER (a test), in the frame's own fractions
bool PointerAt(float& x, float& y);                      // the mouse pointer in the menus' 640x480 (src/common/menu_mouse.cpp)
// what the pointer is over in the PDA's menus (src/game/menu_pointer.cpp); unknown in the others
enum { kPointerUnknown, kPointerOff, kPointerOnCurrent, kPointerOnOther };
int PointerHit();
bool ShopUp();         // a shop's menu is up (the gun shop, Doc's, the bootlegger's, the disarms, the crib's TV and stash)
int PointerButton();
bool ShopAcross();     // a shop picked from with left and right (the gun shop): the wheel steps across   // the pad control of a shop's on-screen button under the pointer, -1 for none
// the pointer over the highlighted row, a slider: -1 or 1 over the left or right half of its value column, 2 elsewhere
// on the row; 0 not over one
int PointerSlider();
void PcSettingsPad(input::Pad& pad);
void OpenPcPage();                                       // the PC Settings page in the PDA (src/game/pc_page.cpp)
uint32_t PcPageEntry(uint32_t hash);                     // its rows' string table entries                     // Y on an Options page opens the settings (src/game/pc_settings.cpp)
std::vector<achievements::Def> AchievementSet();         // GameInfo::achievements (src/common/achievement_set.cpp)
void UpdateAspectHold();  // at each view setup: the frame held at 4:3 (in-engine cutscenes) or 16:9 (front end menus)
bool HudLayoutWanted();  // in the gameplay frame with no menu up: the HUD layer's pieces kept at the frame's edges
void AchievementFrame();  // each gameplay frame: the unlock counts (src/game/achievement_checks.cpp)
}  // namespace fc
