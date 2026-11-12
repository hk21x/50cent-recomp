// The prompts' button pictures: today's Xbox pad (modern_buttons), or while the keyboard and mouse play the keys and
// mouse buttons bound to each control (as True Crime, NightFire and EoN have them).
//
// The pictures are one texture, buttons_Xbox.xtf: 512x64, P8 (swizzled, format 0x0B), two rows of glyphs in 32 texel
// cells, placed in the HUD's and menus' text by a marker. As the disc has them: d-pad arrows (left, up over right,
// down) in two styles, the B X and Y A discs, the L and R trigger plates, the White and Black discs, SELECT and START,
// the L3 and R3 discs, a plus and a ring. When it is bound (GameInfo::texture_bound) they are drawn again, four times
// larger, as a replacement the renderers use in its place (gpu::SetReplacement): A B X Y as coloured letters on dark
// discs, the triggers LT and RT, White and Black as LB and RB (where today's pad has them), Back and Start as View and
// Menu, the d-pads with the arm meant lit white, L3 and R3 as the sticks marked L and R. With the keyboard each shows
// its key (a menu's A Enter and B Backspace), the d-pads the arrow keys. The guest's bytes stay as the disc has them:
// the palette's texture is only replaced, so off, the replacement is just dropped.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "bulletproof.h"
#include "core/input.h"
#include "core/log.h"
#include "gpu/texture.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "strokes.h"

namespace fc {
bool modern_buttons = true;  // the setting (src/common/game_info.cpp)

namespace {

struct Rgba {
    uint8_t r, g, b, a;
};

struct Box {
    int x0, y0, x1, y1;  // inclusive
};

constexpr Colour kDark{30, 30, 30, 255}, kWhite{255, 255, 255, 255}, kEdge{150, 150, 150, 255};
constexpr Colour kCapEdge{200, 200, 200, 255}, kDimEdge{75, 75, 75, 255}, kDimText{95, 95, 95, 255};

// the atlas's glyphs, as the disc has them (their boxes measured on its pixels)
enum Kind { kA, kB, kX, kY, kLB, kRB, kLT, kRT, kView, kMenu, kStick, kPad, kPlus, kRing };
struct Picture {
    Box box;
    Kind kind;
    int lit = 0;  // a d-pad's arm lit: up 1, down 2, left 4, right 8; a stick: 0 the right, 1 the left
};
constexpr int kW = 512, kH = 64;
constexpr uint32_t kFormat = 0x0B;  // P8, swizzled
constexpr uint32_t kBytes = kW * kH;
const Picture kPictures[] = {
    {{2, 2, 30, 30}, kPad, 4},     {{34, 2, 62, 30}, kPad, 1},     {{65, 1, 95, 31}, kPad, 4},   {{97, 1, 127, 31}, kPad, 1},
    {{132, 2, 156, 26}, kB},       {{164, 2, 188, 26}, kX},        {{215, 3, 233, 29}, kLT},     {{279, 7, 297, 24}, kLB},
    {{328, 2, 375, 31}, kView},    {{401, 1, 431, 31}, kStick, 0}, {{449, 1, 461, 13}, kPlus},
    {{2, 34, 30, 62}, kPad, 8},    {{34, 34, 62, 62}, kPad, 2},    {{65, 33, 95, 63}, kPad, 8},  {{97, 33, 127, 63}, kPad, 2},
    {{132, 33, 156, 57}, kY},      {{164, 33, 188, 57}, kA},       {{215, 36, 233, 62}, kRT},    {{279, 40, 297, 57}, kRB},
    {{337, 33, 367, 63}, kStick, 1}, {{398, 33, 437, 63}, kMenu},  {{450, 43, 468, 61}, kRing},
};

struct Square {
    float cx, cy, side;
};
Square Middle(Area a) { return {(a.x0 + a.x1) / 2, (a.y0 + a.y1) / 2, std::min(a.x1 - a.x0, a.y1 - a.y0)}; }
Area Around(Square q, float side) { return {q.cx - side / 2, q.cy - side / 2, q.cx + side / 2, q.cy + side / 2}; }
Area Space(const Picture& p) { return {float(p.box.x0), float(p.box.y0), float(p.box.x1 + 1), float(p.box.y1 + 1)}; }

// a dark disc with a lighter rim
void Disc(Canvas& c, float cx, float cy, float r) {
    float rim = std::max(0.5f, r * 0.1f);
    Area a{cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1};
    Fill(c, a, kEdge, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r); });
    Fill(c, a, kDark, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r - rim); });
}

