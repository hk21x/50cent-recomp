// The mouse pointer in the PDA's menus (the title's and the pause menu's): the row under it is highlighted as the d-pad
// would, and the left button selects it (src/common/menu_mouse.cpp).
//
// The PDA's menus belong to one menu manager, 0x32A170 (0x1F240 sets it up: 0x4A7A0, its layer "menuLayer" at +0): a
// stack of menus (+0xC the count, +0x14 the array), the top one on screen. Each frame 0x49F00 (ecx the manager) runs
// the top menu's update (0x498A0, inlined there: ecx the menu). A menu: +0 its first item, +4 the highlighted one, +8
// the manager, +0xC its record (+0 the layout node's hash, +4 (16 bit) the item count, +0x28 the items' names). The
// manager's map at +0x18 (0x495C0: +0xC the sorted index, +0x10 the {item, hash} entries, +0x18 the count) finds an
// item by its name's hash (0x110700: FNV-1, a-z as A-Z). An item: +0 its vtable (+0xC highlight, +0x10 unhighlight),
// +4 its record (+0x14 its layout node's hash, +0x60 and +0x80 the names of the items above and below), +0x10 1 when
// the d-pad skips it. The d-pad's move (0x49B19): old->unhighlight, menu+4 = new, new->highlight, 0x489D0 (ecx the
// menu: the highlight bar), 0x48BC0 (ecx the manager; 8, 1.0: the sound); done here the same way.
//
// A layout node (0x2CF90: the layer's map at +0xBC, hashed as above): +0x10 its 4x4 matrix (rows; the translation at
// +0x40), +0x84 its parent. The menus' camera puts the PDA's rows at y = 240.75 - 771 * the node's height (480 lines)
// and the highlight bar from x 144 to 494 (640 wide): measured headless on the title's menu, whose rows' bars are 21.75
// lines apart, 0.0282 apart in the layout.
//
// A list box (the saves, the profiles, messages) is an item whose node is the list: +0x1B8 the highlighted slot, +0x1BA
// the entries, +0x1BC the visible slots, +0xA0 each slot's entry (-1 none), +0x1C0 its look (+0x28 the rows' spacing),
// +0x190 a node placed on the highlighted slot (the others are drawn a spacing apart from it). Its d-pad (0x4A590)
// steps with 0x54B20 (on) and 0x54A10 (back), ecx the list's node, with its sounds; the pointer does the same.
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
#include "common/bulletproof.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace fc {
namespace {
constexpr uint32_t kManager = 0x32A170;  // the PDA's menus
constexpr uint32_t kHighlightBar = 0x489D0, kPlaySound = 0x48BC0;
constexpr float kRowsY0 = 240.75f, kRowsScale = 771.0f;  // a node's height to the 480 lines
// the rows' extent across, 640 wide: the title's highlight bar runs from 144 to 494, the Options pages' from 94 to 555
// (GPU-VM shots); the wider is taken for all, the PDA's screen being that wide in each
constexpr float kBarLeft = 94, kBarRight = 555;
// a slider's value column (the Camera page's sensitivity bars, 421 to 551; the volumes' numbers from 425): its left
// half lowers, its right half raises
constexpr float kValueLeft = 415, kValueMiddle = 486;
constexpr int kMaxItems = 32;
// the items' kinds by vtable (their pools, 0x48C10 to 0x490F0, set them): a list box's or a text box's rows are its
// own, not placed here
constexpr uint32_t kListBox = 0x2B93EC, kTextBox = 0x2B9404;
// a slider (the sensitivities, the volumes): its input (0x4A260) takes left (flag 1) and right (2), moving its node's
// value (+0xA0) by its record's step (+0xC4)
constexpr uint32_t kSlider = 0x2B93A4;
constexpr int kListSlots = 16;

uint32_t R(uint32_t va) { return mem::IsCommitted(va) ? kernel::R32(va) : 0; }
float RF(uint32_t va) {
    const uint32_t v = R(va);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

// 0x110700
uint32_t Hash(uint32_t name) {
    uint32_t h = 0x811C9DC5;
    for (int i = 0; i < 64 && mem::IsCommitted(name + i); i++) {
        uint32_t c = kernel::G<uint8_t>(name + i);
        if (!c) break;
        if (c >= 'a' && c <= 'z') c -= 0x20;
        h = h * 0x1000193 ^ c;
    }
    return h;
}

// 0x2A280's sorted map: the entry's first word for `hash`, 0 when none
uint32_t Find(uint32_t map, uint32_t hash) {
    const uint32_t index = R(map + 0xC), entries = R(map + 0x10);
    int lo = 0, hi = int(R(map + 0x18)) - 1;
    if (!index || !entries || hi > 4096) return 0;
    while (lo <= hi) {
        const int mid = (lo + hi) >> 1;
        const uint32_t e = R(index + 4 * mid), h = R(entries + 8 * e + 4);
        if (h == hash) return R(entries + 8 * e);
        if (h < hash) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

// a layout node's place: its matrix times its parents'
bool NodePlace(uint32_t node, float& x, float& y) {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int depth = 0; node && depth < 16; depth++, node = R(node + 0x84)) {
        float p[16], r[16];
        for (int i = 0; i < 16; i++) p[i] = RF(node + 0x10 + 4 * i);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) r[i * 4 + j] = m[i * 4] * p[j] + m[i * 4 + 1] * p[4 + j] + m[i * 4 + 2] * p[8 + j] + m[i * 4 + 3] * p[12 + j];
        std::memcpy(m, r, sizeof m);
    }
    x = m[12], y = m[13];
    return std::isfinite(x) && std::isfinite(y);
}
bool NodeHeight(uint32_t node, float& y) {
    float x;
    return NodePlace(node, x, y);
}
// a node's x to the menus' 640 (the camera's scale across too), times `spread` (see ShopSpread)
float ScreenX(float x, float spread = 1) { return 320 + kRowsScale * x * spread; }
float ScreenY(float y) { return kRowsY0 - kRowsScale * y; }

// a node shown: it and each of its parents (+0x7C, set by the vtable's +0x28)
bool NodeShown(uint32_t node) {
    for (int depth = 0; node && depth < 16; depth++, node = R(node + 0x84))
        if (!kernel::G<uint8_t>(node + 0x7C)) return false;
    return true;
}

// a layout node by its path (0x2CF90's hash) in a layer's map
uint32_t NodeByPath(uint32_t layer, const char* path) {
    uint32_t h = 0x811C9DC5;
    for (const char* c = path; *c; c++) h = h * 0x1000193 ^ uint32_t(*c >= 'a' && *c <= 'z' ? *c - 0x20 : *c);
    return Find(layer + 0xBC, h);
}

// The shops (0x55770 sets up their layers and managers; the shop state 0x3315D8 says which is open, and 0x55900 runs
// that one's manager's frame in the gameplay frame): the crib's TV (2), the stash (4), the gun shop ("WeapShop", 5;
// Grizz's van), Doc's ("Doc", 0xF) and the bootlegger's ("Bootlegger", 0x12; Whoo Kid's music and videos), the
// disarms (0xC). Their layers are drawn by the menus' camera (the bootlegger's list and buttons measured headless at
// the PDA's scale, across as down). Each has a row of buttons ("buttonGrp": its icon at the group, its label beside
// it): Back (B) and the third action (Y) at the left, the action (A) and the second (X) at the right, the label's
// text running from its node away from the icon.
constexpr uint32_t kShopState = 0x3315D8;
uint32_t ShopManager() {
    switch (R(kShopState)) {
    case 2: return 0x331B08;
    case 4: return 0x331608;
    case 5: case 0xF: case 0x12: return 0x3316E8;
    case 0xC: return 0x3317E0;
    }
    return 0;
}
// A shop's layer is laid out for the game's widescreen mode, 16:9 from the frame's height at any width (as the HUD:
// on a wider frame its pieces huddled in a 16:9 middle): its nodes' x, measured headless as fractions of a 16:9 frame
// drawn into 720x480 (where the test's pointer is in the same fractions), spread by 16:9 over 4:3 in the pointer's
// own space on Windows, which takes the menus as 4:3 in the middle (src/common/menu_mouse.cpp). Harry's shot of the
// gun shop at 3440x1440 agrees: its arrows' distance from the edges there is that of 16:9 kept by hud_layout.
float ShopSpread() { return PointerForced() || !app::Widescreen() ? 1.0f : (16.0f / 9.0f) / (4.0f / 3.0f); }
constexpr float kListHalfWidth = 138;  // the bootlegger's list: 290 to 565 (640) about its node
constexpr float kItemHalfWidth = 60;   // a shop's other items about their nodes

struct Button {
    const char* group;
    const char* label;
    int control;
    bool left;  // icon at the left, the label's text to its right
};
constexpr Button kButtons[] = {
    {"buttonGrp/backBtnGrp", "buttonGrp/backBtnGrp/backLabel", input::kB, true},
    {"buttonGrp/actionBtnGrp", "buttonGrp/actionBtnGrp/actionLabel", input::kA, false},
    {"buttonGrp/action2BtnGrp", "buttonGrp/action2BtnGrp/action2Label", input::kX, false},
    {"buttonGrp/action3BtnGrp", "buttonGrp/action3BtnGrp/action3Label", input::kY, true},
};
constexpr float kIconHalf = 14, kLabelText = 90, kButtonHalfHeight = 11;
// The gun shop (Grizz's van) is a picker in the world: its 23 items are buttons, one a weapon, with no place on the
// screen (their nodes are the van's); the d-pad's left and right step between them and the camera follows. Its layer
// has arrows either side of the ammo's name (measured headless: 181.5 and 458.5 across, 382 down): each plays its
// d-pad direction when clicked, and the wheel steps left and right there.
struct Arrow {
    const char* node;
    int control;
};
constexpr Arrow kArrows[] = {{"shopTopGrp/leftArrow", input::kLeft}, {"shopTopGrp/rightArrow", input::kRight}};
constexpr float kArrowHalf = 16;

// a __thiscall guest function (CallGuest puts every register and the stack back)
void CallThis(X86Context& ctx, uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args) {
    if (!fn) return;
    const uint32_t ecx = ctx.ecx;
    ctx.ecx = self;
    kernel::CallGuest(ctx, fn, args);
    ctx.ecx = ecx;
}

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// the top menu's rows as its last frame placed them: the click (the pad's thread) tests the pointer where it is now
struct Rows {
    bool placed = false;  // a menu of rows: a list's, or two or more, no text box (else the click plays A anywhere)
    int n = 0, current = -1;
    uint32_t items[kMaxItems];
    int slots[kMaxItems];  // a list box's visible slot, -1 for an item's own row
    bool slider[kMaxItems];
    float ys[kMaxItems];
    float lefts[kMaxItems], rights[kMaxItems];  // each row's extent across
    struct Box {
        int control;
        float x0, x1, y0, y1;
    } buttons[6];
    int nbuttons = 0;
    bool shop = false;    // a shop's menu: off its rows and buttons a click does nothing (no A anywhere)
    bool arrows = false;  // its left and right arrows shown: the wheel steps across
    int64_t at = 0;  // ms
};
std::mutex g_lock;
Rows g_rows;

// the row at (x, y), -1 for none
int RowAt(const Rows& r, float x, float y) {
    int over = -1;
    for (int i = 0; i < r.n; i++) {
        if (x < r.lefts[i] || x > r.rights[i]) continue;
        float half = 11;  // half the rows' spacing, from the nearest row
        for (int j = 0; j < r.n; j++)
            if (j != i && std::fabs(r.ys[j] - r.ys[i]) > 1) half = std::min(half, std::fabs(r.ys[j] - r.ys[i]) / 2);
        if (std::fabs(y - r.ys[i]) <= half) over = i;
    }
    return over;
}

// a list box item's highlighted slot (+0x1B8 of its node)
int Cursor(X86Context&, uint32_t manager, uint32_t item) {
    const uint32_t box = Find(R(manager) + 0xBC, R(R(item + 4) + 0x14));
    return box ? int16_t(kernel::G<uint16_t>(box + 0x1B8)) : -1;
}

// the top menu's frame: its rows placed, the row under the pointer highlighted
void Track(X86Context& ctx, uint32_t manager) {
    const uint32_t count = R(manager + 0xC), stack = R(manager + 0x14);
    if (!count || count > 64 || !stack) return;
    const uint32_t menu = R(stack + 4 * (count - 1)), record = R(menu + 0xC), layer = R(manager);
    if (!menu || !record || !layer) return;
    // the PDA's menus, and the dialogues over them ("Are you sure...?": a manager of their own on the heap, its top
    // menu named "Dialogue", its layer drawn by the same camera); while a dialogue is up, the menu under it is not
    // the pointer's
    static int64_t dialogue_at = 0;
    const bool dialogue = mem::IsCommitted(record + 6) && std::strncmp(static_cast<const char*>(kernel::HostPtr(record + 6)), "Dialogue", 9) == 0;
    const bool shop = manager == ShopManager();
    const float spread = shop ? ShopSpread() : 1;
    if (!dialogue && ((manager != kManager && !shop) || NowMs() - dialogue_at < 300)) return;
    Rows r;
    bool text = false, list = false;
    const int n = std::min<int>(kernel::G<uint16_t>(record + 4), kMaxItems);
    for (int i = 0; i < n && r.n < kMaxItems; i++) {
        const uint32_t item = Find(manager + 0x18, Hash(R(record + 0x28 + 4 * i)));
        if (item && R(item) == kTextBox) text = true;
        if (!item || R(item + 0x10) == 1) continue;
        const uint32_t node = Find(layer + 0xBC, R(R(item + 4) + 0x14));
        float y;
        if (!node) continue;
        float nx, ny;
        const float centre = shop && NodePlace(node, nx, ny) ? ScreenX(nx, spread) : 0;
        if (R(item) == kListBox) {
            // its visible slots: +0x1BC of them, +0xA0 each one's entry (-1 none), +0x1B8 the highlighted one; the node at
            // +0x190 is placed on the highlighted slot, the others a row's spacing (+0x28 of its look, +0x1C0) apart
            // a list across (its look's +0x3C clear: left and right step it) has its slots a spacing apart to the right
            list = true;
            const int visible = std::min<int>(int16_t(kernel::G<uint16_t>(node + 0x1BC)), kListSlots);
            const uint32_t entries = R(node + 0xA0), look = R(node + 0x1C0);
            const float spacing = RF(look + 0x28);
            const bool across = kernel::G<uint8_t>(look + 0x3C) == 0;
            float hx, hy;
            if (!entries || !std::isfinite(spacing) || spacing <= 0 || spacing > 0.3f || !NodePlace(R(node + 0x190), hx, hy))
                continue;
            const int at = int16_t(kernel::G<uint16_t>(node + 0x1B8));
            for (int k = 0; k < visible && r.n < kMaxItems; k++) {
                if (int16_t(kernel::G<uint16_t>(entries + 2 * k)) < 0) continue;
                r.items[r.n] = item, r.slots[r.n] = k, r.slider[r.n] = false;
                if (across) {
                    const float x = ScreenX(hx + spacing * (k - at), spread), half = kRowsScale * spacing * spread / 2;
                    r.lefts[r.n] = x - half, r.rights[r.n] = x + half;
                    r.ys[r.n++] = ScreenY(hy);
                } else {
                    r.lefts[r.n] = shop ? centre - kListHalfWidth * spread : kBarLeft;
                    r.rights[r.n] = shop ? centre + kListHalfWidth * spread : kBarRight;
                    r.ys[r.n++] = ScreenY(hy - spacing * (k - at));
                }
            }
            continue;
        }
        if (!NodeHeight(node, y)) continue;
        r.items[r.n] = item, r.slots[r.n] = -1, r.slider[r.n] = R(item) == kSlider;
        r.lefts[r.n] = shop ? centre - kItemHalfWidth * spread : kBarLeft;
        r.rights[r.n] = shop ? centre + kItemHalfWidth * spread : kBarRight;
        r.ys[r.n++] = kRowsY0 - kRowsScale * y;
    }
    // a shop's buttons shown: each a box from its icon across its label's text
    for (const Button& b : kButtons) {
        const uint32_t group = shop ? NodeByPath(layer, b.group) : 0, label = group ? NodeByPath(layer, b.label) : 0;
        float gx, gy, lx, ly;
        if (!label || !NodeShown(group) || !NodePlace(group, gx, gy) || !NodePlace(label, lx, ly)) continue;
        const float icon = ScreenX(gx, spread), text = ScreenX(lx, spread), y = ScreenY(gy);
        r.buttons[r.nbuttons++] = {b.control, b.left ? icon - kIconHalf * spread : text - kLabelText * spread,
                                   b.left ? text + kLabelText * spread : icon + kIconHalf * spread,
                                   y - kButtonHalfHeight, y + kButtonHalfHeight};
    }
    for (const Arrow& a : kArrows) {
        const uint32_t node = shop ? NodeByPath(layer, a.node) : 0;
        float ax, ay;
        if (!node || !NodeShown(node) || !NodePlace(node, ax, ay)) continue;
        const float x = ScreenX(ax, spread), y = ScreenY(ay);
        r.buttons[r.nbuttons++] = {a.control, x - kArrowHalf * spread, x + kArrowHalf * spread, y - kArrowHalf, y + kArrowHalf};
        r.arrows = true;
    }
    r.shop = shop;
    r.placed = !text && (list ? r.n >= 1 : r.n >= 2);
    if (dialogue) {
        if (!r.placed) return;  // one not on screen (no rows placed) leaves the menu under it alone
        dialogue_at = NowMs();
    }
    static uint32_t said = 0;
    if (menu != said) {
        said = menu;
        const char* name = static_cast<const char*>(kernel::HostPtr(record + 6));
        LOGR("bulletproof: %s \"%.24s\": %d of %d rows placed%s, %d buttons", shop ? "shop" : "menu",
             mem::IsCommitted(record + 6) ? name : "?", r.n, n,
             r.placed ? "" : " (not a menu of rows: the click plays A anywhere)", r.nbuttons);
        for (int i = 0; i < r.n; i++) LOGI("bulletproof:   row %d at %.1f, %.0f to %.0f", i, r.ys[i], r.lefts[i], r.rights[i]);
        for (int i = 0; i < r.nbuttons; i++)
            LOGR("bulletproof:   button %d: %.0f to %.0f, %.0f to %.0f", r.buttons[i].control, r.buttons[i].x0,
                 r.buttons[i].x1, r.buttons[i].y0, r.buttons[i].y1);
    }
    // the pointer highlights a row as it moves onto it; standing still it leaves the keys' and the wheel's choice alone
    float px, py;
    static float last_x = -1, last_y = -1;
    if (r.placed && PointerAt(px, py)) {
        // the first place seen is only where it starts: a pointer left standing over the window moves nothing
        const bool moved = last_x >= 0 && std::fabs(px - last_x) + std::fabs(py - last_y) > 0.5f;
        last_x = px, last_y = py;
        const int over = RowAt(r, px, py);
        const uint32_t current = R(menu + 4);
        if (over >= 0 && moved && (r.items[over] != current || (r.slots[over] >= 0 && r.slots[over] != Cursor(ctx, manager, r.items[over])))) {
            const uint32_t item = r.items[over];
            LOGI("bulletproof: the pointer highlights row %d", over);
            if (item != current) {
                if (current) CallThis(ctx, R(R(current) + 0x10), current, {});
                kernel::W32(menu + 4, item);
                CallThis(ctx, R(R(item) + 0xC), item, {});
                CallThis(ctx, kHighlightBar, menu, {});
            }
            if (r.slots[over] >= 0) {
                // a list's slot: each step as the d-pad's, through the item's input (its vtable's +8, 0x4A590: the pad's
                // bits 1 left, 2 right, 4 up, 8 down; a list down the screen, its look's +0x3C set, takes up and down).
                // That gives the item's callback the move first: the bootlegger's steps the list itself and shows the
                // chosen entry (0xE5F30: 0x54A10 / 0x54B20, then 0xE47F0); otherwise the list steps (0x54B20 on, 0x54A10
                // back) with its sounds. Stepping the list alone left the shop's panel on the entry before.
                const uint32_t box = Find(layer + 0xBC, R(R(item + 4) + 0x14));
                const bool across = kernel::G<uint8_t>(R(box + 0x1C0) + 0x3C) == 0;
                static uint32_t flags = 0;
                if (!flags) flags = mem::AllocSystem(16);
                for (int guard = 0; flags && guard < kListSlots; guard++) {
                    const int at = int16_t(kernel::G<uint16_t>(box + 0x1B8));
                    if (at == r.slots[over]) break;
                    const bool on = at < r.slots[over];
                    kernel::W32(flags, across ? (on ? 2u : 1u) : (on ? 8u : 4u));
                    CallThis(ctx, R(R(item) + 8), item, {flags});
                    if (int16_t(kernel::G<uint16_t>(box + 0x1B8)) == at) break;  // it did not move: no further
                }
            } else {
                CallThis(ctx, kPlaySound, manager, {8, 0x3F800000});
            }
        }
    }
    for (int i = 0; i < r.n; i++)
        if (r.items[i] == R(menu + 4) && (r.slots[i] < 0 || r.slots[i] == Cursor(ctx, manager, r.items[i]))) r.current = i;
    if (shop) {
        static uint32_t was_item = 0;
        const uint32_t now_item = R(menu + 4);
        if (now_item != was_item && now_item) {
            for (int i = 0; i < n; i++) {
                const uint32_t nm = R(record + 0x28 + 4 * i);
                if (Find(manager + 0x18, Hash(nm)) == now_item && mem::IsCommitted(nm))
                    LOGI("bulletproof: the shop's choice now %.24s", static_cast<const char*>(kernel::HostPtr(nm)));
            }
        }
        was_item = now_item;
    }
    r.at = NowMs();
    std::lock_guard<std::mutex> lock(g_lock);
    g_rows = r;
}
}  // namespace

// Tests: FC_EXIT_LEVEL=N leaves the level as the scripts' ExitLevel(N) does (0xCB680; 12 EXIT_HUB_VAN, Grizz's van,
// 9 EXIT_HUB_DOC), 20 s into play, the player's health held after; FC_SHOP_OPEN=<function> calls a shop's Lua opener once, FC_SHOP_DELAY seconds into
// play (20 unless given) with no shop open (0x833F0 StartBootLeggerInteraction, 0x55EE0 StartWeaponPurchase, 0x560F0
// StartMedicineInteraction), or under 0x100 sets that shop state (0x11 the bootlegger's, past its greeting).
void ShopTest(X86Context& ctx) {
    static const char* exit_level = std::getenv("FC_EXIT_LEVEL");
    static const char* want = std::getenv("FC_SHOP_OPEN");
    static const char* delay = std::getenv("FC_SHOP_DELAY");
    static int64_t first = 0;
    static bool exited = false, done = false;
    if (!exit_level && !want) return;
    if (!first) first = NowMs();
    if (exit_level && !exited && NowMs() - first >= 20000) {
        exited = true;
        const uint32_t code = uint32_t(std::strtoul(exit_level, nullptr, 0));
        kernel::CallGuest(ctx, 0x25FF0, {code, 0xFFFFFFFFu});
        kernel::W32(0x325318, 0xB);
        kernel::CallGuest(ctx, 0x34B60, {});
        LOGR("bulletproof: FC_EXIT_LEVEL %u", code);
    }
    // the player's health (+0x504 of 0x32DCFC) held from then on: the level left this way does not keep its player
    // alive for long (the mission failed screen six seconds into the van)
    static float health = 0;
    const uint32_t player = R(0x32DCFC);
    if (exit_level && player && !exited) health = RF(player + 0x504);
    if (exited && player && health > 0 && RF(player + 0x504) < health) {
        uint32_t v;
        std::memcpy(&v, &health, 4);
        kernel::W32(player + 0x504, v);
    }
    const int64_t wait = delay ? int64_t(std::atof(delay) * 1000) : 20000;
    if (!want || done || NowMs() - first < wait || R(kShopState)) return;
    done = true;
    const uint32_t fn = uint32_t(std::strtoul(want, nullptr, 16));
    if (fn < 0x100) kernel::W32(kShopState, fn);
    else kernel::CallGuest(ctx, fn, {});
    LOGR("bulletproof: FC_SHOP_OPEN %X: the shop state %u", fn, R(kShopState));
}

uint32_t LayoutNode(uint32_t hash) { return Find(R(kManager) + 0xBC, hash); }

// The name keyboard (a profile's name; [0x329A1C], shown while [0x329A10] is 2 and its +0xAA5 is clear; its input is
// 0x597A0, which 0x1E3C0 runs after the PDA's frame): five rows (+0xA6C each one's key count) of 16 byte keys from +0xC,
// 11 a row and 5 rows a page (+0xA60 the page): +0 the code (a character; 0x20 space, 0x10000 backspace, 0x10001 to
// 0x10006 Shift, Caps Lock, Alphabet, Symbols, Accents, Done), +4 its width, +8 1 when it can be chosen. The cursor:
// +0xA64 the row, +0xA68 the key, +0xA80 the key kept for moving between rows. The d-pad's moves (0x579C0 and its
// neighbours) play "COMMON_PAGER_INCREASE_OPTION"; A types the cursor's key (0x59590). It is always drawn in one place
// (0x1F7A0): measured on the GPU-VM (1920x800, as 640x480) from x 119.6 to 524.0 across 14 key widths (the command keys
// are drawn two widths for each of theirs: Done is four keys wide), from y 207.6 down in rows 23.6 apart.
constexpr uint32_t kKeyboard = 0x329A1C, kKeyboardShown = 0x329A10;
constexpr float kKeysLeft = 119.6f, kKeyWidth = (524.0f - 119.6f) / 14, kKeysTop = 207.6f, kKeyRow = 23.6f;

uint32_t KeyboardUp() {
    const uint32_t k = R(kKeyboard);
    if (!k || R(kKeyboardShown) != 2 || kernel::G<uint8_t>(k + 0xAA5)) return 0;
    return k;
}

// the key at (x, y): its row and index in the row, false off the keys (or on one that cannot be chosen)
bool KeyAt(uint32_t k, float x, float y, int& row, int& key) {
    row = int(std::floor((y - kKeysTop) / kKeyRow));
    const float u = (x - kKeysLeft) / kKeyWidth;
    if (row < 0 || row > 4 || u < 0 || u >= 14) return false;
    const uint32_t page = R(k + 0xA60), keys = std::min<uint32_t>(R(k + 0xA6C + 4 * row), 11);
    float left = 0;
    for (uint32_t i = 0; i < keys && page < 3; i++) {
        const uint32_t e = k + 0xC + ((page * 5 + row) * 11 + i) * 16, code = R(e), w = R(e + 4);
        left += float(code > 0x10000 && code < 0x10007 ? 2 * w : w);
        if (u < left) {
            key = int(i);
            return kernel::G<uint8_t>(e + 8) != 0;
        }
    }
    return false;
}

// each PDA frame with the keyboard up: the key under the pointer, as it moves, made the cursor's
void KeyboardTrack(X86Context& ctx) {
    const uint32_t k = KeyboardUp();
    float px, py;
    static float last_x = -1, last_y = -1;
    if (!k || !PointerAt(px, py)) {
        last_x = -1;
        return;
    }
    const bool moved = last_x >= 0 && std::fabs(px - last_x) + std::fabs(py - last_y) > 0.5f;
    last_x = px, last_y = py;
    int row, key;
    if (!moved || !KeyAt(k, px, py, row, key) || (int(R(k + 0xA64)) == row && int(R(k + 0xA68)) == key)) return;
    kernel::W32(k + 0xA64, uint32_t(row));
    kernel::W32(k + 0xA68, uint32_t(key));
    kernel::W32(k + 0xA80, uint32_t(key));
    // the d-pad's sound (0x579C0): 0xFA980("common", the cue, 1.0), 0xFA8F0, 0x104750
    const uint32_t cue = kernel::CallGuest(ctx, 0xFA980, {0x2AE2A8, 0x2B4DB0, 0x3F800000});
    if (cue) kernel::CallGuest(ctx, 0x104750, {kernel::CallGuest(ctx, 0xFA8F0, {cue})});
    LOGI("bulletproof: the pointer on the keyboard's row %d key %d", row, key);
}

bool ShopAcross() {
    std::lock_guard<std::mutex> lock(g_lock);
    return NowMs() - g_rows.at < 1000 && g_rows.shop && g_rows.arrows && !g_rows.placed;
}

bool ShopUp() {
    const uint32_t m = ShopManager();
    return m && R(m + 0xC) != 0;
}

int PointerButton() {
    Rows r;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        r = g_rows;
    }
    float x, y;
    if (NowMs() - r.at > 1000 || !PointerAt(x, y)) return -1;
    for (int i = 0; i < r.nbuttons; i++)
        if (x >= r.buttons[i].x0 && x <= r.buttons[i].x1 && y >= r.buttons[i].y0 && y <= r.buttons[i].y1)
            return r.buttons[i].control;
    return -1;
}

