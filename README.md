# MrWrack PKG Converter v1.4 Test

This is the hardened first PS Vita test candidate.

## Install chain

`MRW-PKG v2 -> SHA-256 verify -> clean extraction -> validate -> fresh head.bin -> PromoterUtil -> LiveArea`

Important v1.4 fixes:
- recursively clears `package_temp` before every install
- clears temp again after success or failure
- never reuses an old/package-provided `head.bin`
- stricter MRW-PKG magic/path validation
- includes a matching PC MRW-PKG v2 packer
- GitHub Actions runs a package-format self-test before Vita compilation
- install still never auto-launches

## Test controls

- D-pad Up/Down: select
- X: install selected MRW-PKG
- Triangle: rescan package folders
- Square: launch manually when an installed-app entry is available
- Start: exit

Scan paths:
- `ux0:/downloads/`
- `ux0:/pkg/`
- `ux0:/data/MrWrackPKG/pkg/`

## Make a matching MRW-PKG on PC

Extract a homebrew VPK to a folder, then run:

`python tools/mrw_pkg_v2.py <folder> <output.pkg>`

The source folder must contain `eboot.bin` and `sce_sys/param.sfo`.

## GitHub build

Only one workflow exists:

`.github/workflows/build.yml`

It runs the MRW-PKG self-test, fetches the GPL VitaShell `head.bin` template,
builds with VitaSDK, verifies the VPK, and uploads the artifact.

This source is still awaiting the first real PS Vita hardware test.
