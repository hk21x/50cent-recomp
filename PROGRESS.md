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
- GitHub: private hk21x/50cent-recomp; SDK at fork main 6a49204

## Log

### 2026-11-12 - Merged: fork main 6970886, main f4c629e; the stick clicks; a 60 minute soak (seed 2)

Gate for dispatch-level (6970886): SMOKE OK on PGR2, PGR1, EoN, Agent Under Fire, NightFire; the launcher links;
wincheck clean; this repository's SMOKE OK. Fork main and main fast-forwarded; windows-vm built f4c629e for the share.
Stick clicks (FC_PAD_CMD's LTHUMB / RTHUMB, in the alley): the left showed nothing, the right moved the camera a
little (a recentre, perhaps); not settled, for Harry's pad. scripts/pad/soak.py takes SOAK_SEED.

Soaks: the 60 minute one (seed 2) and the 30 minute one before were clean (frames to the end, no traps, 99 MB flat)
but were in play for only their first 11 to 12 minutes: a START opened the PDA on a new message (Popcorn's camera
modes), where only B goes back, and the soak's next START did not; its stick and A presses then wandered the pause
menu to Quit. Draws a frame by the minute show it (700 to 1000 in the alley, about 30 at the menus). The pause step
now backs out with B four times (B does nothing at the pause menu's top) and takes Continue (A). scripts/soak.sh 30
with it: in play the whole run (330 to 1388 draws a frame each minute, 35 returns to play), no traps, frames to
1798 s, memory flat.

### 2026-11-12 - Soak: a crash and a hang in XACT's DPC; IRQL now keeps DPCs out (SDK); the menu mouse; README

The 30 minute soak (scripts/pad/soak.py: boot.txt's route into the alley, then random play) stopped presenting at
467 s, two threads spinning (memory flat at 99 MB, no traps). Replayed under gdb (the same script) it crashed at 158 s
instead: SIGSEGV in XACT's engine (0x26375E, under 0x263C88) on the kernel's DPC thread. On the console a DPC runs at
dispatch level on the one processor, and DirectSound's calls raise to it to keep the DPC out (0x26B2D0, a CAutoIrql
from some 70 places; XACT's 0x25EA0F); the runtime kept IRQL as a number only, so XACT's DPC ran over the game
thread's DirectSound calls. SDK branch dispatch-level: raising IRQL to 2 holds a lock shared, lowering below 2 lets it
go, and each DPC runs holding it exclusive (an exclusive lock for raised threads too was tried first);
XBE_NO_DISPATCH_LOCK=1 bypasses it, for comparison. With it, the soak's script ran 600 s in two of three instances
(the third froze at 1.1 s of boot: see below), and scripts/soak.sh 30 ran to the end: frames to 1800 s, no traps,
memory flat at 99 MB (the run without the lock stopped at 467 s).

Boot stalls under load: six first boots at once sometimes start 10 to 15 s late (the cache copy); gdb attached to
one (harness XBE_PTRACER=1, the SDK harness's new opt in, as Yama allows only an ancestor) found the main thread in
the movie loop 0x105F50 waiting for the movie, CRI's server thread 0x12F090 suspended between the vsync loop's
(0x12F400) resumes each 16 ms: normal idle, and it went on. With the lock 0 of 24 boots hung (under 600 frames at 70 s),
without it (XBE_NO_DISPATCH_LOCK=1) 0 of 24: the lock adds no boot hang. The one boot that froze at 1.1 s for 600 s
(three instances of the soak's script) stays unexplained: rare, not seen again in 48 boots.

Front-end music: no music file is opened after the title's trk00_12.adx until the intro, so the silence is the
game's. Clipping: 0.05% of samples, on gunfire; the mixer keeps the APU's headroom and clamps.

Menu mouse (src/common/menu_mouse.cpp): byte 0x342A90 is set at the title's menu and the pause menu (0x1047B0, from
0x84E40 which pauses the music, the speech and the movie textures), 0 in play (memory compared headless); there the
left button plays A and the right B instead of fire, the wheel the d-pad's up and down. Tried headless (a live run,
the harness's CLICKL/CLICKR/WHEELD): a left click at the title opened Choose Difficulty, a right click went back, the
wheel moved the row (the harness gives a notch a poll while held; a real wheel one a click: to check on Windows).
README: status, features so far and a controls table.

### 2026-11-12 - The VM's tests: save fixed; no audio device; the name keyboard garbled (a vertex buffer wait unnamed)

windows-vm drove first boots on WARP with a new test hook (src/common/test_pad.cpp: FC_PAD_CMD=<file> of timed
presses and SHOTs, the pad counted plugged in): the movies never ended, as the VM has no audio device and the
runtime's output thread returned when XAudio2 failed, so nothing took the title's samples (the CRI movies run by
their sound). SDK fork branch audio-null-fallback (6a49204): the mixer pulled every 10 ms then. With it the VM went
Title Screen, New Game, Thug, Create Profile, "aaa", Done: saved (UDATA\56550042\006100610061) and on into the intro,
no exception: the save crash fix holds.

The name keyboard was garbled on D3D12 (the UI atlas and the button sheet drawn huge, the PDA frame gone), the same
with FC_NO_BUTTON_PICTURES=1 (the console's pictures): not the replacement. Headless with RT_THREAD=1 (the render
thread, as Windows runs) the keyboard screen showed the profile screen still, at the keyboard's 115 draws a frame:
the render thread drew vertices it had kept. 0x24E880 (the vertex buffer's Lock2) and 0x24E8D0 wait through
0x251B40, D3D_BlockOnNonSurfaceResource (0x36 bytes as 4039's and 4831's; 5849's signatures have none), which run as
guest code never told the renderer the buffer changes. Named in config/bulletproof.toml: with RT_THREAD=1 the
keyboard draws right. SMOKE OK with and without RT_THREAD.

FC_PAD_CMD's presses now show the pad's pictures; FC_NO_BUTTON_PICTURES=1 leaves the disc's (diagnostic).
The VM confirmed on D3D12 (2515abf): the keyboard right with modern prompts, the save and the intro, no exception.
Gate: 474eaeb and 6a49204 SMOKE OK on PGR2, PGR1, EoN, Agent Under Fire, NightFire; the launcher links. Fork main
fast-forwarded to 6a49204 (c3bd337 + the D3D12 read-back + the audio fallback). The other D3D functions the game
calls unnamed were read: GetTile, the reference counts, Register, GetType, GetLevelCount, GetSurfaceLevel2, the
locks (through the hosted Lock2DSurface), SetPushBufferSize (0x24F6B0), a render state setter (0x24BE60) pushing
through the library's helper the runtime reads: none other waits or tells the renderer anything.

### 2026-11-12 - Harry's crash after creating a save profile: a D3D12 read-back over the game's data

Harry (Windows, RX 7900 XT, D3D12): a crash after creating a profile. His log: at 91.7 s, just after "colour target
made ... 821A1180 512x512", an access violation on guest thread 14 reading guest 71A18C80; his dump (exe base
7FF7D3B00000, fault at +0x50D564) symbolises to __imp__sub_001145D0. 0x1145D0 reads a texture table entry at
[[0x347C30]+0x10] + index*8 with the index [object+0x234]: the one object in the list at 0x349260 (0x822A1200, from
0x11AF60's loop at 0x11B0DB) held 0xFE000000 there, and 0x81A18C80 + 0xFE000000*8 wraps to 0x71A18C80.
The dump's guest memory: from 822A1180, the end of the 512x512 target at 821A1180, 0xFE000000 pixels run to about
822F2180, the end of a 720x480 target at the same address (one was made there at present 64 for the front end).
The save picture (saveimage.xbx) is drawn into the 512x512 target and locked; D3D12's read-back took the frame sized
720x480 target at that address (ColourTargetAt preferred it), so 720x480 pixels went over the game's data behind
the 512x512 surface. Not seen headless: the reference renderer draws into guest memory at the surface's own size.
(Headless the same route, profile "aaa", saves U:\006100610061 and plays on.)

SDK fix (fork branch d3d12-readback-latest): a smaller colour target set up at an address is the one read back until
a frame sized one is set up there again. wincheck and SMOKE OK; the gate and Harry's retry next.

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
