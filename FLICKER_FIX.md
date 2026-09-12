# v1.9 Flicker Fix

The remaining flicker was addressed by changing the rendering model.

Changes:
- the UI is no longer redrawn continuously in the input loop
- redraw only happens when selection/status/content changes
- the currently displayed front buffer remains untouched while idle
- buffer swaps are synchronized around VBlank
- input polling continues independently at a short interval
- double buffering from v1.7 remains enabled

This avoids repeatedly clearing and swapping the entire 960x544 framebuffer
while the user is not doing anything.
