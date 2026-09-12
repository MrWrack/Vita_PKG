# MrWrack PKG Converter v2.1

This build addresses the requested hardware-test issues:

1. Installation/build validation:
   - LiveArea validator now checks each image by its intended format.
   - icon0.png is exactly 128x128, 8-bit RGBA.
   - no obsolete "palette type 3" requirement.

2. LiveArea icon:
   - file is exactly 128x128 px.
   - artwork stays in an approximately 120x120 safe area.
   - transparent RGBA margin is retained.

3. Rendering/flicker:
   - UI redraws only after state changes.
   - double buffering remains.
   - completed back buffer is switched immediately after VBlank.
   - no continuous full-screen clear/swap loop.

4. VPK -> MRW-PKG:
   - VPK Files menu.
   - X converts selected VPK.
   - output goes to ux0:/data/MrWrackPKG/pkg/.

5-8. Controls:
   - Triangle = Delete
   - Square = Refresh
   - Circle = Back
   - X = Select / Convert / Install

9. Menus:
   - Home
   - VPK Files / Convert
   - PKG Files / Install
   - Settings
   - About
   - Exit
   - Delete confirmation screen
   - file size/path information and clearer footer/status placement
