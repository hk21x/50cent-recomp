// The mouse in the menus (Agent Under Fire's and True Crime's way, without a pointer): while one is up and the keyboard
// and mouse play, the left button plays A (select) and the right B (back) instead of what they play in play (the left
// fires: RT; the middle opens the weapons: Black), and the wheel is the d-pad's up and down, a press a notch.
//
// A menu is up while the byte at 0x342A90 is set: 0x1047B0 sets it (from 0x84E40, which pauses the music, the speech
// and the movie textures for the pause menu, and from the front end's 0x1FE60), 0x10473D and 0x1047DA clear it. Memory
// compared headless: 1 at the title's menu and in the pause menu, 0 in play.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

#include "bulletproof.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace fc {
namespace {
// a press of control c (a d-pad bit or an analog button)
void Press(input::Pad& p, int c) {
    if (c < input::kA) p.buttons |= uint16_t(1u << c);
    else p.analog[c - input::kA] = 255;
}
}  // namespace

bool MenuUp() {
    uint8_t v = 0;
    std::memcpy(&v, kernel::HostPtr(0x342A90), 1);
    return v != 0;
}

void MenuMouse(input::Pad& p) {
    constexpr int64_t kPress = 70, kGap = 70;               // ms
    constexpr uint16_t kMouseBits[3] = {0x01, 0x02, 0x04};  // VK_LBUTTON VK_RBUTTON VK_MBUTTON
    static int pending = 0;                                 // notches to play, up positive
    static int64_t pulse = 0;
    static bool was = false;
    const int notches = input::TakeWheel();  // taken in play too, so none wait for the next menu
    const bool menu = MenuUp();
    if (menu != was) {
        LOGR("bulletproof: %s", menu ? "a menu is up: the mouse plays A, B and the d-pad" : "out of the menus");
        pending = 0;
    }
    was = menu;
    if (!menu) return;
    using namespace std::chrono;
    const int64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (input::UsingKeyboard()) {
        const uint32_t held = input::MouseButtons();
        // the buttons out of what they play in play
        for (int c = 0; c < input::kA + 8; c++) {
            auto keys = input::ControlKeys(c);
            bool mouse = false;
            for (int b = 0; b < 3; b++)
                if (held >> b & 1) mouse |= keys[0] == kMouseBits[b] || keys[1] == kMouseBits[b];
            if (!mouse) continue;
            if (c < input::kA) p.buttons = uint16_t(p.buttons & ~(1u << c));
            else p.analog[c - input::kA] = 0;
        }
        if (held & 1) Press(p, input::kA);
        if (held & 2) Press(p, input::kB);
        static uint32_t was_held = 0;
        if (held != was_held) LOG_FIRST(16, "bulletproof: menu mouse buttons %u", held);
        was_held = held;
    }
    if (notches) LOG_FIRST(16, "bulletproof: menu wheel %d", notches);
    if (notches && !pending) pulse = now;
    pending = std::clamp(pending + notches, -8, 8);
    if (pending) {
        if (now - pulse < kPress) {
            Press(p, pending > 0 ? input::kUp : input::kDown);
        } else if (now - pulse >= kPress + kGap) {
            pending += pending > 0 ? -1 : 1;
            pulse = now;
        }
    }
}

// GameInfo::pad: the test hook's presses (src/common/test_pad.cpp), then the mouse in the menus
void Pad(int port, input::Pad& p) {
    TestPad(port, p);
    if (port != 0) return;
    MenuMouse(p);
    PcSettingsPad(p);
}
}  // namespace fc
