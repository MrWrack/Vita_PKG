# v1.6 Screen Renderer

The previous black screen was caused by the local debug-screen compatibility layer
being a no-op renderer.

v1.6 replaces it with a real PS Vita framebuffer renderer:
- allocates a 960x544 CDRAM framebuffer
- presents it through `sceDisplaySetFrameBuf`
- renders ASCII text using an embedded 8x12 bitmap font
- draws the converter menu, file list, controls and error/result codes
- synchronizes presentation to VBlank

This version no longer relies on the fake no-op debugScreen implementation.
