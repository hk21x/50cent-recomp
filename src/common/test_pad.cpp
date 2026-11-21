// A test hook for machines with no one at the pad (the Windows VM): FC_PAD_CMD=<file>. The file's lines are run in
// order and the file is deleted once read; another can be written after. A line is `buttons [seconds]`, buttons joined
// with + as the harness's pad scripts name them (UP DOWN LEFT RIGHT START BACK A B X Y BLACK WHITE LT RT, LS LSD LSL
// LSR and RS RSD RSL RSR for the sticks; WHEELUP WHEELDOWN KEY1 KEY2 KEY3 a wheel notch or a weapon key), held that
// long (1 s by default) then let go for half a second; `WAIT s` waits,
// `SHOT` takes a screenshot (as F12). Seconds, not frames: a software renderer draws a few frames a second, and the
// game runs by the clock. With it set, controller 1 counts as plugged in. Each step is logged ("testpad:").
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include "app/app.h"
#include "bulletproof.h"
#include "core/input.h"
#include "core/log.h"

namespace fc {
namespace {
using Clock = std::chrono::steady_clock;

struct Step {
    std::string buttons;  // empty: a wait
    double seconds = 1.0;
    bool shot = false;
};

std::mutex g_lock;
std::deque<Step> g_steps;
Clock::time_point g_step_end{}, g_next_check{};
bool g_holding = false;
Step g_current;

void Press(const std::string& names, input::Pad& p) {
    std::stringstream ss(names);
    std::string b;
    while (std::getline(ss, b, '+')) {
        if (b == "UP") p.buttons |= 1;
        else if (b == "DOWN") p.buttons |= 2;
        else if (b == "LEFT") p.buttons |= 4;
        else if (b == "RIGHT") p.buttons |= 8;
        else if (b == "START") p.buttons |= 16;
        else if (b == "BACK") p.buttons |= 32;
        else if (b == "LTHUMB") p.buttons |= 64;
        else if (b == "RTHUMB") p.buttons |= 128;
        else if (b == "A") p.analog[0] = 255;
        else if (b == "B") p.analog[1] = 255;
        else if (b == "X") p.analog[2] = 255;
        else if (b == "Y") p.analog[3] = 255;
        else if (b == "BLACK") p.analog[4] = 255;
        else if (b == "WHITE") p.analog[5] = 255;
        else if (b == "LT") p.analog[6] = 255;
        else if (b == "RT") p.analog[7] = 255;
        else if (b == "LS") p.ly = 32767;
        else if (b == "LSD") p.ly = -32767;
        else if (b == "LSL") p.lx = -32767;
        else if (b == "LSR") p.lx = 32767;
        else if (b == "RS") p.ry = 32767;
        else if (b == "RSD") p.ry = -32767;
        else if (b == "RSL") p.rx = -32767;
        else if (b == "RSR") p.rx = 32767;
    }
}

void ReadCommands(const char* path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return;
    std::ifstream f(path);
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string what;
        if (!(ss >> what) || what[0] == '#') continue;
        Step s;
        if (what == "SHOT") s.shot = true;
        else if (what != "WAIT") s.buttons = what;
        if (!(ss >> s.seconds)) s.seconds = what == "WAIT" ? 1.0 : 1.0;
        g_steps.push_back(s);
        n++;
    }
    f.close();
    std::filesystem::remove(path, ec);
    LOGR("testpad: %d steps read from %s", n, path);
}
}  // namespace

void TestPad(int port, input::Pad& p) {
    static const char* path = [] {
        const char* e = std::getenv("FC_PAD_CMD");
        return e && *e ? e : nullptr;
    }();
    if (!path || port != 0) return;
    p.connected = true;
    std::lock_guard<std::mutex> hold(g_lock);
    const auto now = Clock::now();
    if (now >= g_next_check) {
        ReadCommands(path);
        g_next_check = now + std::chrono::milliseconds(250);
    }
    if (now < g_step_end) {
        if (g_holding) Press(g_current.buttons, p);
        return;
    }
    if (g_holding) {  // let go for half a second between presses
        g_holding = false;
        g_step_end = now + std::chrono::milliseconds(500);
        return;
    }
    if (g_steps.empty()) return;
    g_current = g_steps.front();
    g_steps.pop_front();
    auto len = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(g_current.seconds));
    if (g_current.shot) {
#ifdef _WIN32
        app::RequestScreenshot();  // the headless harness has none (it has the snap file)
#endif
        LOGR("testpad: screenshot");
        g_step_end = now + std::chrono::milliseconds(500);
    } else if (g_current.buttons.empty()) {
        LOGR("testpad: wait %.1f s", g_current.seconds);
        g_step_end = now + len;
    } else {
        // WHEELUP, WHEELDOWN, KEY1 to KEY3: a wheel notch or a weapon key (src/game/weapon_keys.cpp), as the step starts
        const std::string& b = g_current.buttons;
        if (b == "WHEELUP" || b == "WHEELDOWN") WeaponWheel(b == "WHEELUP" ? 1 : -1);
        if (b.size() == 4 && b.compare(0, 3, "KEY") == 0) WeaponKey(b[3] - '0');
        LOGR("testpad: %s for %.1f s", g_current.buttons.c_str(), g_current.seconds);
        g_holding = true;
        g_step_end = now + len;
        Press(g_current.buttons, p);
    }
}
}  // namespace fc