// A B X Y: the letter in the button's colour on a dark disc
void FaceButton(Canvas& c, Area a, const char* letter, Colour colour) {
    Square q = Middle(a);
    float r = q.side / 2 - 0.5f;
    Disc(c, q.cx, q.cy, r);
    Text(c, letter, q.cx, q.cy, r * 1.1f, std::max(0.6f, r * 0.24f), colour);
}

// LT RT LB RB: a plate with the label, a trigger rounder at the top than a bumper
void Shoulder(Canvas& c, Area plate, bool trigger, const char* label) {
    float w = plate.x1 - plate.x0, h = plate.y1 - plate.y0;
    float top = trigger ? w * 0.3f : h * 0.25f, bottom = trigger ? w * 0.1f : h * 0.25f, rim = std::max(0.5f, w * 0.06f);
    Fill(c, plate, kEdge, [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom); });
    Fill(c, plate, kDark, [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom) + rim; });
    float height = std::min(h * 0.45f, (w - 2 * rim) * 0.8f / TextWidth(label, 1));
    Text(c, label, (plate.x0 + plate.x1) / 2, (plate.y0 + plate.y1) / 2, height, std::max(0.45f, height * 0.19f), kWhite);
}

// Menu (three lines) and View (two overlapping squares) on a dark disc
void Menu(Canvas& c, Area a) {
    Square q = Middle(a);
    float r = q.side / 2 - 0.5f, k = r / 7;
    Disc(c, q.cx, q.cy, r);
    std::vector<Segment> lines;
    for (int i = -1; i <= 1; i++) lines.push_back({{q.cx - 2.6f * k, q.cy + i * 2.8f * k}, {q.cx + 2.6f * k, q.cy + i * 2.8f * k}});
    Fill(c, Around(q, r * 1.5f), kWhite, [&](float x, float y) { return StrokeDistance(x, y, lines, std::max(0.4f, 0.6f * k)); });
}

void View(Canvas& c, Area a) {
    Square q = Middle(a);
    float r = q.side / 2 - 0.5f, k = r / 7;
    Disc(c, q.cx, q.cy, r);
    auto square = [&](float x0, float y0, bool front) {
        Area s{x0, y0, x0 + 4.4f * k, y0 + 4.4f * k};
        std::vector<Segment> edges = {{{s.x0, s.y0}, {s.x1, s.y0}}, {{s.x1, s.y0}, {s.x1, s.y1}},
                                      {{s.x1, s.y1}, {s.x0, s.y1}}, {{s.x0, s.y1}, {s.x0, s.y0}}};
        Area around{s.x0 - 1, s.y0 - 1, s.x1 + 1, s.y1 + 1};
        if (front) Fill(c, around, kDark, [&](float x, float y) { return BoxDistance(x, y, s, 0.5f * k, 0.5f * k); });
        Fill(c, around, kWhite, [&](float x, float y) { return StrokeDistance(x, y, edges, std::max(0.4f, 0.55f * k)); });
    };
    square(q.cx - 3.6f * k, q.cy - 3.6f * k, false);
    square(q.cx - 0.8f * k, q.cy - 0.8f * k, true);
}

