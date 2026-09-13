# v2.8 Build Fix

Fixes the GitHub Actions compile error in `draw_ui()`.

The project type is `BrowserItem`, not `BrowserEntry`.
Corrected:
- `BrowserEntry *e` -> `BrowserItem *e`
- `BrowserEntry *sel` -> `BrowserItem *sel`

The clean v2.7 UI remains otherwise unchanged.
