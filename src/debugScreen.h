#ifndef MRW_DEBUG_SCREEN_H
#define MRW_DEBUG_SCREEN_H
#include <stdint.h>
#define COLOR_BLACK 0x00000000
int psvDebugScreenInit(void);
int psvDebugScreenPrintf(const char *format, ...);
void psvDebugScreenClear(uint32_t color);
void psvDebugScreenSetFgColor(uint32_t color);
#endif
