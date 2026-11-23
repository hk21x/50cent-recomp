// The mouse in the menus: while one is up and the keyboard and mouse play, the left button plays A (select) and the
// right B (back) instead of what they play in play (the left fires: RT; the middle opens the weapons: Black), and the
// wheel is the d-pad's up and down, a press a notch (in play it changes weapons: src/game/weapon_keys.cpp). The menus show a pointer of their own (a chrome arrow, as the
// PDA's case); in the PDA's menus the row under it is highlighted and the left button selects only that row
// (src/game/menu_pointer.cpp); a press off the rows does nothing. On a slider's row (sensitivity, volume) a press on
// its value column's left or right half lowers or raises it, and the wheel over it changes it.
// The shops (the gun shop, Doc's, the bootlegger's, the disarms) are menus played in play: while one is up the mouse is
// the menus' too, and a click on one of their on-screen buttons (Back, Buy...) plays that button.
//
// A menu is up while the byte at 0x342A90 is set: 0x1047B0 sets it (from 0x84E40, which pauses the music, the speech
// and the movie textures for the pause menu, and from the front end's 0x1FE60), 0x10473D and 0x1047DA clear it. Memory
// compared headless: 1 at the title's menu and in the pause menu, 0 in play.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "app/app.h"
#include "bulletproof.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fc {
namespace {
// FC_POINTER=<file> headless: the pointer from the file's "x y buttons" (640x480, the menus' 4:3), read again each
// tenth of a second, so a run's script can move it
struct Forced {
    bool on = false;
    float x = 0, y = 0;
    uint32_t buttons = 0;
};
Forced ForcedPointer() {
    static const char* path = std::getenv("FC_POINTER");
    static Forced f;
    static std::chrono::steady_clock::time_point next{};
    if (!path || !*path) return f;
    static std::mutex lock;  // the pad's and the menu's frames both ask
    std::lock_guard<std::mutex> hold(lock);
    const auto now = std::chrono::steady_clock::now();
    if (now < next) return f;
    next = now + std::chrono::milliseconds(100);
    if (FILE* file = std::fopen(path, "r")) {
        Forced r;
        if (std::fscanf(file, "%f %f %u", &r.x, &r.y, &r.buttons) == 3) {
            r.on = true;
            if (r.x != f.x || r.y != f.y || r.buttons != f.buttons) LOGI("bulletproof: pointer %.0f %.0f buttons %u", r.x, r.y, r.buttons);
            f = r;
        }
        std::fclose(file);
    }
    return f;
}

#ifdef _WIN32
// the arrow while a menu is up: the runtime hides the cursor over the game (WM_SETCURSOR, and its half second timer
// kCursorTimer, sdk/runtime/app/main.cpp)
constexpr uintptr_t kCursorTimerId = 0x5243;  // kCursorTimer
WNDPROC g_proc = nullptr;
std::atomic<bool> g_arrow{false};

// The pointer's own picture in the PDA's colours: brushed chrome as its case, bright at the tip and steel at the tail,
// with the screen's darkest navy for its edge and a soft shadow, so it shows on the menus' blue and over the world
// behind the pause menu. An arrow of `size` pixels, its tip the hot spot, made for the window's height (32 at 1080
// lines) and again when that changes; Windows' arrow when it cannot be made.
HCURSOR MenuCursor(HWND hwnd) {
    static HCURSOR cursor = nullptr;
    static int made = 0;
    RECT r;
    int size = 32;
    if (GetClientRect(hwnd, &r) && r.bottom > 0) size = std::clamp(int(r.bottom * 32 / 1080), 24, 96);
    if (cursor && made == size) return cursor;
    // the arrow's outline in a unit square: tip, down the left edge, the notch, the tail, back up the right edge
    static const float kShape[7][2] = {{0.06f, 0.04f}, {0.06f, 0.80f}, {0.25f, 0.63f}, {0.39f, 0.93f},
                                       {0.52f, 0.87f}, {0.38f, 0.58f}, {0.64f, 0.58f}};
    const auto inside = [](float x, float y) {
        bool in = false;
        for (int i = 0, j = 6; i < 7; j = i++) {
            const float xi = kShape[i][0], yi = kShape[i][1], xj = kShape[j][0], yj = kShape[j][1];
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
        }
        return in;
    };
    const auto distance = [](float x, float y) {  // to the outline
        float best = 1e9f;
        for (int i = 0, j = 6; i < 7; j = i++) {
            const float ax = kShape[j][0], ay = kShape[j][1], bx = kShape[i][0] - ax, by = kShape[i][1] - ay;
            const float t = std::clamp(((x - ax) * bx + (y - ay) * by) / (bx * bx + by * by), 0.0f, 1.0f);
            best = std::min(best, std::hypot(x - ax - bx * t, y - ay - by * t));
        }
        return best;
    };
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof bi;
    bi.bV5Width = size, bi.bV5Height = -size, bi.bV5Planes = 1, bi.bV5BitCount = 32, bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000, bi.bV5GreenMask = 0x0000FF00, bi.bV5BlueMask = 0x000000FF, bi.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    HCURSOR next = nullptr;
    if (color && mask && bits) {
        auto* px = static_cast<uint32_t*>(bits);
        const float edge = 0.045f, shadow = 0.05f;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float fill = 0, line = 0, shade = 0;  // 4 by 4 samples
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++) {
                        const float u = (x + (sx + 0.5f) / 4) / size, v = (y + (sy + 0.5f) / 4) / size;
                        const bool in = inside(u, v);
                        const float d = distance(u, v);
                        if (in && d > edge * 0.5f) fill += 1;
                        else if (d <= edge * (in ? 0.5f : 0.6f)) line += 1;
                        if (inside(u - shadow, v - shadow) || distance(u - shadow, v - shadow) < edge) shade += 1;
                    }
                fill /= 16, line /= 16, shade /= 16;
                // chrome: near white at the tip to steel blue at the tail, a brighter streak down the middle
                const float t = std::clamp(float(y) / size / 0.9f, 0.0f, 1.0f);
                const float streak = 0.5f + 0.5f * std::cos(std::clamp((float(x) / size - 0.22f) * 9.0f, -3.14159f, 3.14159f));
                const float fr = 236 - 92 * t + 14 * streak, fg = 240 - 84 * t + 12 * streak, fb = 246 - 66 * t + 8 * streak;
                const float lr = 10, lg = 18, lb = 44;  // the edge: the PDA screen's darkest navy
                const float a = std::min(1.0f, fill + line + 0.45f * shade * (1 - fill - line));
                const float cr = a > 0 ? (fill * std::min(fr, 255.0f) + line * lr) / a : 0,
                            cg = a > 0 ? (fill * std::min(fg, 255.0f) + line * lg) / a : 0,
                            cb = a > 0 ? (fill * std::min(fb, 255.0f) + line * lb) / a : 0;
                px[y * size + x] = uint32_t(a * 255 + 0.5f) << 24 | uint32_t(cr + 0.5f) << 16 | uint32_t(cg + 0.5f) << 8 |
                                   uint32_t(cb + 0.5f);
            }
        ICONINFO info{};
        info.fIcon = FALSE;
        info.xHotspot = DWORD(0.06f * size), info.yHotspot = DWORD(0.04f * size);
        info.hbmMask = mask, info.hbmColor = color;
        next = reinterpret_cast<HCURSOR>(CreateIconIndirect(&info));
    }
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    if (!next) return cursor ? cursor : LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    if (cursor) DestroyCursor(cursor);
    cursor = next, made = size;
    LOGR("bulletproof: the menus' pointer picture made, %d pixels", size);
    return cursor;
}

