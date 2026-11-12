#include <array>
#include <cstdlib>

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

// after the settings load: FC_MODERN_BUTTONS (1 or 0) overrides it, for the headless harness, which has no settings file
void Init() {
    if (const char* e = std::getenv("FC_MODERN_BUTTONS"); e && *e) {
        modern_buttons = *e == '1';
        LOGR("bulletproof: FC_MODERN_BUTTONS=%c", *e);
    }
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
    .options = {{"modern_buttons", &fc::modern_buttons, nullptr, "Modern button pictures",
                 "The prompts as today's Xbox pad marks them: LB and RB for White and Black, LT and RT, View and Menu for "
                 "Back and Start. With the keyboard they show the keys. Off, the console's."}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = fc::Init,                     // after the settings load: the harness's overrides
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = fc::TestPad,                   // FC_PAD_CMD's presses, for a machine with no one at the pad (src/common/test_pad.cpp)
    .rumble = nullptr,                    // changes a rumble before it is set
    .mouse_look = true,                   // the mouse turns the camera and aims (the right stick)
    .default_keys = fc::Keys,             // WASD and the mouse
    .texture_bound = fc::ButtonPictures,  // the prompts' pictures (src/common/buttons.cpp)
};
