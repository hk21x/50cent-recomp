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
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

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
constexpr float kBarLeft = 144, kBarRight = 494;         // the highlight bar, 640 wide
constexpr int kMaxItems = 32;
// the items' kinds by vtable (their pools, 0x48C10 to 0x490F0, set them): a list box's or a text box's rows are its
// own, not placed here
constexpr uint32_t kListBox = 0x2B93EC, kTextBox = 0x2B9404;

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
bool NodeHeight(uint32_t node, float& y) {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int depth = 0; node && depth < 16; depth++, node = R(node + 0x84)) {
        float p[16], r[16];
        for (int i = 0; i < 16; i++) p[i] = RF(node + 0x10 + 4 * i);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) r[i * 4 + j] = m[i * 4] * p[j] + m[i * 4 + 1] * p[4 + j] + m[i * 4 + 2] * p[8 + j] + m[i * 4 + 3] * p[12 + j];
        std::memcpy(m, r, sizeof m);
    }
    if (!std::isfinite(m[13])) return false;
    y = m[13];
    return true;
}

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
    bool placed = false;  // a menu of rows: two or more, no list or text box (else the click plays A anywhere)
    int n = 0, current = -1;
    uint32_t items[kMaxItems];
    float ys[kMaxItems];
    int64_t at = 0;  // ms
};
std::mutex g_lock;
Rows g_rows;

// the row at (x, y), -1 for none
int RowAt(const Rows& r, float x, float y) {
    if (x < kBarLeft || x > kBarRight) return -1;
    int over = -1;
    for (int i = 0; i < r.n; i++) {
        float half = 11;  // half the rows' spacing, from the nearest row
        for (int j = 0; j < r.n; j++)
            if (j != i && std::fabs(r.ys[j] - r.ys[i]) > 1) half = std::min(half, std::fabs(r.ys[j] - r.ys[i]) / 2);
        if (std::fabs(y - r.ys[i]) <= half) over = i;
    }
    return over;
}

// the top menu's frame: its rows placed, the row under the pointer highlighted
void Track(X86Context& ctx, uint32_t manager) {
    const uint32_t count = R(manager + 0xC), stack = R(manager + 0x14);
    if (!count || count > 64 || !stack) return;
    const uint32_t menu = R(stack + 4 * (count - 1)), record = R(menu + 0xC), layer = R(manager);
    if (!menu || !record || !layer) return;
    Rows r;
    bool boxes = false;
    const int n = std::min<int>(kernel::G<uint16_t>(record + 4), kMaxItems);
    for (int i = 0; i < n; i++) {
        const uint32_t item = Find(manager + 0x18, Hash(R(record + 0x28 + 4 * i)));
        if (item && (R(item) == kListBox || R(item) == kTextBox)) boxes = true;
        if (!item || R(item + 0x10) == 1) continue;
        const uint32_t node = Find(layer + 0xBC, R(R(item + 4) + 0x14));
        float y;
        if (!node || !NodeHeight(node, y)) continue;
        r.items[r.n] = item, r.ys[r.n++] = kRowsY0 - kRowsScale * y;
    }
    r.placed = !boxes && r.n >= 2;
    static uint32_t said = 0;
    if (menu != said) {
        said = menu;
        const char* name = static_cast<const char*>(kernel::HostPtr(record + 6));
        LOGR("bulletproof: menu \"%.24s\": %d of %d rows placed%s", mem::IsCommitted(record + 6) ? name : "?", r.n, n,
             r.placed ? "" : " (not a menu of rows: the click plays A anywhere)");
        for (int i = 0; i < r.n; i++) LOGI("bulletproof:   row %d at %.1f", i, r.ys[i]);
    }
    // the pointer highlights a row as it moves onto it; standing still it leaves the keys' and the wheel's choice alone
    float px, py;
    static float last_x = -1, last_y = -1;
    if (r.placed && PointerAt(px, py)) {
        const bool moved = std::fabs(px - last_x) + std::fabs(py - last_y) > 0.5f;
        last_x = px, last_y = py;
        const int over = RowAt(r, px, py);
        const uint32_t current = R(menu + 4);
        if (over >= 0 && r.items[over] != current && moved) {
            const uint32_t item = r.items[over];
            LOGI("bulletproof: the pointer highlights row %d", over);
            if (current) CallThis(ctx, R(R(current) + 0x10), current, {});
            kernel::W32(menu + 4, item);
            CallThis(ctx, R(R(item) + 0xC), item, {});
            CallThis(ctx, kHighlightBar, menu, {});
            CallThis(ctx, kPlaySound, manager, {8, 0x3F800000});
        }
    }
    for (int i = 0; i < r.n; i++)
        if (r.items[i] == R(menu + 4)) r.current = i;
    r.at = NowMs();
    std::lock_guard<std::mutex> lock(g_lock);
    g_rows = r;
}
}  // namespace

int PointerHit() {
    Rows r;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        r = g_rows;
    }
    float x, y;
    if (NowMs() - r.at > 250 || !r.placed || !PointerAt(x, y)) return kPointerUnknown;  // no PDA menu's frame lately: another menu
    const int over = RowAt(r, x, y);
    return over < 0 ? kPointerOff : over == r.current ? kPointerOnCurrent : kPointerOnOther;
}
}  // namespace fc

// (ecx the manager): its top menu's frame
X86_EXTERN_FUNC(__imp__sub_00049F00);
X86_FUNC(sub_00049F00) {
    const uint32_t manager = ctx.ecx;
    if (manager == fc::kManager) fc::Track(ctx, manager);
    ctx.ecx = manager;
    __imp__sub_00049F00(ctx, base);
}
