# v2.4 Safe Startup

The app still returned immediately to LiveArea on real hardware.

This build changes startup architecture:

- one 2 MB CDRAM framebuffer instead of two
- no package scan before the first UI frame
- settings load happens after the first UI frame
- VPK/PKG scan happens only when entering those menus
- startup checkpoints are written to:
  `ux0:/data/MrWrackPKG/startup.log`

Log checkpoints:
1. main entered
2. framebuffer init OK
3. first UI frame shown
4. settings loaded
5. opening VPK/PKG menu
6. scan complete

If the app still closes, open `startup.log` in VitaShell and the last line tells
us exactly which startup stage failed.

All LiveArea and controller fixes from v2.2/v2.3 remain.
