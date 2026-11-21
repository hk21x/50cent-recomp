// PC Settings as a page of the game's own menus: Y on an Options page (the title's and the pause menu's) opens it in
// the PDA, three pages of five settings with their values and Next Page; left and right (and A, forward) change the
// highlighted one, the mouse points and clicks as on the other lists, B goes back. F1 still opens the whole window.
//
// The page is the game's Codes > View page ("CodesView", the list box cheatListBox), lent while ours is up. Its handler
// (0xCFF70) fills the list from a table of seven cheats at 0x2FE028, 12 bytes each: an id, the name's string table
// hash and an on byte (the icon: 0x2F62D0 on, 0x2F62C0 off), each shown while 0x81020(0x140 + i) says it is
// unlocked; a row is a group of the layout (musicItemGrp/trackGrp0..5, an icon and a label), so six fit. A calls
// 0xCF3A0(entry), which switches the cheat by its id and its on byte, then 0xCFE30 draws every visible row again from
// its entry (the label by 0x51B40 from the hash, the icon from the byte).
//
// While ours is up the table holds our rows (ids 0xFC500000 + i; the hashes our own string table entries, made here:
// 0xF81D0 answers them, src/game/pc_settings.cpp) and is put back when the page closes; 0x81020 says the six are
// unlocked; 0xCF3A0 steps our setting and rewrites the entry. A row's text changes by its entry naming the other of
// two labels (a label is drawn again only for another entry, 0x51B40), whose text is rewritten in place: a record
// whose +4 is 0 is shown from its own text (0xF8870). The page is pushed as an item's next menu is (0x4A220): the
// manager's menu by name (0x49610), its first item current, 0x489D0, 0x49F20.
#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "common/bulletproof.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fc {
extern bool skip_intro;                 // src/common/game_info.cpp
uint32_t LayoutNode(uint32_t hash);     // the PDA's layout node by its name's hash (src/game/menu_pointer.cpp)
}

namespace {
constexpr uint32_t kTable = 0x2FE028, kTableEntries = 7, kEntry = 12;
constexpr uint32_t kPageName = 0x2AFBC0;  // "CodesView"
constexpr uint32_t kFindMenu = 0x49610, kHighlightBar = 0x489D0, kPushMenu = 0x49F20;
constexpr uint32_t kIdBase = 0xFC500000;
constexpr int kSlots = 6;  // the page's row groups

// ---- the settings

struct Row {
    const char* label;
    std::string (*text)();
    void (*step)(int);
    bool flag = false;  // on or off: shown with the page's tick box
};
std::string OnOff(bool b) { return b ? "On" : "Off"; }
void Toggle(bool& b) {
    b = !b;
    app::Save();
}
template <typename T>
int Cycle(T v, const T* list, int n, int d) {  // the index after v's
    int i = 0;
    for (int k = 0; k < n; k++)
        if (list[k] == v) i = k;
    return (i + d + n) % n;
}
#define FLAG(name, expr) \
    { name, [] { return OnOff(expr); }, [](int) { Toggle(expr); }, true }

struct Mode {
    uint32_t w, h;
    bool operator==(const Mode& o) const { return w == o.w && h == o.h; }
};
std::vector<Mode> Modes() {
    std::vector<Mode> list{{0, 0}};  // the desktop's
#if defined(_WIN32)
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
        Mode m{dm.dmPelsWidth, dm.dmPelsHeight};
        if (dm.dmBitsPerPel >= 32 && dm.dmPelsHeight >= 480 && std::find(list.begin(), list.end(), m) == list.end())
            list.push_back(m);
    }
    std::sort(list.begin() + 1, list.end(), [](const Mode& a, const Mode& b) { return a.w != b.w ? a.w > b.w : a.h > b.h; });
#endif
    return list;
}

const char* const kModes[] = {"Full Screen", "Borderless", "Window"};
const char* const kAspects[] = {"Auto", "4:3", "16:9", "Fill"};
const char* const kMsaaNames[] = {"Off", "2x", "4x", "8x"};
constexpr uint32_t kAnisos[] = {0, 2, 4, 8, 16};
constexpr uint32_t kCaps[] = {30, 60, 120, 144, 240, 0};
constexpr uint32_t kFovs[] = {50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150};

