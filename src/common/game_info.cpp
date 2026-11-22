#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <string>

#include "app/app.h"
#include "bulletproof.h"
#include "core/input.h"
#include "core/log.h"

namespace fc {
namespace {
// keyboard and mouse (Windows virtual key codes: this file builds for the headless harness too)
constexpr uint16_t kMouseLeft = 0x01, kMouseMiddle = 0x04, kBackspace = 0x08, kTab = 0x09, kEnter = 0x0D, kEscape = 0x1B,
                   kSpace = 0x20, kArrowLeft = 0x25, kArrowUp = 0x26, kArrowRight = 0x27, kArrowDown = 0x28,
                   kLeftShift = 0xA0, kLeftCtrl = 0xA2;
}  // namespace

// The console's controls (fixed: no page in the game sets them), from the PS2 version's list matched to the Xbox's
// buttons and tried headless in the first mission (2026-11-11): the left stick moves, the right stick turns the camera
// and aims; A jumps; B takes a hostage, executes, grabs mobile cover; X is the action (reload, counter-kill, cover,
// throw); Y evades, dual-wields, interrogates, dives; RT fires; LT crouches; White heals; Black opens the weapon
// inventory; Back shows the music track; Start pauses (the G-Unit mobile). On today's pad White and Black are LB and RB.
// The keys: WASD and the mouse, as shooters have them; Enter and Backspace still choose and go back in the menus.
std::array<uint16_t, 2> Keys(int c) {
    using namespace input;
    switch (c) {
        case kLSUp: return {'W', 0};
        case kLSDown: return {'S', 0};
        case kLSLeft: return {'A', 0};
        case kLSRight: return {'D', 0};
        case kRT: return {kMouseLeft, 0};        // fire
        case kLT: return {'C', kLeftCtrl};        // crouch
        case kA: return {kSpace, kEnter};         // jump; Enter selects in the menus
        case kB: return {'F', kBackspace};        // hostage, execute, grab cover; Backspace goes back in the menus
        case kX: return {'E', 'R'};               // action, reload, counter-kill, cover
        case kY: return {kLeftShift, 0};          // evade, dive, dual-wield, interrogate
        case kWhite: return {'H', 0};             // heal
        case kBlack: return {'Q', kMouseMiddle};  // weapon inventory
        case kLThumb: return {'Z', 0};
        case kRThumb: return {'X', 0};
        case kUp: return {kArrowUp, 0};
        case kDown: return {kArrowDown, 0};
        case kLeft: return {kArrowLeft, 0};
        case kRight: return {kArrowRight, 0};
        case kBack: return {kTab, 0};             // the music track
        case kStart: return {kEscape, 'P'};       // pause, the G-Unit mobile
        default: return {0, 0};
    }
}

bool skip_intro = false;
bool hide_hud = false;
uint32_t fov_percent = 100;  // the world's field of view, % of the game's (src/game/aspect.cpp)  // the HUD's layer left undrawn (src/game/hud.cpp); F9 in play switches it

// F9 (not a repeat) in play, no menu up: the HUD hidden or shown, and saved (WM_KEYDOWN, the virtual key in wp)
bool Message(void* /*hwnd*/, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* /*result*/) {
    constexpr uint32_t kKeyDown = 0x100;
    constexpr uintptr_t kF9 = 0x78;
    if (msg != kKeyDown || wp != kF9 || (lp & (1 << 30)) || MenuUp()) return false;
    hide_hud = !hide_hud;
    app::Save();
    LOGR("bulletproof: the HUD %s (F9)", hide_hud ? "hidden" : "shown");
    return true;
}

// skip_intro: the boot's logo movies and teaser (Ark\movies: Sierra, Genuine Games, Dolby, Interscope, Havok, the
// teaser) reported missing, which the game passes over to the title; the story's movies (seq, cs) stay
bool HideFile(const std::string& path) {
    if (!skip_intro) return false;
    std::string n;
    for (char c : path) n += char(std::tolower(static_cast<unsigned char>(c)));
    for (const char* movie : {"\\movies\\sierra.sfd", "\\movies\\gglogo.sfd", "\\movies\\dolbyn.sfd", "\\movies\\inter.sfd",
                              "\\movies\\havok.sfd", "\\movies\\teaser.sfd"}) {
        const size_t k = std::char_traits<char>::length(movie);
        if (n.size() >= k && n.compare(n.size() - k, std::string::npos, movie) == 0) return true;
    }
    return false;
}

// after the settings load: FC_MODERN_BUTTONS (1 or 0) overrides it, for the headless harness, which has no settings file
void Init() {
    if (const char* e = std::getenv("FC_MODERN_BUTTONS"); e && *e) {
        modern_buttons = *e == '1';
        LOGR("bulletproof: FC_MODERN_BUTTONS=%c", *e);
    }
    if (const char* e = std::getenv("FC_SKIP_INTRO"); e && *e) skip_intro = *e == '1';
    if (const char* e = std::getenv("FC_FOV"); e && *e) fov_percent = uint32_t(std::atoi(e));
    fov_percent = std::clamp<uint32_t>(fov_percent, 50, 150);
    if (const char* e = std::getenv("FC_HIDE_HUD"); e && *e) hide_hud = *e == '1';
    if (hide_hud) LOGR("bulletproof: the HUD is hidden (hide_hud)");
    if (skip_intro) LOGR("bulletproof: the intro movies are skipped (skip_intro)");
    // every achievement's condition parses (a typo would leave it never met)
    for (const achievements::Def& d : app::kGame.achievements)
        if (std::string e = achievements::Check(d.condition); !e.empty()) LOGE("bulletproof: achievement %s: %s", d.id, e.c_str());
}
}  // namespace fc

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"50 Cent: Bulletproof",     // window and splash
    .short_title = L"Bulletproof",        // message boxes
    .name = L"Bulletproof",               // window classes, crash dumps, handover event
    .log = "bulletproof",                 // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "Bulletproof"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"skip_intro", &fc::skip_intro, nullptr, "Skip intro movies",
                 "Starts at the title, without the Sierra, Genuine Games, Dolby, Interscope and Havok logos and the teaser."},
                {"hide_hud", &fc::hide_hud, nullptr, "Hide the HUD",
                 "Health, cash, the weapon and the messages left out, for screenshots and a clear view. F9 switches it in "
                 "play; the menus and cutscenes still show."},
                {"fov_percent", nullptr, &fc::fov_percent, "Field of view (%)",
                 "50 to 150; 100 is the game's. Wider shows more around 50 Cent, at once; aiming zooms in proportion."},
                {"modern_buttons", &fc::modern_buttons, nullptr, "Modern button pictures",
                 "The prompts as today's Xbox pad marks them: LB and RB for White and Black, LT and RT, View and Menu for "
                 "Back and Start. With the keyboard they show the keys. Off, the console's."}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = fc::Init,                     // after the settings load: the harness's overrides
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = fc::Message,               // F9 hides the HUD
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = fc::Pad,                       // the mouse in the menus (src/common/menu_mouse.cpp), FC_PAD_CMD's presses
    .rumble = nullptr,                    // changes a rumble before it is set
    .achievements = fc::AchievementSet(), // the story, difficulty, unlocks and cash (src/common/achievement_set.cpp)
    .hud_aspect = 4.0f / 3.0f,            // the 2D is laid out for a 4:3 TV (src/game/widescreen.cpp)
    .squeeze_screen_programs = true,      // its 2D is drawn through vertex programs
    .hide_file = fc::HideFile,            // skip_intro
    .mouse_look = true,                   // the mouse turns the camera and aims
    .mouse_direct = true,                 // directly, not as the right stick (src/game/mouse_aim.cpp)
    .default_keys = fc::Keys,             // WASD and the mouse
    // the weapon keys (src/game/weapon_keys.cpp), on the Controls page's Game actions
    .actions = {{"weapon_1", "Weapon: pistol or thrown", {'1', 0}},
                {"weapon_2", "Weapon: SMG", {'2', 0}},
                {"weapon_3", "Weapon: rifle or shotgun", {'3', 0}}},
    .texture_bound = fc::ButtonPictures,  // the prompts' pictures (src/common/buttons.cpp)
    .aspect_live = true,                  // the aspect setting applies at once (src/game/aspect.cpp)
};
