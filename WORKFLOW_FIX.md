# v1.5.5 Workflow Fix

GitHub Actions failed with:

`ModuleNotFoundError: No module named 'PIL'`

The VitaSDK container does not provide Pillow/PIL.

Fix:
- removed all PIL/Pillow usage from the GitHub workflow
- PNG validation now uses Python's built-in `struct` module only
- reads the PNG IHDR header directly
- verifies `icon0.png` is 120x120
- verifies PNG color type 3 (indexed/palette)
- no external Python package installation is required

The build can now continue directly to CMake after the validation step.