constexpr int kRows = kSlots - 1;  // and Next Page
struct Page {
    const char* name;
    Row rows[kRows];
};
const Page kPages[] = {
    {"Display",
     {{"Display Mode", [] { return std::string(kModes[uint32_t(app::Get().display_mode) % 3]); },
       [](int d) {
           auto& s = app::Get();
           s.display_mode = app::DisplayMode((int(s.display_mode) + d + 3) % 3);
           app::Save();
           app::ApplyDisplay();
       }},
      {"Resolution",
       [] {
           auto& s = app::Get();
           return s.width && s.height ? std::to_string(s.width) + " x " + std::to_string(s.height) : std::string("Desktop");
       },
       [](int d) {
           auto& s = app::Get();
           static const std::vector<Mode> list = Modes();
           Mode m = list[size_t(Cycle(Mode{s.width, s.height}, list.data(), int(list.size()), d))];
           s.width = m.w;
           s.height = m.h;
           app::Save();
           app::ApplyDisplay();
       }},
      {"Aspect Ratio", [] { return std::string(kAspects[app::Get().aspect % 4]); },
       [](int d) {
           auto& s = app::Get();
           s.aspect = (s.aspect % 4 + uint32_t(d + 4)) % 4;
           app::Save();
       }},
      FLAG("Vertical Sync", app::Get().vsync),
      {"Frame Rate Cap",
       [] {
           const uint32_t c = app::Get().fps_cap;
           return c ? std::to_string(c) : std::string("Unlimited");
       },
       [](int d) {
           auto& s = app::Get();
           s.fps_cap = kCaps[Cycle(s.fps_cap, kCaps, 6, d)];
           app::Save();
       }}}},
    {"Graphics",
     {{"Render Scale", [] { return std::to_string(app::Get().render_percent) + "%"; },
       [](int d) {
           auto& s = app::Get();
           int r = int(s.render_percent) + d * 25;
           if (r > 200) r = 50;
           if (r < 50) r = 200;
           s.render_percent = uint32_t(r);
           app::Save();
       }},
      {"MSAA", [] { return std::string(kMsaaNames[std::min<uint32_t>(app::Get().msaa, 3)]); },
       [](int d) {
           auto& s = app::Get();
           s.msaa = (std::min<uint32_t>(s.msaa, 3) + uint32_t(d + 4)) % 4;
           app::Save();
       }},
      {"Edge Smoothing",
       [] {
           auto& s = app::Get();
           return std::string(s.smaa ? "SMAA" : s.fxaa ? "FXAA" : "Off");
       },
       [](int d) {
           auto& s = app::Get();
           int at = (s.smaa ? 2 : s.fxaa ? 1 : 0);
           at = (at + d + 3) % 3;
           s.fxaa = at == 1;
           s.smaa = at == 2;
           app::Save();
       }},
      {"Texture Filtering",
       [] {
           const uint32_t a = app::Get().aniso;
           return a ? std::to_string(a) + "x" : std::string("Default");
       },
       [](int d) {
           auto& s = app::Get();
           s.aniso = kAnisos[Cycle(s.aniso, kAnisos, 5, d)];
           app::Save();
       }},
      {"Sharpening", [] { return std::to_string(app::Get().sharpen) + "%"; },
       [](int d) {
           auto& s = app::Get();
           int v = int(s.sharpen) + d * 25;
           if (v > 100) v = 0;
           if (v < 0) v = 100;
           s.sharpen = uint32_t(v);
           app::Save();
       }}}},
    {"Game",
     {{"Field of View", [] { return std::to_string(fc::fov_percent) + "%"; },
       [](int d) {
           fc::fov_percent = kFovs[Cycle(fc::fov_percent, kFovs, 11, d)];
           app::Save();
       }},
      FLAG("Hide the HUD", fc::hide_hud),
      FLAG("Skip Intro Movies", fc::skip_intro),
      FLAG("Modern Button Pictures", fc::modern_buttons),
      FLAG("Enhanced Textures", app::Get().world_textures)}},
};
constexpr int kPageCount = int(sizeof kPages / sizeof kPages[0]);