// a d-pad: a dark disc with the cross, the arms named lit (up 1, down 2, left 4, right 8) white
void DPad(Canvas& c, Area box, int lit) {
    Square q = Middle(box);
    Area a = Around(q, q.side);
    float k = q.side / 20, rim = std::max(0.5f, 0.8f * k);
    Fill(c, a, kEdge, [&](float x, float y) { return EllipseDistance(x, y, a); });
    Area in{a.x0 + rim, a.y0 + rim, a.x1 - rim, a.y1 - rim};
    Fill(c, a, kDark, [&](float x, float y) { return EllipseDistance(x, y, in); });
    float w = 2.6f * k, l = q.side / 2 - 3.0f * k, round = 0.8f * k;
    Area arms[4] = {{q.cx - w, q.cy - l, q.cx + w, q.cy}, {q.cx - w, q.cy, q.cx + w, q.cy + l},
                    {q.cx - l, q.cy - w, q.cx, q.cy + w}, {q.cx, q.cy - w, q.cx + l, q.cy + w}};
    constexpr Colour kArm{110, 110, 110, 255};
    for (int i = 0; i < 4; i++)
        Fill(c, arms[i], kArm, [&](float x, float y) { return BoxDistance(x, y, arms[i], round, round); });
    for (int i = 0; i < 4; i++) {
        if (!(lit >> i & 1)) continue;
        Area arm = arms[i];  // its outer part, the middle left grey
        if (i == 0) arm.y1 -= w;
        if (i == 1) arm.y0 += w;
        if (i == 2) arm.x1 -= w;
        if (i == 3) arm.x0 += w;
        Fill(c, arm, kWhite, [&](float x, float y) { return BoxDistance(x, y, arm, round, round); });
    }
}

// a thumbstick from above, pressed (L3, R3): a dark cap, its dished top a ring lighter, the stick's letter on it
void Stick(Canvas& c, Area box, const char* letter) {
    Square q = Middle(box);
    Area a = Around(q, q.side);
    float rim = std::max(0.5f, q.side * 0.04f);
    Fill(c, a, kEdge, [&](float x, float y) { return EllipseDistance(x, y, a); });
    Area in{a.x0 + rim, a.y0 + rim, a.x1 - rim, a.y1 - rim};
    Fill(c, a, kDark, [&](float x, float y) { return EllipseDistance(x, y, in); });
    float d = q.side * 0.2f;
    Area dish{a.x0 + d, a.y0 + d, a.x1 - d, a.y1 - d};
    constexpr Colour kRing{80, 80, 80, 255};
    Fill(c, a, kRing, [&](float x, float y) { return std::abs(EllipseDistance(x, y, dish)) - rim * 0.75f; });
    Text(c, letter, q.cx, q.cy, q.side * 0.36f, std::max(0.5f, q.side * 0.07f), kWhite);
}

// the disc's plus and ring, white
void Plus(Canvas& c, Area a) {
    Square q = Middle(a);
    float h = q.side / 2 - 0.5f;
    std::vector<Segment> lines = {{{q.cx - h, q.cy}, {q.cx + h, q.cy}}, {{q.cx, q.cy - h}, {q.cx, q.cy + h}}};
    Fill(c, a, kWhite, [&](float x, float y) { return StrokeDistance(x, y, lines, std::max(0.6f, q.side * 0.1f)); });
}

void Ring(Canvas& c, Area a) {
    Square q = Middle(a);
    float r = q.side / 2 - 1.0f, half = std::max(0.6f, q.side * 0.07f);
    Fill(c, a, kWhite, [&](float x, float y) { return std::abs(CircleDistance(x, y, q.cx, q.cy, r)) - half; });
}

// the keyboard's pictures (NightFire's, as True Crime has them)
constexpr uint16_t kVkLeftButton = 0x01, kVkRightButton = 0x02, kVkMiddleButton = 0x04, kVkLeft = 0x25, kVkUp = 0x26,
                   kVkRight = 0x27, kVkDown = 0x28;

