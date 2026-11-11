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

## Rendering
- **[game]** The front end draws into a 720x480 off-screen target, then one full-screen quad (vertex shader B0064BF1)
  copies it to the back buffer. The movies draw the same way from a 640x480 LIN_A8R8G8B8 texture.
- **[game]** 0x24E880, an unnamed D3D library function (probably D3DVertexBuffer_Lock2), writes NV2A method 0x1710
  directly through MakeSpace.