// ---- our labels: two a slot, the text rewritten in place

constexpr uint32_t kTextMax = 64;
struct Label {
    uint32_t hash = 0, record = 0, entry = 0;
};
Label g_labels[kSlots][2];
int g_which[kSlots] = {};
bool g_made = false;

// fixed labels: the page's title in place of "Codes" while it is ours, and the button bar's "Change" for A
uint32_t NameHash(const char* n) {  // 0x110700
    uint32_t h = 0x811C9DC5;
    for (; *n; n++) {
        uint32_t c = uint8_t(*n);
        if (c >= 'a' && c <= 'z') c -= 0x20;
        h = h * 0x1000193 ^ c;
    }
    return h;
}
const uint32_t kCodesTitle = NameHash("menuNameLabels_codes");
Label g_title, g_change;
uint32_t g_change_name = 0;  // "pcChangeLbl" in guest memory, for the prompt table
void MakeFixed(Label& l, uint32_t hash, const char* text) {
    l.hash = hash;
    const uint32_t n = uint32_t(std::strlen(text));
    l.record = mem::AllocSystem(12 + kTextMax, 16);
    std::memset(kernel::HostPtr(l.record), 0, 12 + kTextMax);
    kernel::W32(l.record, hash);
    kernel::W16(l.record + 8, uint16_t(n));
    kernel::W16(l.record + 10, uint16_t(12 + kTextMax));
    std::memcpy(kernel::HostPtr(l.record + 12), text, n);
    l.entry = mem::AllocSystem(16, 16);
    std::memset(kernel::HostPtr(l.entry), 0, 16);
    kernel::W32(l.entry, hash);
    kernel::W32(l.entry + 4, l.record);
    kernel::W16(l.entry + 12, 1);
}

void MakeLabels() {
    if (g_made) return;
    g_made = true;
    MakeFixed(g_title, kCodesTitle, "PC Settings");
    constexpr char kChange[] = "pcChangeLbl";
    g_change_name = mem::AllocSystem(sizeof kChange, 16);
    std::memcpy(kernel::HostPtr(g_change_name), kChange, sizeof kChange);
    MakeFixed(g_change, NameHash(kChange), "Change");
    for (int s = 0; s < kSlots; s++)
        for (int v = 0; v < 2; v++) {
            Label& l = g_labels[s][v];
            l.hash = 0xFC5A0000u + uint32_t(s * 2 + v);
            l.record = mem::AllocSystem(12 + kTextMax, 16);
            std::memset(kernel::HostPtr(l.record), 0, 12 + kTextMax);
            kernel::W32(l.record, l.hash);
            kernel::W16(l.record + 10, uint16_t(12 + kTextMax));
            l.entry = mem::AllocSystem(16, 16);
            std::memset(kernel::HostPtr(l.entry), 0, 16);
            kernel::W32(l.entry, l.hash);
            kernel::W32(l.entry + 4, l.record);
            kernel::W16(l.entry + 12, 1);  // as the disc's entries: not loaded, no count
        }
}

// slot s shows `text`: the other label, its text rewritten; its hash
uint32_t SetText(int s, const std::string& text) {
    g_which[s] ^= 1;
    const Label& l = g_labels[s][g_which[s]];
    const std::string t = text.substr(0, kTextMax - 1);
    char* p = static_cast<char*>(kernel::HostPtr(l.record + 12));
    std::memset(p, 0, kTextMax);
    std::memcpy(p, t.data(), t.size());
    kernel::W16(l.record + 8, uint16_t(t.size()));
    return l.hash;
}

// ---- the page's state

std::atomic<bool> g_open_request{false};
bool g_on = false;  // our rows in the table
int g_page = 0;
uint8_t g_saved[kTableEntries * kEntry];
constexpr uint32_t kPrompt = 0x329D1C;  // the prompt table's "CodesView": B A X Y label names from +4
uint32_t g_prompt_a = 0;

std::string RowText(int slot) {
    if (slot == kRows) return std::string("Next Page (") + kPages[(g_page + 1) % kPageCount].name + ")";
    const Row& r = kPages[g_page].rows[slot];
    return std::string(r.label) + ":  " + r.text();
}
bool RowOn(int slot) {
    if (slot == kRows) return false;
    const Row& r = kPages[g_page].rows[slot];
    return r.flag && r.text() == "On";
}

