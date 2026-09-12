#include "debugScreen.h"
#include <stdio.h>
#include <stdarg.h>
int psvDebugScreenInit(void){ return 0; }
int psvDebugScreenPrintf(const char *format, ...){
    va_list ap; va_start(ap,format); int r=vprintf(format,ap); va_end(ap); return r;
}
void psvDebugScreenClear(uint32_t color){ (void)color; }
void psvDebugScreenSetFgColor(uint32_t color){ (void)color; }
