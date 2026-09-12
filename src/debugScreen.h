#ifndef MRW_DEBUG_SCREEN_H
#define MRW_DEBUG_SCREEN_H
#include <stdint.h>

#define COLOR_BLACK   0xFF000000u
#define COLOR_WHITE   0xFFFFFFFFu
#define COLOR_GREEN   0xFF00FF00u
#define COLOR_YELLOW  0xFF00FFFFu
#define COLOR_RED     0xFF0000FFu

int  psvDebugScreenInit(void);
void psvDebugScreenShutdown(void);
int  psvDebugScreenPrintf(const char *format, ...);
void psvDebugScreenClear(uint32_t color);
void psvDebugScreenSetFgColor(uint32_t color);
void psvDebugScreenPresent(void);

#endif