bool MouseButton(uint16_t vk) { return vk == kVkLeftButton || vk == kVkRightButton || vk == kVkMiddleButton; }

// a key's name on its cap, empty for the arrows (drawn as heads) and keys it does not know
std::string KeyLabel(uint16_t vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, char(vk));
    if (vk >= 0x70 && vk <= 0x7B) return "F" + std::to_string(vk - 0x70 + 1);
    switch (vk) {
        case 0x08: return "BKSP";
        case 0x09: return "TAB";
        case 0x0D: return "ENTER";
        case 0x10: case 0xA0: case 0xA1: return "SHIFT";
        case 0x11: case 0xA2: case 0xA3: return "CTRL";
        case 0x12: case 0xA4: case 0xA5: return "ALT";
        case 0x1B: return "ESC";
        case 0x20: return "SPACE";
        case 0x2E: return "DEL";
        default: return "";
    }
}

// a key: a dark cap with a light edge and its name, or an arrow's head; dim when not lit
void KeyCap(Canvas& c, Area cap, uint16_t vk, bool lit = true) {
    float w = cap.x1 - cap.x0, h = cap.y1 - cap.y0, r = std::min(w, h) * 0.2f, edge = std::max(0.5f, std::min(w, h) * 0.07f);
    Fill(c, cap, lit ? kCapEdge : kDimEdge, [&](float x, float y) { return BoxDistance(x, y, cap, r, r); });
    Fill(c, cap, kDark, [&](float x, float y) { return BoxDistance(x, y, cap, r, r) + edge; });
    Colour ink = lit ? kWhite : kDimText;
    float cx = (cap.x0 + cap.x1) / 2, cy = (cap.y0 + cap.y1) / 2;
    if (vk >= kVkLeft && vk <= kVkDown) {
        float k = std::min(w, h) * 0.22f;
        std::vector<Pt> head;
        if (vk == kVkUp) head = {{cx, cy - k}, {cx + k, cy + k * 0.7f}, {cx - k, cy + k * 0.7f}};
        if (vk == kVkDown) head = {{cx, cy + k}, {cx - k, cy - k * 0.7f}, {cx + k, cy - k * 0.7f}};
        if (vk == kVkLeft) head = {{cx - k, cy}, {cx + k * 0.7f, cy - k}, {cx + k * 0.7f, cy + k}};
        if (vk == kVkRight) head = {{cx + k, cy}, {cx - k * 0.7f, cy + k}, {cx - k * 0.7f, cy - k}};
        Fill(c, cap, ink, [&](float x, float y) { return PolygonDistance(x, y, head); });
        return;
    }
    std::string label = KeyLabel(vk);
    if (label.empty()) return;
    float height = std::min(h * 0.5f, (w - 2 * edge - std::max(1.0f, w * 0.12f)) / TextWidth(label.c_str(), 1));
    Text(c, label.c_str(), cx, cy, height, std::max(0.45f, height * 0.2f), ink);
}

