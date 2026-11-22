# 50 Cent: Bulletproof recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 2 - New Game reaches the first mission (the alley) headless; the console's layout read (LT crouches,
  RT fires, X reloads, White heals, Black the weapons); modern button pictures and WASD and mouse keys (to see on Windows); SMOKE OK on that route. The CRI movie server race fixed (src/game/movie_server.cpp). Next: Windows
- 60 fps headless with the simulation on the clock (Havok steps of 1/60 s); on Harry's PC 60.0 fps, about 2 ms of host work a frame
- To check on Windows: picture and sound; the front end is silent after the title music (58 s) headless; gameplay
  audio peaks at full scale; the modern and key button pictures (the reference renderer does not draw replacements)
- XDK: 5849 (D3D8 not LTCG), 157 of 278 functions, 11 of 13 data
- Push buffers: none recorded or replayed. Game code writes no push buffer tags and calls no push buffer API; the
  library's one tag write (0x257380) is under the hosted Begin/DrawVertices. 0x24E880 pushes a vertex cache flush
- GitHub: private hk21x/50cent-recomp; SDK at fork main c06f201; README with GPU-VM shots (docs/img)

## Log

### 2026-11-22 - The movie's sides: three attempts, stopped (write-up)
- Harry: the mission failed screen fix works (black sides, Restart and Quit fine). The cutscene piece is still there
  (a saved game's mission select, then the mission's movie: the mission select's picture at the movie's left).
- Tried, none helped: the loading layer left undrawn under a movie; the movie's clear given its whole target; no
  layer drawn while a movie is on screen. Harry's log of the last run has neither the "widened" nor the "left undrawn
  under a movie" line.
- Learned: the player 0x105F50 runs the movie's frames itself (its own loop; it clears with 0x122350, black, the
  target, depth and stencil, at 0x1061C3 and 0x10624C, then draws the quad), so the game draws no layer meanwhile
  and its clear already covers the guest's whole target. The stale picture is therefore outside what the guest
  draws or clears: most likely host side, in how the renderer keeps or presents the frame when the movie's new
  720x480 target replaces the menu's (the area the pillarboxed movie leaves uncovered keeps the old picture).
- Next (per the 3-attempts rule, not started): reproduce at 21:9 on the GPU-VM from a save's mission select and
  capture which host target is presented during the movie and what fills its sides; the fix is then likely an SDK
  change (gated on the other games' smoke tests).

### 2026-11-22 - The mission failed screen on black (src/game/hud.cpp)
- Harry (3440x1440): the death screen's picture covered only a 16:9 middle; the world and the HUD showed at its sides.
- It is not a menu (Harry's log: "out of the menus" just before it) but the PDA's group "missionFail" (Restart, Quit),
  shown by StartDeathDialogue 0x5C660 (picture shell\wallp\fail.tex; sets 0x323068 until its entry finishes, then
  0x1FDC0 pauses play: 0x3297B0 = 1). The reset that hides missionFail (about 0x1F563) clears 0x3297B0.
- 0x5C660 hooked: from it until 0x3297B0 has been set and cleared again, while 0x3297B0 is set, the game's clear
  (0x122350, black, the target alone) runs just before the PDA's layer ([0x3297B8]) draws. Smoke OK; built on the
  GPU-VM. Harry confirmed it on his PC (3440x1440).

### 2026-11-22 - No layer drawn under a movie (src/game/hud.cpp)
- The clear fix did not help: Harry's log has no "widened" line, so the movie's clear already covered its target and
  the piece at the movie's side is drawn on each movie frame, under the movie: the mission select's menu going on.
- While a movie is on screen (gpu::State::pillar_fit) no layer (0x29FA0) is drawn; the first few are logged with
  the loading and HUD layers' addresses. On the console the movie covers them all. Boot movies unchanged on the
  GPU-VM; smoke OK. Harry to confirm; if the piece stays, it is not drawn as a layer.

### 2026-11-22 - A movie's clear covers its whole target (src/game/widescreen.cpp)
- The loading layer fix did not help Harry's case: his log has no "loading screen left undrawn" line. Loading a saved
  game's mission from the mission select, the movie draws to a new 720x480 target (821A1480, made as it starts), and
  the picture of the mission select stayed at the movie's sides at 3440x1440.
- While a movie is on screen (gpu::State::pillar_fit) the game's clear (0x122350) is given the whole target as its
  viewport and the viewport put back after; a line is logged when that widens it. The boot movies (GPU-VM): no
  change, their clear already covered the target. Smoke OK. Harry to confirm.

### 2026-11-22 - The loading screen left undrawn under a movie (src/game/hud.cpp)
- Harry (3440x1440): a cutscene after the hub showed a grey piece and the ends of the LOADING text at the movie's sides.
- The flow loads a mission while its opening movie plays (LoadMission, PlayMovie, LoadScreen); the loading layer
  ("loadScreenLayer", kept at 0x340F60 by 0xD36A0) goes on drawing under the movie, which on the console covers it.
  Here the movie is fitted at 4:3 in the middle, so the layer's sides showed. While a movie is on screen
  (gpu::State::pillar_fit) that layer is left undrawn.
- Ruled out on the GPU-VM with probes: the movie's clear covers the whole 640x480 target, and the window's back
  buffer is cleared each frame. The first mission's movie (from the menus) draws no layer under it, so the case
  is Harry's to confirm. Smoke OK.
- Next: the death screen (a 16:9 picture in the Restart/Quit dialogue; the world and HUD show at its sides at 21:9).

### 2026-11-22 - The shops whole on a wide frame (src/game/hud.cpp, src/game/widescreen.cpp)
- Harry (3440x1440): in the gun shop the stat bars sat apart from their frames, the counts and "9mm" off their
  labels; the bootlegger's list text was cut off at its left. The shops draw in the gameplay frame with no menu up,
  so hud_layout moved each of their pieces to its own side. Now hud_layout is on only while the HUD's own layer
  (0x32FA30) draws through 0x29FA0; the shops, like the menus, stay whole in the middle.
- GPU-VM 1920x800: Doc's shop together in the middle (its text box and Back/Buy were at the frame's edges), the
  HUD's health, cash and item count still at the edges, as is the "Buy some meds" prompt; in the alley the ammo,
  Lloyd's panel and the pager icons at the edges as before. The gun shop and bootlegger not reached on foot.

### 2026-11-22 - Achievements checked on the GPU-VM: the story's end, the weapons (src/game/achievement_checks.cpp)
- FC_FLOW_START=N (a test): the loaded save's flow step (0x32ACE0, profile +0x224) set to N before 0x27C30 applies
  it, so Load Game starts flow.lua's GameFlow[N] (from 1: 55 the S041 movie then the credits, 10 the first hub).
  Writing 0x337070 after the load (0xD1590) did nothing: the flow had already taken the step.
- The story's end on GPU-VM (Gangsta profile, FC_FLOW_START=55): "the story finished (difficulty 0.500)", Vessel and
  Hard Road unlocked, Soldier Through not; the credits rolled. PlayCreditsMovie is called once in all the scripts
  (step 56), so the m9_3 level check is gone: a continue from m9_3's autosave reaches it with no level loaded.
- Weapons: items 350.. are the store's offers, not owned (0x84080 sets them from inventory.lua's WeaponsUnlocked as
  each mission is reached), so Armed Up and Full Arsenal would have come from the story alone. Now Armed Up is a gun
  shop purchase (0xE3590 pays, then 0x31D70(weapon) on 0x325320, returning to 0xE38DD) and Full Arsenal all 20 owned
  at once: the hub stash's counts (20 shorts at 0x3254FC, restored with a save; the hub deposits carried weapons,
  0x31E30) or a holster. GPU-VM in the hub: "weapons owned 1 of 20", 3 with FC_GIVE_WEAPONS=7,14. A purchase not
  tried (the gun shop not found on foot).
