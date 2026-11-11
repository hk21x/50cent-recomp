# 50 Cent: Bulletproof recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 1 - set up; boots headless to the front-end menu with no game code; **every frame is black** in the
  reference renderer
- XDK: 5849 (D3D8 not LTCG), 157 of 278 functions, 11 of 13 data
- Push buffers: none found by name; one unnamed D3D helper (0x24E880) writes a command directly. Recorded push buffers
  not yet ruled out in Ghidra
- GitHub: private hk21x/50cent-recomp; SDK at fork main 26f372b

## Log

### 2026-11-11 - Repository set up from the SDK template; boots to the menu headless

Harry created hk21x/50cent-recomp (its first commit holds only a README) and asked for the repository to be set up
after the research pass. This commit adds the SDK template, the SDK submodule on fork main (26f372b) and the game's
names: target and exe `Bulletproof`, config/bulletproof.toml (hand entries; it includes the scan's config/game.toml),
the launcher's release table (0abb01d5... "USA, Europe"), the release workflow, README and CLAUDE.md.

Disc: `C:\ISOs\50 Cent - Bulletproof (USA, Europe)\...xiso.iso` on the windows-vm, extracted with extract-xiso into
assets/ (542 files, 5.5 GB, one xbe). Title 0x56550042, region flags 7.

Evidence, with no game code or hooks (scripts/harness.sh 90 scripts/pad/boot.txt):
- Recompile: 11,502 functions, 290 jump tables, no catch blocks.
- No traps and no missing kernel imports. First boot copies Ark\Shell and Ark\Player to Z:, then plays the Sofdec
  movies (sierra, gglogo, DolbyN, inter, havok, teaser) with ADX music. START skips them, and from about vblank 1800
  the front end draws about 116 draws a frame (115 into a 720x480 off-screen target 0x021A4F80, then one full-screen
  quad to the back buffer 0x01236000 with vertex shader B0064BF1).
- REF captures are all black, the movies included. CAP draw lists are well formed.
- `d3d: MakeSpace from 0024E89D`: sub_24E880 (resource, flags) writes method 0x1710 (one dword) unless flag 0x10, waits
  for the resource (0x251B40) and returns its data | 0x80000000. It looks like D3DVertexBuffer_Lock2.

scripts/pad/boot.txt is provisional (START and A every five seconds), and so is the smoke route built on it.

Next: the black frames. The movies and the menu share the final full-screen quad (vertex shader B0064BF1, 4
instructions, screen-space positions, texture modulated by diffuse). Then name 0x24E880 and check for recorded push
buffers with `scripts/ghidra.sh PushBufferScan`.
