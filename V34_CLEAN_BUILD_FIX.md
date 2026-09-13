# v3.4 Clean Build Fix

Rebuilt from the last known-good v3.1 source instead of patching the broken v3.3 main.c.

Kept:
- GitHub + MrWrack black/neon-green UI
- cyber background
- VPK convert loading screen
- PKG install loading screen
- existing working Vita input loop and package logic

This avoids the v3.2/v3.3 issue where controller code was accidentally inserted into draw_ui().