int PointerSlider() {
    Rows r;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        r = g_rows;
    }
    float x, y;
    if (NowMs() - r.at > 1000 || !r.placed || !PointerAt(x, y)) return 0;
    const int over = RowAt(r, x, y);
    if (over < 0 || over != r.current || !r.slider[over]) return 0;
    return x < kValueLeft ? 2 : x < kValueMiddle ? -1 : 1;
}

int PointerHit() {
    if (const uint32_t k = KeyboardUp()) {  // the name keyboard: A over the cursor's key, nothing off the keys
        float x, y;
        int row, key;
        if (!PointerAt(x, y)) return kPointerUnknown;
        if (!KeyAt(k, x, y, row, key)) return kPointerOff;
        return int(R(k + 0xA64)) == row && int(R(k + 0xA68)) == key ? kPointerOnCurrent : kPointerOnOther;
    }
    Rows r;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        r = g_rows;
    }
    float x, y;
    if (NowMs() - r.at > 1000 || !PointerAt(x, y)) return kPointerUnknown;  // no PDA menu's frame lately: another menu
    if (!r.placed) return r.shop ? kPointerOff : kPointerUnknown;  // a shop's picker: only its buttons click
    const int over = RowAt(r, x, y);
    return over < 0 ? kPointerOff : over == r.current ? kPointerOnCurrent : kPointerOnOther;
}
}  // namespace fc

namespace fc {
void PcPageFrame(X86Context& ctx, uint32_t manager, uint32_t menu);  // src/game/pc_page.cpp
}

// (ecx the manager): its top menu's frame
X86_EXTERN_FUNC(__imp__sub_00049F00);
X86_FUNC(sub_00049F00) {
    const uint32_t manager = ctx.ecx;
    if (manager == fc::kManager) {
        const uint32_t count = kernel::R32(manager + 0xC), stack = kernel::R32(manager + 0x14);
        fc::PcPageFrame(ctx, manager, count && count < 64 && stack ? kernel::R32(stack + 4 * (count - 1)) : 0);
    }
    fc::Track(ctx, manager);
    if (manager == fc::kManager) fc::KeyboardTrack(ctx);
    ctx.ecx = manager;
    __imp__sub_00049F00(ctx, base);
}
