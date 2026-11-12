// 50 Cent: Bulletproof's own settings and hooks, shared by src/common and src/game
#pragma once
#include <array>
#include <cstdint>

namespace fc {
extern bool modern_buttons;  // the prompts show today's Xbox pad (src/common/buttons.cpp)
// FNV-1a of buttons_Xbox.xtf's 512x64 P8 bytes as the disc has them
constexpr uint64_t kButtonsDisc = 0xD9B9754E92AA4661ull;  // at 0x81BD6D80 in the title menu, 2026-11-11
bool ButtonPictures(uint32_t header, uint32_t palette);  // GameInfo::texture_bound
std::array<uint16_t, 2> Keys(int control);               // GameInfo::default_keys
void Init();                                             // GameInfo::init
}  // namespace fc
