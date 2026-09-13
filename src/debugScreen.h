#ifndef MRW_DEBUG_SCREEN_H
#define MRW_DEBUG_SCREEN_H
#include <stdint.h>

#define COLOR_BLACK   0xFF000000u
#define COLOR_WHITE   0xFFFFFFFFu
#define COLOR_GREEN   0xFF00FF00u
#define COLOR_YELLOW  0xFF00FFFFu
#define COLOR_RED     0xFF0000FFu
#define COLOR_DARK    0xFF101010u
#define COLOR_PANEL   0xFF181818u
#define COLOR_DIM     0xFF808080u
#define COLOR_NEON    0xFF00E85Au

int  psvDebugScreenInit(void);
void psvDebugScreenShutdown(void);
int  psvDebugScreenPrintf(const char *format, ...);
void psvDebugScreenClear(uint32_t color);
void psvDebugScreenSetFgColor(uint32_t color);
void psvDebugScreenPresent(void);
void psvDebugScreenSetXY(int x, int y);
void psvDebugScreenFillRect(int x, int y, int w, int h, uint32_t color);
void psvDebugScreenRect(int x, int y, int w, int h, uint32_t color, int thickness);
void psvDebugScreenLine(int x0, int y0, int x1, int y1, uint32_t color);
void psvDebugScreenCircle(int cx, int cy, int r, uint32_t color);

#endif
