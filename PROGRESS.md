# 50 Cent: Bulletproof recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 2 - New Game reaches the first mission (the alley) headless; the console's layout read (LT crouches,
  RT fires, X reloads, White heals, Black the weapons); modern button pictures and WASD and mouse keys (to see on Windows); SMOKE OK on that route. The CRI movie server race fixed (src/game/movie_server.cpp). Next: Windows
- 60 fps headless with the simulation on the clock (Havok steps of 1/60 s); Windows frame rate from Harry's run
- To check on Windows: picture and sound; the front end is silent after the title music (58 s) headless; gameplay
  audio peaks at full scale; the modern and key button pictures (the reference renderer does not draw replacements)
- XDK: 5849 (D3D8 not LTCG), 157 of 278 functions, 11 of 13 data
- Push buffers: none recorded or replayed. Game code writes no push buffer tags and calls no push buffer API; the
  library's one tag write (0x257380) is under the hosted Begin/DrawVertices. 0x24E880 pushes a vertex cache flush
- GitHub: private hk21x/50cent-recomp; SDK at fork main c3bd337

## Log

### 2026-11-12 - Modern button pictures; the keyboard and mouse; the console's layout

The layout (no page in the game sets it): the PS2 version's list (psxdatacenter SLUS-21315 / SLES-53906) matched to
the Xbox's face buttons, and each other button tried alone in the alley (snapshots): RT fires on its own (15/20 ->
14/20), LT held crouches (it is not aiming, as the earlier entry had it), White held shows the health packs (heal),
Black held the weapon inventory, X reloads; A jump, B hostage / execute / grab cover, Y evade / dual-wield / dive by
the PS2 list. The d-pad's up and Back showed nothing; the stick clicks cannot be pressed in the harness (its pad
script has no names for them). Today's pad has White and Black where LB and RB are, so the console's layout stands
on a modern pad; only the pictures and the keys are new.

Button pictures (src/common/buttons.cpp, with True Crime's stroke font src/common/strokes.*): the prompts' glyphs are
one texture, buttons_Xbox.xtf (512x64 P8 swizzled, FNV-1a D9B9754E92AA4661, at 0x81BD6D80 in the menus), two rows of
32 texel cells (their boxes measured on its pixels). Bound (GameInfo::texture_bound), it is drawn again four times
larger as a replacement (gpu::SetReplacement), the guest's bytes left alone: A B X Y as coloured letters on dark
discs, LT RT, LB RB for White and Black, View and Menu, d-pads with the arm lit, L3 R3 as sticks marked L and R; with
the keyboard (input::UsingKeyboard) the bound keys. Setting modern_buttons (on). The reference renderer does not draw
replacements, so FC_BUTTONS_DUMP=dir writes each look drawn: both checked (FC_KEY_PICTURES=1 for the keys).

Keys (GameInfo::default_keys, src/common/game_info.cpp): WASD moves, the mouse looks (mouse_look), the left button
fires, C or Ctrl crouches, Space jumps (Enter selects), F hostage / cover (Backspace back), E or R action and reload,
Shift evades, H heals, Q or the middle button the weapons, Tab the music track, Esc or P pauses, the arrows the d-pad.

SMOKE OK; wincheck clean. Next: Harry's Windows run for the pictures; the mouse in the menus (a pointer, as True
Crime's); his answers on the alley's frame rate and sound.

### 2026-11-11 - Windows build; 60 fps; Harry's three priorities

windows-vm: main 2ab47b7 builds on Windows (LLVM 22.1.3, 4.3 min, no errors; xbe_symbols rewrote config\game.toml
identically) and boots 103 s with no traps or application errors on WARP (about 2.4 frames a second, no GPU or
audio); ready for Harry at \\192.168.1.229\Work\50cent-recomp\out\build\release\Bulletproof.exe.
Harry (through windows-vm): 1. the first mission on Windows, 2. at least 60 frames a second, 3. modern controller
buttons and layout plus keyboard and mouse, as True Crime has.

60 fps: the game asks for interval 0x80000002 (two, or immediate when late: 30 a second on the console), but no
5849 layout gives the runtime that interval, so it presents every vertical blank: 60 a second headless (frames equal
vertical blanks). The simulation follows the clock, not frames: Havok's step (0x189230, its second argument's
start, end, step, rate) logged under gdb in the alley 0.0167 s a step (rate 59.6 to 60.1, 0.0190 and 0.0144 for a
long and a short frame). So 60 runs at the right speed, and more should too. Real frame rates need Harry's run.

