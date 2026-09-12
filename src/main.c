
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/ctrl.h>
#include <string.h>
#include "../include/app.h"

void scan_packages(BrowserList *out);
void settings_load(Settings *s);
void settings_save(const Settings *s);
void list_select(BrowserList *list, int index);
void list_update_scroll(BrowserList *list, float dt, int enabled);

static void ensure_dirs(void) {
    sceIoMkdir(MRW_DATA_ROOT, 0777);
    sceIoMkdir(MRW_PKG_ROOT, 0777);
    sceIoMkdir(MRW_STAGE_ROOT, 0777);
}

int main(void) {
    ensure_dirs();

    Settings settings;
    settings_load(&settings);

    BrowserList browser;
    scan_packages(&browser);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    unsigned int last_buttons = 0;
    while (1) {
        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);

        unsigned int pressed = pad.buttons & ~last_buttons;
        last_buttons = pad.buttons;

        if (pressed & SCE_CTRL_UP)
            list_select(&browser, browser.selected - 1);
        if (pressed & SCE_CTRL_DOWN)
            list_select(&browser, browser.selected + 1);

        // Triangle = real rescan of all configured paths.
        if (pressed & SCE_CTRL_TRIANGLE)
            scan_packages(&browser);

        // Start exits this alpha scaffold.
        if (pressed & SCE_CTRL_START)
            break;

        list_update_scroll(&browser, 1.0f/60.0f, settings.smooth_scroll);
        sceKernelDelayThread(16666);
    }

    settings_save(&settings);
    sceKernelExitProcess(0);
    return 0;
}