LRESULT CALLBACK PointerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_arrow.load() && !app::OverlayOpen()) {
        if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT) {
            SetCursor(MenuCursor(hwnd));
            return TRUE;
        }
        if (msg == WM_TIMER && wp == kCursorTimerId) return 0;  // the runtime's timer would hide it while it stands still
    }
    return CallWindowProcW(g_proc, hwnd, msg, wp, lp);
}
void ShowArrow(bool show) {
    HWND hwnd = static_cast<HWND>(app::Window());
    if (!hwnd) return;
    if (!g_proc) {
        g_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PointerProc)));
        LOGR("bulletproof: the menus' pointer is ready");
    }
    if (g_arrow.exchange(show) != show) PostMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}
#endif

// a press of control c (a d-pad bit or an analog button)
void Press(input::Pad& p, int c) {
    if (c < input::kA) p.buttons |= uint16_t(1u << c);
    else p.analog[c - input::kA] = 255;
}
}  // namespace

bool PointerAt(float& x, float& y) {
    const Forced f = ForcedPointer();
    if (f.on) {
        x = f.x, y = f.y;
        return true;
    }
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(app::Window());
    POINT c;
    RECT r;
    if (!input::UsingKeyboard() || GetForegroundWindow() != hwnd || !GetCursorPos(&c) || !ScreenToClient(hwnd, &c) ||
        !GetClientRect(hwnd, &r) || r.right <= 0 || r.bottom <= 0)
        return false;
    // the menus' 4:3 stays in the frame's middle at any width
    x = 640 * (0.5f + (float(c.x) / r.right - 0.5f) * app::FrameAspect() / (4.0f / 3.0f));
    y = 480 * float(c.y) / r.bottom;
    return true;
#else
    (void)x, (void)y;
    return false;
#endif
}