- Disarms and medals were right: the disarm shop sets 290+n after paying (0xE5AB0), a medal pickup 370+mission.
- Doc's shop is an in-world picker (an arrow over the counter, A buys), not a menu of rows: the pointer does nothing
  there yet.

### 2026-11-22 - Sharp text and HUD (SDK sharp-p8; src/game/hud.cpp); the weapon keys as Game actions (SDK game-actions)
- Game actions (SDK dd5b311, gated on pgr2, pgr1, eon, agentunderfire, nightfire): GameInfo::actions, a title's own
  keyboard actions after the console's inputs in the key tables (settings.toml [keys] by name, the key capture,
  Default keys), a "Game actions" table on the Controls page, input::ActionHeld. Bulletproof's weapon_1..3 (1, 2,
  3) are read as each goes down in the 0x86400 hook, in place of the fixed WM_KEYDOWN digits.
- Sharp text: every texture the menus bind is P8 (headless probe: five sheets, each with its palette 0x400 before
  it), which Sharpen2D left out; and the game draws its menus and HUD through its own cameras (src/game/aspect.cpp),
  so the renderers never counted them as 2D. SDK: Sharpen2D takes P8 with the palette bound (the replacement keeps
  the palette's hash and stands in only while it is bound); State::flat_2d marks a game's own 2D; P8 sheets up to
  256 texels four times. Here: the layer draw 0x29FA0 and the keyboard 0x59330 set flat_2d; sharpen_2d on.
- Headless (REF frames, XBE_SHARPEN_DUMP): the PDA frame, icons, backgrounds and both fonts sharpened, white glyphs
  from their palettes. GPU-VM (D3D12, 1920x800, render 200%): 14 sheets sharpened (the HUD's and the weapon icons
  too); P8 replacements bound with no palette mismatches (a diagnostic build); the frames differ exactly at the
  text's, icons' and frame's edges. At 800 lines the text is ~1.7 times its 480 and the change is small; at 1440 it
  is three times. Setting "Sharp text and HUD" (Graphics).
- The keyboard harness note: the UI preview's 6 failing checks fail on fork main too (not from this).

### 2026-11-22 - Weapon keys: real input on Windows; 1 takes turns through the arms' holsters
- GPU-VM (C:\bpctl\run.ps1 gains digit keys and "wheel N": the cursor put on the window, mouse_event's wheel):
  with FC_GIVE_WEAPONS=7,14,1 in the alley, real 2, 3, a notch down, a notch up and 1 logged slots 2, 3, 0, 3, 0.
- Thrown weapons: there is no seventh holster (player +0x664 on is the ammo table, eight types 8 bytes apart, as
  0x31A60 copies it). Grenades, molotovs and sticky bombs are class 1 (the weapon table's +0x60), so they go in the
  arms' holsters with the pistols (FC_GIVE_WEAPONS=21: holster 0), and 1 reaches them: no fourth key needed.
- 1, 2, 3 now start from the holster after the last chosen, so the two arm holsters take turns (the weapon put away
  goes where the drawn one was): with a grenade in 0 and the Gat in 1, 1 four times chose 0, 1, 0, 1. SMOKE OK.
- The store (the hub's weapon shop, "WeapShop" / shopLayer, a list box with left and right arrows) cannot be reached
  headless from a new game; left for a save that is there.

### 2026-11-22 - The pointer on the name keyboard (src/game/menu_pointer.cpp)
- The keyboard ([0x329A1C]; up while [0x329A10] is 2 and its +0xAA5 clear) is run by 0x597A0, which 0x1E3C0 jumps
  to after the PDA's frame (so it is handled from the 0x49F00 hook). Five rows (+0xA6C counts) of 16 byte keys from
  +0xC (11 a row, 5 rows a page, +0xA60 the page): +0 code (0x10000 backspace, 0x10001..6 the command keys, 0x20
  space), +4 width, +8 can be chosen. Cursor +0xA64 row, +0xA68 key, +0xA80 the key kept between rows; A types the
  cursor's key (0x59590). Drawn always in one place: on the GPU-VM shot at 1920x800, x 626..1300 over 14 key widths
  (a command key two widths for each of its own), rows 39.4 apart from y 346: 119.6..524.0 and 207.6 + 23.6 a row
  in the menus' 640x480.
- The pointer moving onto a key makes it the cursor (with the d-pad's sound, "COMMON_PAGER_INCREASE_OPTION"); a
  click plays A over the cursor's key and nothing off the keys.
- Headless (profile creation, FC_POINTER): the pointer at (300, 300) took the cursor to row 3 key 3 (w), at
  (307, 243) to row 1 key 3 (c); a click typed "c"; a click at x 600 did nothing (still "c"). SMOKE OK.

### 2026-11-22 - The pointer on sliders and volumes (src/game/menu_pointer.cpp, src/common/menu_mouse.cpp)
- Harry: the pointer in the screens it left to the keys (this entry: the sliders; the name keyboard and store next).
- Read (dumps of Options > Camera and Audio/Video): the sensitivities and the three volumes are one item kind,
  vtable 0x2B93A4, input 0x4A260: left (flag 1) and right (2) move its node's value (+0xA0; the volumes 95, 95, 85)
  by its record's step (+0xC4); on/off rows are 0x2B93BC, buttons 0x2B9370. A held d-pad steps once, then repeats
  slowly (headless: one frame of LEFT 95 -> 94; 30 frames 2 steps).
- The rows' extent across widened to 94..555 (the Options pages' bar; the title's is 144..494). On the highlighted
  slider's row, a press on the value column's left half (415..486) lowers a step and its right half raises one,
  held repeating ten a second after 0.4 s; the wheel over it is a step a notch instead of moving the highlight.
  Each step is the d-pad for one read of the pad.
- Headless (FC_POINTER, .local/click_run.sh with timed releases): a click at x 450 on Sound Effect Volume 95 -> 94,
  at x 520 back to 95; a 1.3 s hold stepped 8. SMOKE OK; wincheck clean.

### 2026-11-22 - Black bars beside the movie after leaving a mission (src/game/widescreen.cpp)
- The grey (51) at the sides of the title movie after Quit: headless with XBE_CLEAR_LOG=1, the game's clear 0x122350
  clears target 021A1180 to FF333333 for about five frames at boot and as a mission is left (the loading screen's),
  then the movie draws to another target, 021A4F80; on a wide frame the stale grey stayed beside the 4:3 movie. The
  hook turns that colour black. Headless: no FF333333 clears left. GPU-VM, 1920x800, the same Quit: black bars. SMOKE OK.

### 2026-11-22 - The fade to black covers a wide frame's sides (src/game/hud.cpp)
- Harry: ending a mission, the fade blackened the middle only; the sides should go too.
- The fade is a layout layer, "fadeLayer" (0x336E30), its group "fadeGrp" (0x336E34), both found by 0x8E220: one
  black quad laid out for the console's screen, kept centred (the HUD's layout and the menus' 4:3 both leave a piece
  about the middle there), so at 2.4:1 the world stayed lit at the sides. While that layer draws (0x29FA0, already
  hooked for hide_hud) its group's x scale (+0x10, identity) is 8 times wider, then put back.
- GPU-VM, 1920x800, pause menu > Quit > Yes: before, the middle black and the alley lit at the sides; after, black
  edge to edge. SMOKE OK. Seen there too: the title movie after it has dark grey (51) bars at its sides, the game's
  own clear showing beside the 4:3 movie (the SDK clears black); fixed in the next entry.

### 2026-11-21 - The mouse wheel and 1, 2, 3 change weapons (src/game/weapon_keys.cpp)
- Harry: the wheel to change guns, and 1, 2, 3 on the keyboard.
- Read: the holsters are the player +0x5BC, 7 slots of 0x1C (+0 weapon, +0xC class, +0x10 in use): 0 and 1 pistols
  (class 1), 2 SMG (2), 3 rifle / shotgun / heavy (3), 4 the left hand, 5 the right (held). Black's tap (0x86400)
  calls 0x46C00 (the choice, player +0x3C94, to the next slot with a weapon; 0x45400 shows it), then after 0.25 s
  (0x334F58 against 0x2B83FC) the player's event 10 draws it; 0x334F60 (the press latch) gates that code. The
  Lua AddWeapon is 0x469B0 (weapon, holster), its ammo 0x1CD00 on the holsters (ammo type, rounds).
- Before 0x86400: a notch down is the game's own step, up the step backwards, drawn 0.1 s after the last notch;
  1, 2, 3 choose that class's holster (the other pistol when one is held) and draw at once. Off in menus, in the
  inventory and while 0x3297B0 is set. The wheel in play went nowhere before (menu_mouse took and dropped it).
- Headless (FC_GIVE_WEAPONS=7,14,1, FC_PAD_CMD's new WHEELUP / WHEELDOWN / KEY1-3): the hand held MP, AK, Gat,
  AK, Gat, Nine, MP, Gat in turn as asked. SMOKE OK; wincheck clean. Next: Harry's try on Windows.

### 2026-11-21 - PC Settings as a page of the PDA (Y on Options); the pointer's first place
- Harry: the other ports' in-game settings page; then "use the GPU-VM to diagnose without me".
- src/game/pc_page.cpp: the Codes > View page ("CodesView", list box cheatListBox) is lent while ours is up. Its
  handler 0xCFF70 fills the list from seven cheats at 0x2FE028 (12 bytes: id, name hash, on byte) shown while
  0x81020(0x140 + i) says unlocked, each row a layout group (musicItemGrp/trackGrp0..5: an icon and a label, so six
  rows); A calls 0xCF3A0(entry), then 0xCFE30 draws the visible rows again from their entries (0x51B40 label from
  the hash, 0x51550 icon on/off). Ours: the table holds our rows (ids 0xFC500000+i) and is put back when the page
  closes; 0x81020 shows six; 0xCF3A0 steps ours; left/right (the handler's flags 1 and 2; up/down are 4/8, A 0x10)
  step back/on before the game's handler, then 0xCFE30; rows not on/off have their tick box's alpha 0. A row's text
  is one of two labels a slot (0x51B40 draws again only for another entry), rewritten in place (a record with +4 0
  is shown from its own text, 0xF8870). The title label menuNameLabels_codes answers "PC Settings" while ours is up;
  the prompt table's CodesView entry gets A "Change". Pushed as an item's next menu (0x4A220): 0x49610 by name,
  first item current, 0x489D0, 0x49F20, from the PDA manager's frame (0x49F00 hook).
- Headless: Y opened it, Next Page Display > Graphics > Game, left/right Field of View 110, 100, 90; B restored the
  table. GPU-VM (pause menu > Options > Y): the three pages drawn right, "PC Settings" title, A Change.
- The pointer: the first place it was seen counted as a move, so a pointer standing over the window highlighted the
  row under it when a menu came up (GPU-VM: the title's Options instead of New Game). Now it only starts there.

### 2026-11-21 - Quality of life: skip intro, hide HUD (F9), field of view, Quit Game, sharp text and HUD
- Harry asked for the other ports' quick wins and field of view (the in-game PC Settings page is next).
- skip_intro (src/common/game_info.cpp, GameInfo::hide_file): Ark\movies' sierra, gglogo, DolbyN, inter, havok and
  teaser reported missing; the game passes over them: the title's Press START about 10 s into boot (45 s before).
- hide_hud (src/game/hud.cpp): the HUD is the layout layer "HUDLayer" (0x4F350 looks it up in the HUD's layer set
  0x3311A0, kept at 0x32FA30); a layer is drawn by 0x29FA0 (ecx the layer, its top groups +4/+8 by their vtable's +0xC,
  0x52720 for a group: drawn while +0x7C). That draw is skipped for the HUD's layer. F9 in play (no menu up)
  switches it and saves (GameInfo::message). Headless: the message icon and START gone.
- fov_percent 50 to 150 (src/game/aspect.cpp): the world's view 0x334CF0 (camera 0x334E20; the gameplay frame
  0x86890), vertical field +0xD8 = 0.585 rad from its constructor 0x121F10; widened around each setup (0x1221E0,
  0x122020): tan(half) times the setting. Headless at 130%: m00 1.437 for 1.868; 50 smaller, more of the alley.
- Quit Game (src/game/pc_settings.cpp): the prompt table's "Main Menu" entry (0x329DE4, the title's menu) gets our
  label "Quit Game" on X (the same string table entry trick as PC Settings); X there calls app::Quit. Headless: the
  bar shows it, X quit ("quit asked for").
- sharpen_2d tried and taken back out: on GPU-VM with XBE_SHARPEN_DUMP not one texture was sharpened (gpu::Sharpen2D
  takes swizzled textures up to 512 texels, not palettized; this game's 2D pictures are none of those, the button
  pictures a P8 512x64). Sharp text here would need the SDK to take them: not done.

### 2026-11-21 - The pointer on the dialogues ("Are you sure...?"); the cursor hidden in play
- Harry: the load dialogue (Yes / No) did not follow the mouse, and the cursor showed in play. A dialogue is a menu
  manager of its own on the heap (here 0x820FFE54, layer 0x82100354), its top menu's record named "Dialogue" (items
  dlgOpt0..3, the unused ones skipped, +0x10 1); found by dumps with No, Yes, No highlighted (its menu +4 moved
  between 0x82102030 and 0x82102014). Its layer is drawn by the PDA's camera: rows at 250.4 and 273.3 by the formula,
  250.6 and 273.5 in Harry's screenshot. 0x49F00's hook now tracks any manager whose top menu is "Dialogue" with rows
  placed, and leaves the PDA's menu under it alone meanwhile.
- The cursor: ShowArrow came after MenuMouse's return for "no menu up", so leaving a menu never took the arrow away;
  now before it (the runtime hides the cursor over the game when the arrow is off).
- Headless (the kept profile disk): Load Game, the save, the dialogue: hover moved Yes, No, Yes; a click on Yes loaded
  the save ("5:38am - New York City").

### 2026-11-21 - The launcher's and the game's icon (launcher/icon.ico)
- Harry: the image he gave is the launcher's icon, not the README's. launcher/icon.ico from it (256 PNG, 128 to 16,
  the black outside the rounded corners made transparent): the launcher build and the game exe (CMakeLists, resource
  1) both take it. The README's top image is now a banner cut from the alley shot (docs/img/bulletproof-banner.webp).

### 2026-11-21 - README in Juiced's style, shots from GPU-VM
- Harry gave an icon and asked for the README in the Juiced README's form, every shot taken on GPU-VM.
  GPU-VM: C:\Testing\50cent-recomp at main (33d4201), the disc extracted into assets, built with CI=1 build.bat;
  task BulletproofRun (C:\bpctl\run.ps1, Juiced's driver for Bulletproof.exe) and BulletproofDesk (a desktop capture,
  hidden window). Injected letter keys do not reach the game there (Enter, arrows, Esc and F-keys do), so the route is
  driven by FC_PAD_CMD (C:\bpctl\pad.txt): START through the logos, New Game, Thug, X continue without saving, both
  movies skipped, then the alley. Scripts in .local/gpuvm (g.sh, shot.sh; TAIL= pad lines before the shot).
- Shots at 1920x800: all off, best (render 200%, MSAA 4x, sharpen 50%), MSAA 2/4/8, FXAA, SMAA, render 200%,
  sharpen 50/100; hero, 16:9 and 4:3 while firing (the HUD fades out a few seconds after the last action); the title
  menu, the pause menu, a movie, and the settings window's Display, Graphics, Game and Achievements pages (Y on the
  Options page; Page Down pages it). docs/img/bulletproof-*.webp: banner, hero, wipe (animated), widescreen, menus,
  pc-settings (animated), the four close-up comparisons (lamp post and vent, 2x).

### 2026-11-21 - The pointer in the lists too (saves, profiles, messages)
- Harry: on Load Game the pointer did not move the highlight; a click selected the save already highlighted. Lists
  were left to A anywhere. A list box item's node is the list (0x4A590, its d-pad): +0x1B8 the highlighted slot,
  +0x1BA the entries, +0x1BC the visible slots, +0xA0 each slot's entry, +0x1C0 its look (+0x28 the spacing, 0.0295),
  +0x190 a node on the highlighted slot (found by a trace: it moved with the highlight). Each visible slot with an
  entry is now a row; the pointer steps the list there with its own 0x54B20 / 0x54A10 and sounds.
- Headless with a saved profile (two runs, the disk kept): New Game's profile list ("aaa", "- Create New Profile -"
  at 195.3 and 218.0 as drawn); the pointer moved the highlight down, up and down again, and a click on "Create New
  Profile" opened the name keyboard. The keyboard (a text box) keeps A anywhere.

### 2026-11-21 - A pointer in the PDA's menus: the row under it highlighted, the left button selects it
- Harry asked (passed on from the Agent Under Fire session) for a menu pointer of the game's own; the menus had
  none (the mouse played A, B and the d-pad). src/game/menu_pointer.cpp, src/common/menu_mouse.cpp.
- The PDA's menus (title, Options, pause menu) are one menu manager, 0x32A170 (0x1F240: 0x4A7A0, layer "menuLayer"
  at +0). Found by diffing heap dumps with the title's row moved (0, 1, 2, 1): the top menu's +4 is the highlighted
  item. Menu: +0 first item, +4 highlighted, +8 manager, +0xC record (+0 node hash, +4 item count, +0x28 item names).
  Item: +0 vtable (+0xC highlight, +0x10 unhighlight; pools 0x48C10..0x490F0 give each kind its own, list box
  0x2B93EC, text box 0x2B9404), +4 record (+0x14 its layout node's hash, +0x60/+0x80 the names above and below),
  +0x10 1 when skipped. Names hash with 0x110700 into the manager's map at +0x18 (0x495C0); nodes into the layer's
  map at +0xBC (0x2CF90): +0x10 a 4x4 matrix, +0x84 the parent. 0x49F00 (ecx the manager) runs the top menu's frame;
  the d-pad's move (0x49B19): unhighlight, menu+4, highlight, 0x489D0 (the bar), 0x48BC0(8, 1.0) (the sound).
- Each of the manager's frames (0x49F00 hooked) places the top menu's rows: y = 240.75 - 771 * the node's height
  (480 lines), the bar from x 144 to 494 (640), both measured from the title's highlight bar. A row the pointer
  moves onto is highlighted the d-pad's way; standing still it leaves the keys' choice alone. The left button plays
  A over the highlighted row, nothing off the rows; menus with a list or text box, or under two rows (messages,
  the profile prompts) keep A anywhere. The pointer: a chrome arrow with a navy edge and a shadow (Windows: the
  window subclassed for WM_SETCURSOR and the runtime's hide timer, as AUF's), shown while a menu is up.
- Headless (FC_POINTER=<file> "x y buttons" in the menus' 640x480): title rows placed at 232.5/254.2/275.9 as drawn;
  hover moved the highlight (Options, Load Game); a click on Load Game opened "No save games were found", on
  Options the Options page (rows 189..276 as drawn); a click off the rows did nothing; the pause menu's top (6 rows)
  highlighted Tasks under the pointer. wincheck clean for both files. Next: Harry's Windows run (the cursor's look,
  hover and clicks), then the store's and sliders' left and right if wanted.

### 2026-11-21 - Achievements: 22 on the story, difficulty, unlocks and cash (src/common/achievement_set.cpp)
- Harry confirmed the HUD at any aspect (the ammo count too), the mouse, the pop-in and the button pictures; asked for
  achievements, the full set, named for the game's missions.
- Mapped (.local/progress_re.md): the story is flow\flow.lua (Ark\Scripts\scripts.ark); each level loads through
  LoadMission (0x268D0), whose 0x348D0 keeps "mX_Y" as X*10+Y at 0x32DD94. Story order m0 Queens, m2 Safe House,
  m1 Junkyard, m3 Chinatown, m4 Disused Subway, m5 Projects, m6 Docklands, m7 City Morgue, m8 Little Italy, m9
  Vessel, then PlayCreditsMovie (Lua 0x26400). Difficulty: the float at 0x323618 from the table at 0x2FE010 (Playa
  0.05 .. G-Unit Soldier 0.8; found by diffing Thug and G-Unit Soldier dumps). Unlocks: one byte an item, the
  profile's at 0x323498 (committed at checkpoints, 0x34B60 -> 0x811B0) and those since at 0x32DDB8; numbers from
  0x810A0 (disarms 290, weapons 350, Grizz medals 370). Cash: the player (0x32DCFC) +0x3C28.
- The set: a mission done when the next one's first level loads (a memory condition on 0x32DD94); Vessel and the two
  difficulty finishes from a hook on PlayCreditsMovie at level 93; the medal, disarm and weapon counts each gameplay
  frame (src/game/achievement_checks.cpp, awarded only after being seen short); cash through the player pointer.
- SDK eaf6f0a: achievements::Award(id), for those a hook awards (condition "0=1"); on fork main as merge c06f201
  (fork main had moved to 068400d, launcher changes; the merge gated again on all five and smoke OK).
- Headless: pokes in the alley (level 20, item 370 in the new unlocks, cash 10000) unlocked Queens, Decorated and
  Pocket Money, nothing before them. Gate passed on pgr2, pgr1, eon, agentunderfire, nightfire; smoke OK; wincheck clean.
- Not seen yet: the credits hook (headless cannot finish the story), and whether a weapon's byte means bought or
  only offered in the store. Left out for now: music and videos (their real item numbers not mapped).

### 2026-11-21 - The ammo count kept with its panel on any frame (SDK 2d14c76)
- Harry (D3D12, 21:9 switched to 4:3 with F1; screenshot 10-00-05): the ammo count "12/.." stayed at its 16:9 place,
  cut off at the 4:3 frame's right edge, while its panel, the mail icon and the START prompt moved in. The text's
  positions are D3DVSDT_SHORT3 (decl v0 type 0x35, stride 6: R16G16B16A16_SINT on the GPU), and gpu::ProgramExtent,
  the renderers' CPU pass for a piece's extent, gave up on anything but floats, so HudMove never moved it. Headless
  it was right ("13/20" inside its panel at 4:3, p_ammo.txt: p_cam plus RT/LT from 8400, REF=8520): the reference
  renderer takes the extent from its transformed vertices.
- ProgramExtent now reads 16 bit integer and normalised positions. The live change itself works (the panel moved).
- Gate passed on pgr2, pgr1, eon, agentunderfire and nightfire; smoke OK; wincheck clean.
- In-engine cutscenes at 4:3 (655c027): Harry confirmed.

### 2026-11-21 - In-engine cutscenes at 4:3 on any frame (src/game/widescreen.cpp, app::HoldAspect)
- Harry: on an ultrawide, in-engine cutscenes (the alley door after Lloyd's lock) showed the 3D across the whole
  frame, the game's letterbox bars short of the sides. They are the game's 3D, not movies: run by its scripts with
  cinematicMode (0xC7190 stores it at 0x32FAD8; 0x4B190 draws the CinemaLayer's bars while it is set).
- While 0x32FAD8 is set the gameplay frame hook holds the frame at 4:3 (new SDK app::HoldAspect, d0fc75c): the views
  take 4:3 (src/game/aspect.cpp), the renderers remake their targets and pillarbox it, as the 4:3 setting does.
- Evidence: boot.txt headless at 21:9, cinematicMode 1 from 215.2 to 219.4 s ("the 3D views at 1.333:1", then back at
  2.390:1); ref_11820 is that doorway at 4:3 with the bars across it. Gate passed on pgr2, pgr1, eon, agentunderfire
  and nightfire; smoke OK; wincheck clean.
- Open: Harry reports the HUD not moving to its new places when the aspect setting changes in play; headless a live
  change (frame_aspect file, 21:9 to 16:9) moves it as at boot, and the D3D11/12 code takes the new shape each
  present. Details asked.

### 2026-11-20 - Frame rate in play restored: the HUD's CPU extent pass only for draws that could be HUD
- Harry: a big FPS drop on loading into the game with d02c877. Cause: with hud_layout on (all of play), the D3D11/12
  renderers ran every draw with a vertex program through gpu::ProgramExtent, the CPU interpreter, up to 1024 vertices
  each, before HudMove's cheap checks threw the 3D world out. Counted in the alley frame: about 1700 draws and 123k
  vertices a frame through it; with the new gpu::HudPiece gate (no depth test, a program, a flat matrix) first, 7
  draws and 28 vertices.
- SDK 50bf254 (fork main): gate passed on pgr2, pgr1, eon, agentunderfire and nightfire; smoke OK; wincheck clean;
  the HUD capture at frame 8160 unchanged.
- Next: Harry's Windows play test of the frame rate and the HUD.

### 2026-11-20 - Cutscenes whole at 4:3 on any frame; the HUD kept at the frame's edges (src/game/widescreen.cpp)

Harry: asked for every cutscene at 4:3 whatever the aspect setting, and the HUD to fit the frame.
- Cutscenes: the player 0x105F50 draws each movie frame as one screen quad, x 0 to 720 and y -64 to 536 (the 4:3
  picture enlarged to a widescreen TV's width, top and bottom cut off), from its 512x512 LIN_A8R8G8B8 texture. While
  it plays, gpu::State::pillar_fit (SDK) fits such a quad whole into the frame's height and squeezes it to 4:3, bars
  at the sides. The player's own black bars over the top and bottom 64 lines (cutscene +0x24, 0x10641E through
  0x121B70) would cover the fitted picture: cleared while it plays, put back after. Headless M001 at 16:9 and 21:9:
  the whole picture at 4:3, nothing over it. The movie quad is drawn by a screen program (B0064C31), so
  GameInfo::squeeze_screen_programs is on and the program shaders take the vertical scale too.
- HUD: drawn through a flat camera of the views' aspect (aspect.cpp), each piece a vertex program draw with depth
  testing off at a fixed place laid out for 16:9 (headless captures at 4:3, 16:9 and 21:9: its pieces' screen x
  went as 1 / aspect; the START prompt and the partner's box ran off a 4:3 frame's edges and sat in a 16:9 middle of
  a 21:9 one). While the gameplay frame 0x86890 runs, gpu::State::hud_layout 16:9 (SDK) moves each piece to one side
  of the middle sideways to keep its distance from that edge as at 16:9; its shape is the camera's, right already.
  Text is drawn by another program (B00652F1, v0 scaled plus an offset, no matrix), so where a piece lands comes
  from running its program on the CPU. Headless at 4:3 and 21:9: the START prompt and the box with Lloyd Banks's
  portrait, name and bar at the corners; the crosshair and "Task added" in the middle. Menus (2D for 4:3) stay at
  4:3 in the middle (GameInfo::hud_aspect).
- The pause menu is drawn in the gameplay frame, its pieces through the same programs: at 21:9 its name, portrait and
  Back moved off the phone. hud_layout is off while a menu is up (fc::MenuUp); headless 21:9 the phone whole again,
  the HUD still at the edges. The cutscene headless at 4:3: the whole picture fills the frame.
- SDK c850160 (gated on pgr2, pgr1, eon, agentunderfire, nightfire). SMOKE OK; wincheck clean; shadercheck 23 of 23. Next: Harry to check cutscenes, the HUD and the menus on Windows at
  his 21:9 and at 16:9 and 4:3.

### 2026-11-20 - Cutscene flicker: Sofdec's frames out of order (movie_server.cpp frame lock)

Harry: still flickering with SDK 034fe74; the aspect setting works. The diagnosis of 2026-11-19 was wrong: the
movie buffer itself goes back. A probe on 0x114CB0 (the lock and conversion of a frame, synchronous on the main
thread, about 8 ms) dumped 0x822B2B00 after each frame (M001: 512 wide, pitch 2048), matched against ffmpeg's decode
of assets M001.sfd (1053 frames, no steps back): headless, 29 and 35 of the first 300 frames were out of display
order, mostly a P frame shown three early (486, 490, 488, 489; 498, 502, 500, 501), a different set each run. At a
camera cut that is the next shot's frame and then two of the last again, as in Harry's video. The game takes the
frame (0xF8C40 -> 0x14B960 mwPlyGetCurFrm), releases it (0x14B3E0), and the cutscene player 0x105F50 converts it
after (0x114CB0 -> 0x14C500); the server pass 0x1556F0 decodes on its own thread meanwhile. A lock between the
server pass and 0xF8C40 alone: still 25 and 26 per 300. Held from 0xF8C40 until 0x114CB0 converts the frame (not past
it when 0xF8C40 converted it already, +0x5E4): 0 of 600, twice, no extra repeats or skips, the 100 ms way out never
taken. SDK 034fe74 (a target's lock pending until its bytes change) is not the fix but harmless; left as is.
SMOKE OK; wincheck clean. Harry to check the 5:38am cutscene on Windows.

### 2026-11-19 - Movie flicker fixed in the SDK; the aspect setting applies at once

Harry (Windows): the mouse camera "works great"; no edge pop-in noticed since 3f5f6e3; asked for the aspect setting
to apply at once; a video of the flicker (.local/win, 3440x1440 at 240 fps, the cutscene cs\M001.sfd).
- Flicker: brightness per video frame and frames around a cut show the shot before the cut again for two game
  frames, without its caption. The movie frame is one 512x512 LIN_A8R8G8B8 surface 0x822B2B00, also a render
  target (the game draws two quads into it, then samples it; CAP=1 headless); CRI Sofdec locks it (MarkDirty) once a
  movie frame from its decoding thread and writes it there. The renderers' LatchCpuWrite spent the lock on one look:
  run behind the title, it could look before the decoder's writes, see the same bytes, and keep the GPU's copy until
  the next lock. SDK 034fe74 (fork branch target-lock-watch): the lock stays pending for 16 looks until the bytes
  change (D3D11 and D3D12). Headless cannot show it (the reference renderer reads guest memory): Harry to check.
- Aspect: SDK c9e6489 (Juiced's 167e22d cherry-picked: GameInfo::aspect_live, the console always a widescreen TV);
  src/game/aspect.cpp writes app::FrameAspect() to the views' aspect 0x349478 as each view is set up (0x1221E0,
  0x122020, the only readers). Headless (a frame_aspect file in the run folder): 1.778, then 1.330, then 2.390 in
  the alley within a frame each; snapshots at each show the same height and more at the sides. The 2D stays as the
  game draws it at 4:3 and 16:9, squeezed into a centred 16:9 when wider (hud_aspect 16:9). SMOKE OK; wincheck clean.

### 2026-11-19 - The view's culling planes 1.25 times wider (view_cull.cpp)

Harry saw pop-in at the 16:9 frame's edges. The aspect is not the cause: the game picks 720x480 widescreen
(0x16FA8 -> 0x11AD80) and sets the views' aspect 0x349478 to 16:9, which both the projection (0x11EBF0) and the side
planes (0x121D00: view +0xF0..+0x12C) take. The planes do cull: the alley's frame 8400 headless drew 1701 draws, 2078
with them twice as wide (FC_CULL_WIDEN=2). On the console a TV's overscan hid the edges, so objects whose bounds are
smaller than what they draw were cut out of sight. src/game/view_cull.cpp builds the planes 1.25 times wider and
taller (zoom argument divided), the projection unchanged. Whether anything besides drawing reads those planes is not
known (30 functions touch +0xF0..+0x120 offsets); to watch on Windows: auto-aim taking targets just off the screen.
Also: the aspect setting is read at boot (app::Widescreen is static) and the 4:3 mode's aspect is 720/480 = 1.5,
not 4:3. SMOKE OK.

### 2026-11-19 - The mouse turns the camera directly (mouse_aim.cpp)

Harry: the mouse played the right stick (its ease in and the stick's speed). The third person camera's update 0x94000
(thiscall: camera, angles {pitch, yaw} in radians at 0x322FE4, step) was found by logging the callers of the pad's
channel reader 0x11F1F0 for the right stick's channels while in the alley (0x94088, 0x94136; 0x5E2C0 also reads
them into an input record, use unknown). src/game/mouse_aim.cpp adds the mouse's counts to the angles before the
update (0.0008 rad a count at 100%, as AUF), so the game's pitch clamp holds; GameInfo::mouse_direct leaves the
stick to the controller. Yaw grows to the right, pitch downwards (snapshot at -0.87: the sky); the game's y invert
byte 0x32346E is set at a first boot (stick up looks down), and the mouse ignores both invert bytes (up looks up).
Headless FC_MOUSE_TEST=dx,dy,from,to: 50 counts a call for 61 calls turned yaw 2.44 rad; 40 up held pitch at the
clamp -0.8727, 40 down at 1.0472. SMOKE OK; wincheck clean.
Also: harness runs were twice stopped at 120 s ("Terminated", nothing in the log), as by another session's
pkill -x game; .local/hrun.sh builds the harness as bp50. Widened culling (FC_CULL_WIDEN, parked in .local/view_cull.cpp)
still to compare. To check on Windows: the feel and speed, and aiming modes (0x5E2C0's record may have had the mouse).

### 2026-11-13 - Saves listed as damaged: the find pattern "~*.*" (SDK: DOS wildcards)

Harry: Load Game showed his save as damaged. The game lists a save by finding its profile file in the save's folder
with FindFirstFile (0x12CDB2) on "%s~*.*" (format 0x2D3558); the file is "~P5628E0D4", no dot. The SDK's WildMatch
was literal, so nothing matched; the game then zeroes the find data's times and the entry's flag at +0xB2 marks it
damaged. SDK fork branch dos-wildcards (60fc186): a pattern ending in ".*" also matches a name with no dot, as on the
console. Headless: a profile made (.local/p_mkprofile.txt), then Load Game lists "aaa" with "Load Profile"; Yes opens
U:\006100610061\~P5628E0D4 and the game goes on into the mission's cutscene.
Still open from his report: cutscene flicker on Windows, pop-in at the 16:9 window's edges, the 4:3/16:9 setting
changing nothing, the mouse turning the camera as a stick rather than directly.

### 2026-11-12 - Harry's crash at 452 s: a Lua script error's longjmp (SDK: declared continuations)

Harry played front end, profile and the alley on Windows (most of it as expected) and the game stopped at 452 s:
"indirect call to 000E7A28 has no recompiled function", guest trap at the return address 82E5383C. 0xE7A28 is the
instruction after "call 0x1E0E28" in 0xE7A00: _setjmp3 (the CRT's, "VC20" at +0x20) in Lua 5.0's
luaD_rawrunprotected (L->errorJmp at +0x50: the handler's previous, its jmp_buf at +4, its status at +0x44). A
script error's luaD_throw (0xE79C0) longjmps there (0x1E0DAC; 0x1E0E21 jmp [buf+0x14]), into the middle of a
recompiled function. SDK fork branch setjmp-continuations: [xbe] continuations lists such addresses; each is made a
function and a continuation (as C++ catch blocks' are), and the function holding the call to setjmp catches the
X86Resume the jump throws. config/bulletproof.toml: continuations = [0xE7A28]; the only call to _setjmp3.
Test (temporary, removed): once, a protected call running luaD_throw(L, 2): returned 2, L's handler as before, esp
as before, the game went on into the alley. SMOKE OK; wincheck clean. His log also shows 60.0 fps, 16.66 ms a frame,
about 2 ms of host work a frame in the alley (RX 7900 XT).

### 2026-11-12 - PC settings from the game's Options page ("Y PC Settings")

Harry asked for PC settings in the game's menu. A seventh row on the pause menu's Options page means a new node in
the menu layout (fe.ark; rows are layout nodes matched by name to descriptors such as 0x2E9C48's handler 0xD0AB0 for
the autosave row): not done. Agreed instead: a prompt in the PDA's button bar. src/game/pc_settings.cpp: the prompt
table 0x329C40 (0x1EDAA0 fills it; an entry a menu: name, then labels for B, A, X, Y) gets our label on the Y of
"Options" (0x329C7C) and "MainOptions" (0x329C90); the string table's lookup 0xF81D0 answers its hash with an entry
over our own record (the hash, 0, length, size, "PC Settings"), loaded by the game's own 0xF8870; 0x1F010 (a menu
coming up, its name at [+0xC] + 6) tells the Options pages; there Y opens the settings window (app::ToggleOverlay) and
the game does not see it. Headless: "Y PC Settings" in the bar of the title's Options and the pause menu's, Y asks for
the window. SMOKE OK; wincheck clean.

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
