# v1.5.6 LiveArea spec fix

The previous 120x120 icon size was incorrect for `sce_sys/icon0.png`.

Corrected:
- `sce_sys/icon0.png`: 128x128
- indexed/palette PNG
- no alpha channel
- workflow validator now requires 128x128
- no PIL dependency in GitHub Actions

This specifically targets VitaShell install error `0x8010113D`.