// entry i of the table as slot i shows now
void WriteEntry(int i) {
    const uint32_t e = kTable + uint32_t(i) * kEntry;
    kernel::W32(e, kIdBase + uint32_t(i));
    kernel::W32(e + 4, SetText(i, RowText(i)));
    kernel::G<uint8_t>(e + 8) = RowOn(i) ? 1 : 0;
}

void Enter() {
    MakeLabels();
    std::memcpy(g_saved, kernel::HostPtr(kTable), sizeof g_saved);
    g_on = true;
    for (int i = 0; i < kSlots; i++) WriteEntry(i);
    g_prompt_a = kernel::R32(kPrompt + 8);
    kernel::W32(kPrompt + 8, g_change_name);
    LOGR("bulletproof: the PC Settings page, %s", kPages[g_page].name);
}
void Leave() {
    if (!g_on) return;
    std::memcpy(kernel::HostPtr(kTable), g_saved, sizeof g_saved);
    kernel::W32(kPrompt + 8, g_prompt_a);
    g_on = false;
    LOGR("bulletproof: out of the PC Settings page");
}

uint32_t CallThis(X86Context& ctx, uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args) {
    const uint32_t ecx = ctx.ecx;
    ctx.ecx = self;
    const uint32_t r = kernel::CallGuest(ctx, fn, args);
    ctx.ecx = ecx;
    return r;
}
}  // namespace

namespace fc {
// our labels' string table entries (src/game/pc_settings.cpp's 0xF81D0 asks), 0 for another hash
uint32_t PcPageEntry(uint32_t hash) {
    if (!g_made) return 0;
    if (hash == g_change.hash) return g_change.entry;
    if (g_on && hash == kCodesTitle) return g_title.entry;
    for (auto& slot : g_labels)
        for (const Label& l : slot)
            if (l.hash == hash) return l.entry;
    return 0;
}

// the Options page's Y (src/game/pc_settings.cpp): the page opens on the PDA's next frame
void OpenPcPage() { g_open_request = true; }

// each of the PDA menu manager's frames (src/game/menu_pointer.cpp), its top menu `menu`
void PcPageFrame(X86Context& ctx, uint32_t manager, uint32_t menu) {
    const uint32_t record = menu ? kernel::R32(menu + 0xC) : 0;
    const bool on_page = record && mem::IsCommitted(record + 6) &&
                         std::strncmp(static_cast<const char*>(kernel::HostPtr(record + 6)), "CodesView", 10) == 0;
    if (g_on && !on_page) Leave();
    if (!g_open_request.exchange(false)) return;
    Enter();
    const uint32_t page = CallThis(ctx, kFindMenu, manager, {kPageName});
    if (!page) {
        LOGE("bulletproof: no CodesView menu to show the PC Settings on");
        Leave();
        return;
    }
    kernel::W32(page + 4, kernel::R32(page));
    CallThis(ctx, kHighlightBar, page, {});
    CallThis(ctx, kPushMenu, manager, {page});
}
}  // namespace fc

// (profile; base, i): whether unlock base + i is still locked; our six rows are shown
X86_EXTERN_FUNC(__imp__sub_00081020);
X86_FUNC(sub_00081020) {
    const uint32_t base_id = kernel::R32(ctx.esp + 4), i = kernel::R32(ctx.esp + 8);
    if (g_on && base_id == 0x140 && i < kTableEntries) {
        ctx.eax = i < uint32_t(kSlots) ? 0 : 1;
        ctx.esp += 12;  // ret 8
        return;
    }
    __imp__sub_00081020(ctx, base);
}

