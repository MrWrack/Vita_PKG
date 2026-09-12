
#include <psp2/io/fcntl.h>
#include <stdio.h>
#include <string.h>
#include "../include/app.h"

#define SETTINGS_PATH MRW_DATA_ROOT "/settings.ini"

static void defaults(Settings *s) {
    s->accent.r = 0;
    s->accent.g = 255;
    s->accent.b = 140;
    s->smooth_scroll = 1;
}

void settings_load(Settings *s) {
    defaults(s);
    SceUID fd = sceIoOpen(SETTINGS_PATH, SCE_O_RDONLY, 0);
    if (fd < 0) return;

    char buf[256];
    int n = sceIoRead(fd, buf, sizeof(buf)-1);
    sceIoClose(fd);
    if (n <= 0) return;
    buf[n] = 0;

    int r,g,b,scroll;
    if (sscanf(buf, "accent=%d,%d,%d\nsmooth_scroll=%d", &r,&g,&b,&scroll) == 4) {
        if (r>=0 && r<=255 && g>=0 && g<=255 && b>=0 && b<=255) {
            s->accent.r = (uint8_t)r;
            s->accent.g = (uint8_t)g;
            s->accent.b = (uint8_t)b;
        }
        s->smooth_scroll = scroll ? 1 : 0;
    }
}

void settings_save(const Settings *s) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "accent=%u,%u,%u\nsmooth_scroll=%d\n",
        s->accent.r, s->accent.g, s->accent.b, s->smooth_scroll);

    SceUID fd = sceIoOpen(SETTINGS_PATH,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd < 0) return;
    sceIoWrite(fd, buf, n);
    sceIoClose(fd);
}
