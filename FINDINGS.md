# 50 Cent: Bulletproof recomp - findings

What the work has established, by topic. PROGRESS.md has the dated log with the evidence for each item. Each item
is marked **[console]** (how the Xbox, its XDK or the runtime behaves: the fix is in the SDK) or **[game]** (this
game's own code and data, with its addresses).

## The image
- **[game]** One xbe, `default.xbe` (SHA-1 0abb01d5e8b323f13d26ac98b619e42220795093), title 0x56550042, region flags 7,
  from the "USA, Europe" disc. Base 0x10000, entry 0x11074, kernel thunk 0x2AD700.
- **[game]** Sections: .text, D3D, XACTENG, XGRPH, DSOUND, WMADEC, PSFD00, XPP (code); .rdata, .data, DOLBY,
  $$XTINFO/$$XTIMAGE, dspfx (9 DSP effects), dirtydisk_en, .XTLID.
- **[game]** Libraries are all 1.0.5849: XAPILIB, XBOXKRNL, D3D8 (not LTCG), XACTENG, XGRAPHC, DSOUND, LIBCMT. No
  network libraries. Statically linked middleware: CRI ADX/AHX/AFS (May 2005), CRI Sofdec, Havok, Lua, Dolby.

## Data
- **[game]** Everything is under `ark/`: .ark archives, .xtf textures, .adx audio, .sfd movies, XACT .xwb/.xsb banks.
  The first boot copies `Ark\Shell` and `Ark\Player` to the Z: cache partition (wbinvd at 0x113800 and 0x114Bxx
  around the copy are ignored, which is harmless).

## Front end
- **[game]** First boot: logo movies (sierra, gglogo, DolbyN, inter, havok, a CRI Sofdec/ADX screen) and the teaser,
  each skipped by START, then the Title Screen (New Game, Load Game, Options). New Game: Choose Difficulty (Playa,
  Hustler, Thug (default), Gangsta, G-Unit Soldier), then the profile screen (A Create Profile: a name keyboard; X
  Continue without saving, confirmed with No as the default). The intro sequence seq\S001.sfd and the mission
  cutscene cs\m001.sfd follow; START skips each. The front end draws about 116 draws a frame.

## Rendering
- **[game]** The front end draws into a 720x480 off-screen target, then one full-screen quad (vertex shader B0064BF1)
  copies it to the back buffer. The movies draw the same way from a 640x480 LIN_A8R8G8B8 texture.
- **[game]** 0x24E880, an unnamed D3D library function (probably D3DVertexBuffer_Lock2), writes NV2A method 0x1710
  directly through MakeSpace.
- **[game]** Vertex shaders are created once, then placed in program memory by the game's own allocator (state at
  [0x365A14]: +0x20 the next free slot of 0x88; each shader record +0x04 handle, +0x0C size, +0x10 its slot or -1).
  0x12A4A5 calls LoadVertexShader(handle, next free slot), and 0x12A54D SelectVertexShader. The size comes from
  0x250E10 (GetVertexShaderSize, unnamed: it reads the shader object's +8).
- **[console]** The XDK's shader object keeps the instruction count at +8 (5849 CreateVertexShader 0x2509F6; +0 a
  refcount of 1, +4 the flags). The host's record held 0 there, so every program loaded at slot 0 over the last one,
  and the full-screen copy ran another program's code (DP4s against c10 to c13): every frame was black. Fixed in the
  SDK (fork branch bulletproof-vs-size c3bd337), which now writes the count.
- **[game]** No recorded push buffers: the one push buffer tag store (0x257468, 0x20001, in 0x257380) is reached only
  under the library's Begin and DrawVertices, which the host replaces; game code stores no tags (PushBufferScan).
- **[game]** No game code writes the push buffer: all 14 users of the push pointer [0x25BDB0] and every reader of
  the device [0x25BDA8] are in the D3D section. About 40 of those library functions are unnamed and run their own
  code. The only one seen pushing so far is 0x24E880.

## Movies (CRI Sofdec)
- **[game]** Player state 0x49D6A0 (0x14A900): eight handles from +0x6C, 0x2D4 each, +0x00 in use, +0x3C the SFD
  handle, +0x60 being served; +0x170C switches the server off. The server 0x1556F0 (callers 0x1557D0, 0x1557F0, run
  through the CRI server table 0x132770) tests +0x170C once, then 0x1556A0 / 0x1555D0 serve each handle.
- **[game]** Stopping or resetting a movie zeroes +0x3C, stops the SFD and restores it (0x14AE20, 0x149A10), under
  the switch from 0x14B190 and 0x14A4F0 (0x1554A0(1) ... (0)), but not from 0x14B140 (the game's 0xF8D00).
- **[console]** The console keeps the server and the movie thread apart by thread priority on one core; the host
  runs them side by side. src/game/movie_server.cpp makes switching off wait for a pass under way, and puts the
  unswitched stop under the switch.
- **[game]** CRI's svm lock (0x132930 / 0x132950) calls 0x12EF00 / 0x12EF80, a critical section on 0x3AC5A0
  (imports [0x2AD750] and [0x2AD74C]): a real lock, not the cause.

## Controls
- **[game]** In play: left stick walks; LT aims (over the shoulder camera); RT fires while aiming; X reloads.
