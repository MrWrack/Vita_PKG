# v1.5.4 Workflow Fix

GitHub could not find `scripts/validate_livearea_pngs.py`.

Fix:
- LiveArea PNG validation is embedded directly in `.github/workflows/build.yml`.
- No separate validation script is required.
- `icon0.png` is still checked for indexed PNG mode and exact 120x120 size.
