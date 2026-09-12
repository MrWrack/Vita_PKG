# MrWrack PKG Converter v2.2

This build specifically targets VitaShell installation error `0x8010113D`.

## LiveArea formats
- icon0.png: 128x128, 8-bit indexed PNG, no alpha
- pic0.png: 960x544, 8-bit indexed PNG, no alpha
- bg0.png: 840x500, 8-bit indexed PNG, no alpha
- startup.png: 280x158, 8-bit RGBA

The icon motif remains approximately 120x120 inside the 128x128 canvas, but the
outer margin is an opaque background rather than transparent alpha.

## Controls
- Triangle = Delete
- Square = Refresh
- Circle = Back
- X = Select / Convert / Install
- Start = Exit

## Menus
- Home
- VPK Files / Convert
- PKG Files / Install
- Settings
- About
- Exit
