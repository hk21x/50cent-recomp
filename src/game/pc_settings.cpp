// PC settings from the game's own Options page: "Y PC Settings" in the PDA's prompt bar there (the title's Options and
// the pause menu's), and Y opens the settings window (the F1 window).
//
// The prompt bar is a table 0x329C40 the front end fills at start (0x1EDAA0): an entry a menu, 0x14 bytes: the menu's
// name, then the label names (string table names) for B, A, X and Y; 0 hides that button. 0x1F010 (arg 2: the menu,
// its name at [+0xC] + 6) finds the current menu's entry by name each time one comes up and sets the bar from it. The
// Options pages are "Options" (0x329C7C) and "MainOptions" (0x329C90), their X and Y free; here their Y gets our label.
//
// A label name is hashed (0x110700: FNV-1, case blind) and looked up in the string table ([0x32304C], 0xF81D0: an entry
// of 0x10 bytes, the hash, the source record, the text once loaded, flags and a count). 0xF8870 loads the text from the
// record on first use: the hash, 0, the length (16 bit at +8), the record's size (16 bit at +0xA), the text. The disc
// has no "PC Settings", so 0xF81D0 answers our label's hash with an entry of ours over a record of ours.
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

#include "app/app.h"
#include "common/bulletproof.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace {
// our labels: the name the prompt table gives (hashed as the game's), the text the string table answers with
struct Label {
    const char* name;
    const char* text;
    uint32_t hash = 0, name_va = 0, entry = 0;  // in guest memory once made
};
Label g_labels[] = {{"pcSettingsLbl", "PC Settings"}, {"quitGameLbl", "Quit Game"}};
Label& kPcSettings = g_labels[0];
Label& kQuitGame = g_labels[1];
constexpr uint32_t kOptions = 0x329C7C, kMainOptions = 0x329C90;  // the prompt table's entries; Y at +0x10
constexpr uint32_t kTitleMenu = 0x329DE4;                         // "Main Menu", the title's; X at +0xC

std::atomic<bool> g_options_page{false}, g_title_menu{false};

// 0x110700's hash: FNV-1 over the name, a-z as A-Z
uint32_t Hash(const char* s) {
    uint32_t h = 0x811C9DC5;
    for (; *s; s++) {
        uint32_t c = uint8_t(*s);
        if (c >= 'a' && c <= 'z') c -= 0x20;
        h = h * 0x1000193 ^ c;
    }
    return h;
}

void Make(Label& l) {
    if (l.entry) return;
    l.hash = Hash(l.name);
    const uint32_t name_size = uint32_t(std::strlen(l.name)) + 1, text_size = uint32_t(std::strlen(l.text)) + 1;
    l.name_va = mem::AllocSystem(name_size, 16);
    std::memcpy(kernel::HostPtr(l.name_va), l.name, name_size);
    const uint32_t record_size = 12 + ((text_size + 3) & ~3u);
    const uint32_t record = mem::AllocSystem(record_size, 16);
    std::memset(kernel::HostPtr(record), 0, record_size);
    kernel::W32(record, l.hash);
    kernel::W16(record + 8, uint16_t(text_size - 1));
    kernel::W16(record + 10, uint16_t(record_size));
    std::memcpy(kernel::HostPtr(record + 12), l.text, text_size);
    l.entry = mem::AllocSystem(16, 16);
    std::memset(kernel::HostPtr(l.entry), 0, 16);
    kernel::W32(l.entry, l.hash);
    kernel::W32(l.entry + 4, record);
    kernel::W16(l.entry + 12, 1);  // as the disc's entries: not loaded, no count
}
}  // namespace

// the front end's prompt table, filled: our labels on the Options pages' Y and the title menu's X
X86_EXTERN_FUNC(__imp__sub_001EDAA0);
X86_FUNC(sub_001EDAA0) {
    __imp__sub_001EDAA0(ctx, base);
    for (Label& l : g_labels) Make(l);
    kernel::W32(kOptions + 0x10, kPcSettings.name_va);
    kernel::W32(kMainOptions + 0x10, kPcSettings.name_va);
    kernel::W32(kTitleMenu + 0xC, kQuitGame.name_va);
    LOGR("bulletproof: \"PC Settings\" on the Options pages' Y, \"Quit Game\" on the title menu's X");
}

// (this string table, hash): our labels' entries
X86_EXTERN_FUNC(__imp__sub_000F81D0);
X86_FUNC(sub_000F81D0) {
    const uint32_t hash = kernel::R32(ctx.esp + 4);
    __imp__sub_000F81D0(ctx, base);
    for (const Label& l : g_labels)
        if (hash == l.hash && l.entry) ctx.eax = l.entry;
    if (const uint32_t e = fc::PcPageEntry(hash)) ctx.eax = e;  // the PC Settings page's rows (src/game/pc_page.cpp)
}

// (?, menu): a menu comes up; whether it is an Options page or the title's menu
X86_EXTERN_FUNC(__imp__sub_0001F010);
X86_FUNC(sub_0001F010) {
    const uint32_t menu = kernel::R32(ctx.esp + 8);
    bool options = false, title = false;
    if (menu && mem::IsCommitted(menu)) {
        const uint32_t name = kernel::R32(menu + 0x0C) + 6;
        if (mem::IsCommitted(name)) {
            const char* n = static_cast<const char*>(kernel::HostPtr(name));
            options = std::strncmp(n, "Options", 8) == 0 || std::strncmp(n, "MainOptions", 12) == 0;
            title = std::strncmp(n, "Main Menu", 10) == 0;
        }
    }
    if (options != g_options_page.load()) LOGI("bulletproof: %s", options ? "on an Options page" : "off the Options pages");
    g_options_page = options;
    g_title_menu = title;
    __imp__sub_0001F010(ctx, base);
}

namespace fc {
// GameInfo::pad (src/common/menu_mouse.cpp): on an Options page, Y opens the settings window and the game does not see it
void PcSettingsPad(input::Pad& p) {
    static bool was = false;
    const bool y = p.analog[input::kY - input::kA] > 128;
    const bool on = g_options_page && MenuUp();
    if (on && y && !was) {
        LOGR("bulletproof: Y on the Options page: the PC Settings page");
        OpenPcPage();
    }
    was = y;
    if (on) p.analog[input::kY - input::kA] = 0;
    // X on the title's menu: Quit Game (nothing to lose there: the game saves as it goes)
    static bool was_x = false;
    const bool x = p.analog[input::kX - input::kA] > 128;
    const bool title = g_title_menu && MenuUp();
    if (title && x && !was_x) {
        LOGR("bulletproof: X on the title menu: Quit Game");
        app::Quit();
    }
    was_x = x;
    if (title) p.analog[input::kX - input::kA] = 0;
}
}  // namespace fc