// a mouse from above, the button pressed lit (none: just the mouse, for looking about)
void Mouse(Canvas& c, Area a, uint16_t vk) {
    float h = (a.y1 - a.y0) * 0.92f, w = std::min(h * 0.64f, (a.x1 - a.x0) * 0.92f);
    float cx = (a.x0 + a.x1) / 2, cy = (a.y0 + a.y1) / 2;
    Area body{cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
    float r = w / 2, edge = std::max(0.5f, w * 0.08f), split = body.y0 + h * 0.42f;
    Fill(c, body, kCapEdge, [&](float x, float y) { return BoxDistance(x, y, body, r, r * 0.9f); });
    Fill(c, body, kDark, [&](float x, float y) { return BoxDistance(x, y, body, r, r * 0.9f) + edge; });
    auto inside = [&](float x, float y) { return BoxDistance(x, y, body, r, r * 0.9f) + edge * 1.8f; };
    float gap = edge * 0.8f;
    if (vk == kVkLeftButton)
        Fill(c, body, kWhite, [&](float x, float y) { return std::max({inside(x, y), x - (cx - gap), y - (split - gap)}); });
    if (vk == kVkRightButton)
        Fill(c, body, kWhite, [&](float x, float y) { return std::max({inside(x, y), (cx + gap) - x, y - (split - gap)}); });
    std::vector<Segment> lines = {{{cx, body.y0 + edge}, {cx, split}}, {{body.x0 + edge, split}, {body.x1 - edge, split}}};
    Fill(c, body, kCapEdge,
         [&](float x, float y) { return std::max(StrokeDistance(x, y, lines, edge * 0.5f), -inside(x, y) - edge); });
    Area wheel{cx - w * 0.09f, body.y0 + h * 0.13f, cx + w * 0.09f, body.y0 + h * 0.31f};
    Fill(c, body, vk == kVkMiddleButton ? kWhite : kDimText,
         [&](float x, float y) { return BoxDistance(x, y, wheel, w * 0.09f, w * 0.09f); });
}

// what the pictures show: a modern pad's, or the keys and mouse buttons bound to each control (neither: the disc's)
struct Look {
    bool modern = false, keys = false;
    std::array<std::array<uint16_t, 2>, input::kControls> bound{};
    bool operator==(const Look&) const = default;
};

Look Current() {
    static const bool force = [] {  // FC_KEY_PICTURES=1: the keys' pictures headless, where no keyboard plays
        const char* e = std::getenv("FC_KEY_PICTURES");
        return e && *e == '1';
    }();
    // FC_NO_BUTTON_PICTURES=1: the disc's pictures whatever plays (a diagnostic); FC_PAD_CMD's presses are a pad's,
    // so with it the pad's pictures show although no controller is plugged in (src/common/test_pad.cpp)
    static const bool off = [] {
        const char* e = std::getenv("FC_NO_BUTTON_PICTURES");
        return e && *e == '1';
    }();
    static const bool test_pad = [] {
        const char* e = std::getenv("FC_PAD_CMD");
        return e && *e;
    }();
    Look l;
    if (off) return l;
    l.keys = force || (!test_pad && input::UsingKeyboard());
    if (!l.keys) {
        l.modern = modern_buttons;
        return l;
    }
    for (int c = 0; c < input::kControls; c++) l.bound[size_t(c)] = input::ControlKeys(c);
    return l;
}

// the key a control is played with (its first binding), in the space `a`
void BoundKey(Canvas& c, Area a, const std::array<uint16_t, 2>& keys) {
    uint16_t vk = keys[0] ? keys[0] : keys[1];
    if (MouseButton(vk)) return Mouse(c, a, vk);
    float w = a.x1 - a.x0, h = a.y1 - a.y0, side = std::min(w, h);
    std::string label = KeyLabel(vk);
    float cw = label.size() > 1 ? w : side;  // a long name gets the whole width
    float cy = (a.y0 + a.y1) / 2, ch = std::min(side, cw);
    float x0 = a.x0 + (w - cw) / 2;
    KeyCap(c, {x0, cy - ch / 2, x0 + cw, cy + ch / 2}, vk);
}

void DrawKeys(Canvas& c, const Look& look) {
    for (const Picture& p : kPictures) {
        Area a = Space(p);
        auto key = [&](int control) { BoundKey(c, a, look.bound[size_t(control)]); };
        switch (p.kind) {
            case kA: key(input::kA); break;
            case kB: key(input::kB); break;
            case kX: key(input::kX); break;
            case kY: key(input::kY); break;
            case kLB: key(input::kWhite); break;
            case kRB: key(input::kBlack); break;
            case kLT: key(input::kLT); break;
            case kRT: key(input::kRT); break;
            case kView: key(input::kBack); break;
            case kMenu: key(input::kStart); break;
            case kStick: key(p.lit ? input::kLThumb : input::kRThumb); break;
            case kPad: key(p.lit == 1 ? input::kUp : p.lit == 2 ? input::kDown : p.lit == 4 ? input::kLeft : input::kRight); break;
            case kPlus: Plus(c, a); break;
            case kRing: Ring(c, a); break;
        }
    }
}

void DrawModern(Canvas& c) {
    for (const Picture& p : kPictures) {
        Area a = Space(p);
        float w = a.x1 - a.x0, h = a.y1 - a.y0;
        Area trigger{a.x0 + w * 0.06f, a.y0 + h * 0.08f, a.x1 - 0.5f, a.y1 - h * 0.08f};
        float bh = std::min(h * 0.75f, w * 0.75f), cy = (a.y0 + a.y1) / 2;
        Area bumper{a.x0 + 0.5f, cy - bh / 2, a.x1 - 0.5f, cy + bh / 2};
        switch (p.kind) {
            case kA: FaceButton(c, a, "A", {100, 200, 60, 255}); break;
            case kB: FaceButton(c, a, "B", {235, 60, 60, 255}); break;
            case kX: FaceButton(c, a, "X", {50, 140, 245, 255}); break;
            case kY: FaceButton(c, a, "Y", {245, 200, 40, 255}); break;
            case kLB: Shoulder(c, bumper, false, "LB"); break;
            case kRB: Shoulder(c, bumper, false, "RB"); break;
            case kLT: Shoulder(c, trigger, true, "LT"); break;
            case kRT: Shoulder(c, trigger, true, "RT"); break;
            case kView: View(c, a); break;
            case kMenu: Menu(c, a); break;
            case kStick: Stick(c, a, p.lit ? "L" : "R"); break;
            case kPad: DPad(c, a, p.lit); break;
            case kPlus: Plus(c, a); break;
            case kRing: Ring(c, a); break;
        }
    }
}

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

Rgba Straight(const std::array<float, 4>& p) {
    if (p[3] <= 0) return {0, 0, 0, 0};
    auto ch = [&](float v) { return uint8_t(std::clamp(v / p[3] * 255 + 0.5f, 0.0f, 255.0f)); };
    return {ch(p[0]), ch(p[1]), ch(p[2]), uint8_t(std::clamp(p[3] * 255 + 0.5f, 0.0f, 255.0f))};
}

constexpr int kSharp = 4;

// a look drawn four times larger, B8G8R8A8 rows
std::vector<uint32_t> Draw(const Look& look) {
    Canvas c;
    c.scale = kSharp;
    c.size = kW * kSharp;
    c.px.assign(size_t(c.size) * c.size, {0, 0, 0, 0});  // square: the canvas's rows past kH stay unused
    look.modern ? DrawModern(c) : DrawKeys(c, look);
    std::vector<uint32_t> out(size_t(kW * kSharp) * (kH * kSharp), 0);
    for (int y = 0; y < kH * kSharp; y++)
        for (int x = 0; x < kW * kSharp; x++) {
            Rgba q = Straight(c.px[size_t(y) * c.size + x]);
            out[size_t(y) * kW * kSharp + x] = uint32_t(q.b) | uint32_t(q.g) << 8 | uint32_t(q.r) << 16 | uint32_t(q.a) << 24;
        }
    return out;
}

// the atlas where it was last seen, and the look it has
struct Atlas {
    uint32_t data = 0;
    Look drawn;  // the disc's at first
    struct Drawing {
        Look look;
        std::vector<uint32_t> sharp;
    };
    std::vector<Drawing> cache;  // the newest last
};
std::unique_ptr<Atlas> g_atlas;
std::mutex g_lock;
// the last 512x64 P8 texture found not to be the atlas, and its first bytes' hash then: hashed whole again only once
// those change
uint32_t g_rejected = 0;
uint64_t g_rejected_head = 0;

void Apply(Atlas& a, const Look& look) {
    if (!look.modern && !look.keys) {
        gpu::ClearReplacement(a.data);
    } else {
        auto d = std::find_if(a.cache.begin(), a.cache.end(), [&](const Atlas::Drawing& x) { return x.look == look; });
        if (d == a.cache.end()) {
            a.cache.push_back({look, Draw(look)});
            if (a.cache.size() > 4) a.cache.erase(a.cache.begin());
            d = a.cache.end() - 1;
        }
        gpu::SetReplacement(a.data, kBytes, kW * kSharp, kH * kSharp, d->sharp);
        // FC_BUTTONS_DUMP=dir (a diagnostic): the look as drawn, four times larger, a PAM over grey (the reference
        // renderer does not draw replacements, so this is how it is seen headless)
        if (const char* dir = std::getenv("FC_BUTTONS_DUMP"); dir && *dir) {
            std::string path = std::string(dir) + (look.keys ? "/buttons_keys.pam" : "/buttons_modern.pam");
            if (FILE* f = std::fopen(path.c_str(), "wb")) {
                std::fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 3\nMAXVAL 255\nTUPLTYPE RGB\nENDHDR\n", kW * kSharp, kH * kSharp);
                for (uint32_t q : d->sharp) {
                    const uint32_t al = q >> 24;
                    const uint8_t px[3] = {uint8_t(((q >> 16 & 255) * al + 128 * (255 - al)) / 255),
                                           uint8_t(((q >> 8 & 255) * al + 128 * (255 - al)) / 255),
                                           uint8_t(((q & 255) * al + 128 * (255 - al)) / 255)};
                    std::fwrite(px, 1, 3, f);
                }
                std::fclose(f);
            }
        }
    }
    a.drawn = look;
    LOGR("bulletproof: the button pictures (%08X) drawn as %s", a.data,
         look.modern ? "a modern pad's" : look.keys ? "the keys and mouse" : "the disc has them");
}
}  // namespace