// (entry): a cheat switched; ours steps its setting (Next Page turns the page) and the entries are written again
X86_EXTERN_FUNC(__imp__sub_000CF3A0);
X86_FUNC(sub_000CF3A0) {
    const uint32_t entry = kernel::R32(ctx.esp + 4);
    const uint32_t id = entry ? kernel::R32(entry) : 0;
    if (g_on && id >= kIdBase && id < kIdBase + uint32_t(kSlots)) {
        const int slot = int(id - kIdBase);
        if (slot == kRows) {
            g_page = (g_page + 1) % kPageCount;
            LOGR("bulletproof: PC Settings, %s", kPages[g_page].name);
        } else {
            kPages[g_page].rows[slot].step(1);
            LOGR("bulletproof: PC Settings: %s", RowText(slot).c_str());
        }
        for (int i = 0; i < kSlots; i++) WriteEntry(i);
        ctx.esp += 4;  // ret (cdecl)
        return;
    }
    __imp__sub_000CF3A0(ctx, base);
}

// (item): the list's visible rows drawn again from their entries; ours that are not on/off lose the tick box (its alpha
// 0, as the game sets 1 with the icon's vtable +0x20)
X86_EXTERN_FUNC(__imp__sub_000CFE30);
X86_FUNC(sub_000CFE30) {
    const uint32_t item = kernel::R32(ctx.esp + 4);
    __imp__sub_000CFE30(ctx, base);
    if (!g_on || !item) return;
    const uint32_t box = fc::LayoutNode(kernel::R32(kernel::R32(item + 4) + 0x14));
    if (!box) return;
    const int n = int16_t(kernel::G<uint16_t>(box + 0x1BA));
    for (int k = 0; k < n && k < 16; k++) {
        const uint32_t node = kernel::R32(kernel::R32(box + 0xA4) + 12 * k), entry = kernel::R32(kernel::R32(box + 0xA4) + 12 * k + 4);
        const uint32_t id = entry ? kernel::R32(entry) : 0;
        if (!node || id < kIdBase || id >= kIdBase + uint32_t(kSlots)) continue;
        const int slot = int(id - kIdBase);
        if (slot < kRows && kPages[g_page].rows[slot].flag) continue;
        const uint32_t icon = kernel::R32(kernel::R32(node + 0xA8));
        if (!icon) continue;
        const uint32_t zero = 0;
        CallThis(ctx, kernel::R32(kernel::R32(icon) + 0x20), icon, {zero, 4});
    }
}

// (item, flags): the page's handler. While ours is up, left (flag 1) and right (2) step the highlighted row back and on
// (Next Page turns the pages), its rows then drawn again (0xCFE30); up and down (4, 8), A (0x10) and B as the game's
constexpr uint32_t kDrawRows = 0xCFE30;
X86_EXTERN_FUNC(__imp__sub_000CFF70);
X86_FUNC(sub_000CFF70) {
    const uint32_t item = kernel::R32(ctx.esp + 4), flags_va = kernel::R32(ctx.esp + 8);
    int d = 0;
    if (g_on && item && flags_va) {
        const uint32_t flags = kernel::R32(flags_va);
        d = (flags & 2) ? 1 : (flags & 1) ? -1 : 0;
    }
    if (d) {
        const uint32_t box = fc::LayoutNode(kernel::R32(kernel::R32(item + 4) + 0x14));
        const int at = box ? int16_t(kernel::G<uint16_t>(box + 0x1B8)) : -1;
        const uint32_t slots = box ? kernel::R32(box + 0xA0) : 0;
        const int index = at >= 0 && slots ? int16_t(kernel::G<uint16_t>(slots + 2 * at)) : -1;
        const uint32_t entry = index >= 0 ? kernel::R32(kernel::R32(box + 0xA4) + 12 * index + 4) : 0;
        const uint32_t id = entry ? kernel::R32(entry) : 0;
        if (id >= kIdBase && id < kIdBase + uint32_t(kSlots)) {
            const int slot = int(id - kIdBase);
            if (slot == kRows) g_page = (g_page + d + kPageCount) % kPageCount;
            else kPages[g_page].rows[slot].step(d);
            LOGR("bulletproof: PC Settings: %s", slot == kRows ? kPages[g_page].name : RowText(slot).c_str());
            for (int i = 0; i < kSlots; i++) WriteEntry(i);
            __imp__sub_000CFF70(ctx, base);
            kernel::CallGuest(ctx, kDrawRows, {item});
            return;
        }
    }
    __imp__sub_000CFF70(ctx, base);
}
