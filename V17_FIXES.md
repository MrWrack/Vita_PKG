# v1.7 – conversion + flicker fix

Hardware-test fixes:

## Screen
- Replaced single framebuffer with two 960x544 CDRAM framebuffers.
- Rendering occurs only into the back buffer.
- Buffer swap is synchronized to VBlank.
- This is intended to remove the visible flicker/tearing from v1.6.

## VPK -> PKG
Press X on a `.vpk` item:
- parses the VPK as a ZIP archive
- supports stored and DEFLATE-compressed files
- blocks unsafe relative paths and encrypted ZIP entries
- preserves every relative path
- calculates SHA-256 per file
- writes MRW-PKG v2
- saves output to `ux0:/data/MrWrackPKG/pkg/<name>.pkg`
- rescans the list after conversion

Press X on a `.pkg` item to run the existing homebrew MRW-PKG install flow.

This custom `.pkg` is MRW-PKG, not Sony retail PKG.
