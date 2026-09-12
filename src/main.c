
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/ctrl.h>
#include <string.h>
#include "../include/app.h"
#include "../include/install_flow.h"

void scan_packages(BrowserList *out);
void settings_load(Settings *s);
void settings_save(const Settings *s);
void list_select(BrowserList *list, int index);
void list_update_scroll(BrowserList *list, float dt, int enabled);
int launch_title_manual(const char *title_id);

static MrwInstallProgress g_progress;

static void ensure_dirs(void){
    sceIoMkdir(MRW_DATA_ROOT,0777);
    sceIoMkdir(MRW_PKG_ROOT,0777);
    sceIoMkdir(MRW_STAGE_ROOT,0777);
    sceIoMkdir("ux0:data/MrWrackPKG/package_temp",0777);
}
int main(void){
    ensure_dirs();
    Settings settings; settings_load(&settings);
    BrowserList browser; scan_packages(&browser);
    memset(&g_progress,0,sizeof(g_progress));
    strcpy(g_progress.stage,"Ready");
    strcpy(g_progress.message,"Select an MRW-PKG and press X to install");

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    unsigned last=0;
    while(1){
        SceCtrlData pad; memset(&pad,0,sizeof(pad));
        sceCtrlPeekBufferPositive(0,&pad,1);
        unsigned pressed=pad.buttons & ~last; last=pad.buttons;

        if(pressed&SCE_CTRL_UP) list_select(&browser,browser.selected-1);
        if(pressed&SCE_CTRL_DOWN) list_select(&browser,browser.selected+1);

        if((pressed&SCE_CTRL_CROSS) && browser.count>0){
            BrowserItem *it=&browser.items[browser.selected];
            if(it->type==ITEM_PKG){
                int r=mrw_install_homebrew_pkg(it->path,&g_progress);
                if(r==0){
                    // Rescan after install completes. No launch here.
                    scan_packages(&browser);
                }
            }
        }

        if(pressed&SCE_CTRL_TRIANGLE) scan_packages(&browser);

        // Launch remains a separate manual action for known installed entries.
        if((pressed&SCE_CTRL_SQUARE) && browser.count>0){
            BrowserItem *it=&browser.items[browser.selected];
            if(it->type==ITEM_INSTALLED && it->title_id[0])
                launch_title_manual(it->title_id);
        }

        if(pressed&SCE_CTRL_START) break;

        list_update_scroll(&browser,1.0f/60.0f,settings.smooth_scroll);
        sceKernelDelayThread(16666);
    }
    settings_save(&settings);
    sceKernelExitProcess(0);
    return 0;
}