namespace {
std::atomic<int> g_weapon_wheel{0}, g_weapon_key{0};
}
void WeaponWheel(int notches) { g_weapon_wheel += notches; }
void WeaponKey(int key) { g_weapon_key = key; }
int TakeWeaponWheel() { return g_weapon_wheel.exchange(0); }
int TakeWeaponKey() { return g_weapon_key.exchange(0); }

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
    // a slider's steps to play (raise positive): each one the d-pad's left or right for a single read of the pad, one
    // read apart, as the slider moves a step for every frame the d-pad is held (a 70 ms press moved it 8)
    static int steps = 0;
    static bool stepped = false;
    static bool was = false;
    const int notches = input::TakeWheel();  // taken in play too, so none wait for the next menu
    // a shop's menu is played in play (no menu up), but takes the mouse as a menu does
    const bool menu = MenuUp() || ShopUp();
    if (menu != was) {
        LOGR("bulletproof: %s", menu ? "a menu is up: the mouse plays A, B and the d-pad" : "out of the menus");
        pending = 0, steps = 0;
    }
    was = menu;
#ifdef _WIN32
    ShowArrow(menu && input::UsingKeyboard());  // before the return below: out of the menus the runtime hides it again
#endif
    if (!menu) {
        if (notches) WeaponWheel(notches);  // in play the wheel changes weapons (src/game/weapon_keys.cpp)
        return;
    }
    using namespace std::chrono;
    const int64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    const Forced forced = ForcedPointer();
    if (input::UsingKeyboard() || forced.on) {
        const uint32_t held = forced.on ? forced.buttons : input::MouseButtons();
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
        // the left button: A over the highlighted row, nothing off the rows (decided as it goes down; over another
        // row, once the pointer's frame has highlighted it); A as before in the menus not placed (PointerHit unknown)
        static bool was_left = false, armed = false;
        static int button = -1;  // a shop's on-screen button pressed: its control, held while the left button is
        static int64_t repeat_at = 0;
        const int hit = PointerHit(), slide = PointerSlider();
        const bool down = (held & 1) && !was_left;
        if (down) {
            button = PointerButton();
            armed = button < 0 && hit != kPointerOff;
            LOGI("bulletproof: menu click (pointer %d, slider %d, button %d)", hit, slide, button);
        }
        was_left = held & 1;
        if (!(held & 1)) button = -1;
        if (button >= 0) {
            Press(p, button);
        } else if ((held & 1) && armed && (slide == -1 || slide == 1)) {
            // a slider's value column: its left half lowers a step, its right half raises one; held, a step every
            // tenth of a second after 0.4 s
            if (down) steps += slide, repeat_at = now + 400;
            else if (now >= repeat_at && std::abs(steps) < 2) steps += slide, repeat_at = now + 100;
        } else if ((held & 1) && armed && (hit == kPointerOnCurrent || hit == kPointerUnknown)) {
            Press(p, input::kA);
        }
        if (held & 2) Press(p, input::kB);
        static uint32_t was_held = 0;
        if (held != was_held) LOG_FIRST(16, "bulletproof: menu mouse buttons %u", held);
        was_held = held;
    }
    if (notches) LOG_FIRST(16, "bulletproof: menu wheel %d", notches);
    // over a highlighted slider the wheel changes its value a step a notch (up raises) instead of moving the highlight
    if (notches && PointerSlider() != 0) steps = std::clamp(steps + notches, -20, 20);
    else if (notches) {
        if (!pending) pulse = now;
        pending = std::clamp(pending + notches, -8, 8);
    }
    if (steps && !stepped) {
        Press(p, steps > 0 ? input::kRight : input::kLeft);
        LOGI("bulletproof: slider step %s", steps > 0 ? "right" : "left");
        steps += steps > 0 ? -1 : 1;
        stepped = true;
    } else {
        stepped = false;
    }
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
