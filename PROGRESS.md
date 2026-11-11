# 50 Cent: Bulletproof recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 2 - New Game reaches the first mission (the alley) headless with the HUD, the left stick walks; SMOKE OK
  on that route. Not yet checked: firing, aiming, the face buttons, sound (Windows)
- XDK: 5849 (D3D8 not LTCG), 157 of 278 functions, 11 of 13 data
- Push buffers: no game code writes them (every push pointer user is in the D3D library); one unnamed library helper
  (0x24E880) pushes a vertex cache flush. CreatePushBuffer/RunPushBuffer not seen; Ghidra check still to do
- GitHub: private hk21x/50cent-recomp; SDK at fork main c3bd337

## Log

### 2026-11-11 - The first mission headless: pad route and smoke test

Mapped live (a long harness run driven through its `cmd` file, a `snap` after each press): START through the logo
movies (the CRI Sofdec/ADX screen among them) and the teaser to the Title Screen (New Game / Load Game / Options),
New Game, Choose Difficulty (Thug, Normal, the default), the profile screen (X: Continue without saving; the
confirmation's default is No, so UP then A), then START skips seq\S001.sfd and cs\m001.sfd. The mission loads
voiceovr.afs and starts in an alley: 50 in third person with Lloyd Banks, the HUD (Health, Armor, $0, Nine 15/20,
"Lloyd Banks: Picking Lock"), about 1900 draws a frame. Holding the left stick walks 50 down the alley. No traps.

scripts/pad/boot.txt is that route from a first boot (in the alley by about vblank 7200, then walking).
scripts/game.env: SMOKE_MENU "5800 6100" (the intro movie, 3 draws a frame: the front end's own screens draw
about 116, over smoke's 2D limit of 100) and SMOKE_SECS 150.
Evidence: SMOKE OK: menu 3, race 1858 draws a frame over the last 300 frames, no traps; with
PAD=scripts/pad/none.txt it fails ("race not drawing: 1 draws a frame").

Next: firing and aiming (RT, LT) and the face buttons headless; then a Windows build for picture and sound; the
push buffer check in Ghidra.

### 2026-11-11 - Black frames: every vertex program was loaded at slot 0 (SDK: the shader object's size)

The final full-screen copy (vertex shader B0064BF1, `mov oPos, v0` and three more) drew with positions of 1e25 and w
of 0. Its constants were right (XBE_VSC: c-95 = 360.5 240.5, c-70 = 0.03125 ...). REF_DRAWLOG with a temporary
dump of the program the reference renderer ran showed 10 instructions that start with DP4s against c10 to c13:
another shader's program.

The game places programs itself: 0x12A4A5 LoadVertexShader(handle, [state+0x20]), then adds the shader's size,
which it reads through 0x250E10 (GetVertexShaderSize: the object's +8). The host's CreateVertexShader returned a
zeroed 16-byte record, so the size was 0: logged loads were "B00652B1 at 0" then "B0064BF1 at 0", and later loads
overwrote both. The XDK's own CreateVertexShader stores the count at +8 (0x2509F6).

Fix (SDK c3bd337, fork branch bulletproof-vs-size): the hosted CreateVertexShader writes the instruction count at
+8. Evidence: REF captures now show the Sofdec teaser movie, the PDA-style front end (profile name entry with
"aaa" typed by the pad script, Choose Difficulty: Playa ... G-Unit Soldier).

Gate (smoke-tools/gate.sh, each game at its main): PGR2, PGR1, EoN, Agent Under Fire and NightFire SMOKE OK on
c3bd337; True Crime's own line conflicts with fork main in app.h and interp.cpp (not this change), and passed on its
pin with the commit cherry-picked (148c60e, local test only). The launcher links. Fork main fast-forwarded to
c3bd337. This repository's smoke: SMOKE OK.

Next: a named pad route into the first mission and a smoke route that reaches gameplay.

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