// GameInfo::texture_bound: D3DBaseTexture Data (+4, physical) and Format (+12: the format in bits 8-15, a swizzled
// texture's log2 width in bits 20-23 and height in 24-27)
bool ButtonPictures(uint32_t header, uint32_t /*palette*/) {
    const uint32_t format = kernel::R32(header + 12);
    if ((format >> 8 & 0xFF) != kFormat || (format >> 20 & 15) != 9 || (format >> 24 & 15) != 6) return false;
    const uint32_t data = mem::VirtualFromPhysical(kernel::R32(header + 4));
    std::lock_guard<std::mutex> hold(g_lock);
    Look look = Current();
    if (g_atlas && g_atlas->data == data) {
        if (look == g_atlas->drawn) return false;
        Apply(*g_atlas, look);
        return true;
    }
    if (!mem::IsCommitted(data) || !mem::IsCommitted(data + kBytes - 1)) return false;
    // another 512x64 P8 texture, or the atlas moved: only the disc's bytes are taken for it
    const auto* bytes = static_cast<const uint8_t*>(kernel::HostPtr(data));
    const uint64_t head = Fnv(bytes, 256);
    if (data == g_rejected && head == g_rejected_head) return false;
    const uint64_t hash = Fnv(bytes, kBytes);
    if (hash != kButtonsDisc) {
        g_rejected = data;
        g_rejected_head = head;
        LOG_FIRST(8, "bulletproof: a 512x64 P8 texture at %08X, FNV-1a %016llX, not the button pictures", data,
                  static_cast<unsigned long long>(hash));
        return false;
    }
    if (g_atlas && g_atlas->data != data) gpu::ClearReplacement(g_atlas->data);
    g_atlas = std::make_unique<Atlas>();
    g_atlas->data = data;
    if (!look.modern && !look.keys) return false;
    Apply(*g_atlas, look);
    return true;
}
}  // namespace fc