Controls: neither Options (Game, Camera, Audio/Video, Customize Pager, Credits) nor the pause menu (Continue, Tasks,
Communications, Entertainment, Options: + Codes, Enable Autosave; Quit) has a controller page: the controls are
fixed. The pad is read once a frame by 0x11F530 (XInputGetState for the port at +0x28, report at +0xE4): the d-pad,
Start, Back and stick clicks become 1.0 or 0 at +0x4C to +0x88, the analog buttons (past a dead zone [0x30E65C]) at
+0x5C on, the sticks at +0x3C to +0x48; last frame's copy at +0x8C. The game's logical buttons are BUTTON_1 to
BUTTON_12 (strings); the map from channels to them is next. Prompts: the HUD and menus draw glyphs from
buttons_Xbox.xtf (in an .ark), placed in strings by a marker ("This string should have an action button (").

### 2026-11-11 - The movie server race (crash in 0x153280); the gameplay buttons; push buffers; audio

Crash: signal 11 reading guest 0x48 in 0x153280, from the CRI Sofdec server (0x12F090 -> 0x132840 -> 0x1556F0 ->
0x1556A0 -> 0x1555D0 -> 0x153280) during the logo movies: 3 of 6 headless runs with WAV output (WAV=audio.wav),
0 of 4 without, so on Windows (real audio) it would be likelier. gdb watchpoints on handle 0's SFD handle
(0x49D748) showed the movie thread zeroing it and putting it back many times a movie, always on its own thread:
CRI stops or resets the SFD that way (0x14AE20, 0x149A10), under the state's server switch +0x170C (0x1554A0)
that the server 0x1556F0 tests once before serving the eight handles. On the console the server's thread outranks
the movie thread on one core; here they overlap. src/game/movie_server.cpp: switching the server off waits for a
pass under way (fences on both sides, a 100 ms limit, the caller's own pass excepted). That alone: 1 crash in 12;
the stop 0x14AE20 also runs unswitched from 0x14B140 (the game's 0xF8D00), so it now throws the switch itself when
its caller has not. 0 crashes in 18 runs with WAV; the 100 ms limit was never reached. SMOKE OK.

Buttons in the alley (live run driven through the cmd file): LT moves the camera over the shoulder to aim; RT
while aiming fires (15/20 -> 13/20 after two presses); X reloads (back to 15/20). A, B and Y change the view and
the pose, but stills do not say what they do. No traps.

Push buffers (Ghidra, scripts/ghidra.sh PushBufferScan): the only push buffer tag store is 0x257468 (0x20001) in
0x257380, reached only from the D3D library's Begin, DrawVertices and DrawIndexedVertices, which the host replaces.
The game's .text stores no tags and calls nothing that builds or runs one; its other hits are flags and colours
(0x118B20 unpacks a colour). Nothing to replay.

Audio (WAV of the route, 248 s, scripts/wavstats.py): the logo movies around -30 dB, the teaser and title music
(trk00_12.adx) 45 to 58 s, then silence through the front end (only blips at the menu presses) until the intro movie
at 107 s; the mission about -15 dB RMS with music (trk05_27.adx, act_12.adx), voices (voiceovr.afs) and peaks at
full scale (11116 clipped samples). Whether the front end has music on the console: to hear on Windows.

Next: a Windows build through the windows-vm session.

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
