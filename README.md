# MrWrack PKG Converter v1.1 Alpha

PS Vita homebrew converter/installer project.

## Current install pipeline

MRW-PKG -> verify/extract -> `ux0:data/MrWrackPKG/package_temp` ->
validate `eboot.bin` + `sce_sys/param.sfo` -> prepare `head.bin` ->
ScePromoterUtil -> LiveArea.

**Launch is always manual. Installation never auto-launches an app.**

## GitHub

The only workflow is:

`.github/workflows/build.yml`

GitHub Actions uses the VitaSDK container and uploads the generated VPK artifact.

## Important v1.1 limitation

The PromoterUtil plumbing is now implemented, including the PAF module sequence
used by VitaShell. A correct package `head.bin` still requires the template plus
metadata/HMAC patching. v1.1 deliberately refuses to fabricate an invalid
`head.bin`.

This keeps the installer safe to continue developing without claiming the final
LiveArea install has already been hardware-tested.
