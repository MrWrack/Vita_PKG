# v1.1 status

Implemented:
- clean GitHub workflow at `.github/workflows/build.yml`
- VitaSDK VPK artifact build
- correct package temp path owned by MrWrack PKG Converter
- app tree validation
- PAF load/unload sequence
- ScePromoterUtil init/promote/exit sequence
- no auto-launch
- third-party GPL attribution

Remaining before final on-device installer:
- port MRW-PKG v2 extraction + SHA-256 verification into refactored tree
- incorporate a GPL-compatible head.bin template
- patch TITLE_ID / CONTENT_ID into head.bin
- calculate required package HMAC fields
- on-device install test
