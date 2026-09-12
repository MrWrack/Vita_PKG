# v2.2 installation fix for 0x8010113D

The previous v2.1 icon used RGBA transparency. Real Vita LiveArea rules are stricter.

This build uses:
- icon0.png: 128x128, 8-bit indexed PNG (color type 3), no alpha
- pic0.png: 960x544, 8-bit indexed PNG, no alpha
- bg0.png: 840x500, 8-bit indexed PNG, no alpha
- startup.png: 280x158, 8-bit RGBA (alpha allowed)
- template.xml retained

The artwork inside icon0 still keeps visual padding around the motif, but the PNG
itself no longer uses transparency. The margin is rendered as the icon background.

The GitHub workflow rejects a build if any of these files have the wrong IHDR
dimensions, bit depth, or color type.
