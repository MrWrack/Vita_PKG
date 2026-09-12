
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/ctrl.h>
#include <psp2/debugScreen.h>
#include <stdio.h>
#include <string.h>
#include "../include/app.h"
#include "../include/install_flow.h"

#define printf psvDebugScreenPrintf

void scan_packages(BrowserList *out);
void settings_load(Settings *s);
void settings_save(const Settings *s);
void list_select(BrowserList *list, int index);
void list_update_scroll(BrowserList *list, float dt, int enabled);
int launch_title_manual(const char *title_id);

static MrwInstallProgress g_progress;
static int g_last_result = 0;

static void ensure_dirs(void){
    sceIoMkdir(MRW_DATA_ROOT,0777);
    sceIoMkdir(MRW_PKG_ROOT,0777);
    sceIoMkdir(MRW_STAGE_ROOT,0777);
    sceIoMkdir("ux0:data/MrWrackPKG/package_temp",0777);
}

static const char *type_name(ItemType t){
    switch(t){
        case ITEM_VPK: return "VPK";
        case ITEM_PKG: return "MRW-PKG";
        case ITEM_INSTALLED: return "Installed";
        default: return "File";
    }
}

static void draw_ui(const BrowserList *b){
    psvDebugScreenClear(COLOR_BLACK);
    psvDebugScreenSetFgColor(0x0000FF00);
    printf("MRWRACK PKG CONVERTER  v1.5 TEST\n");
    psvDebugScreenSetFgColor(0x00FFFFFF);
    printf("Homebrew VPK / MRW-PKG installer\n");
    printf("------------------------------------------------------------\n");

    if(b->count <= 0){
        psvDebugScreenSetFgColor(0x0000FFFF);
        printf("\nNo VPK/MRW-PKG files found.\n");
        printf("Scan paths:\n");
        printf(" ux0:/downloads/\n ux0:/pkg/\n ux0:/data/MrWrackPKG/pkg/\n");
    } else {
        int start=b->selected-4; if(start<0) start=0;
        int end=start+9; if(end>b->count) end=b->count;
        for(int i=start;i<end;i++){
            BrowserItem *it=(BrowserItem*)&b->items[i];
            if(i==b->selected){
                psvDebugScreenSetFgColor(0x0000FF00);
                printf("> ");
            } else {
                psvDebugScreenSetFgColor(0x00FFFFFF);
                printf("  ");
            }
            printf("[%s] %s\n", type_name(it->type), it->name);
        }
    }

    psvDebugScreenSetFgColor(0x00FFFFFF);
    printf("\n------------------------------------------------------------\n");
    printf("X Install   Triangle Rescan   Square Launch   START Exit\n");

    psvDebugScreenSetFgColor(
        !strcmp(g_progress.stage,"Error") ? 0x000000FF :
        !strcmp(g_progress.stage,"Done")  ? 0x0000FF00 : 0x0000FFFF
    );
    printf("\nStatus: %s  %d%%\n",g_progress.stage,g_progress.percent);
    printf("%s\n",g_progress.message);
    if(g_last_result)
        printf("Result: %d / 0x%08X\n",g_last_result,(unsigned)g_last_result);
}

int main(void){
    ensure_dirs();
    psvDebugScreenInit();

    Settings settings; settings_load(&settings);
    BrowserList browser; memset(&browser,0,sizeof(browser)); scan_packages(&browser);

    memset(&g_progress,0,sizeof(g_progress));
    strcpy(g_progress.stage,"Ready");
    strcpy(g_progress.message,"Select an MRW-PKG and press X");

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    unsigned last=0;

    while(1){
        draw_ui(&browser);

        SceCtrlData pad; memset(&pad,0,sizeof(pad));
        sceCtrlPeekBufferPositive(0,&pad,1);
        unsigned pressed=pad.buttons & ~last; last=pad.buttons;

        if(pressed&SCE_CTRL_UP) list_select(&browser,browser.selected-1);
        if(pressed&SCE_CTRL_DOWN) list_select(&browser,browser.selected+1);

        if((pressed&SCE_CTRL_CROSS) && browser.count>0){
            BrowserItem *it=&browser.items[browser.selected];
            if(it->type==ITEM_PKG){
                g_last_result=mrw_install_homebrew_pkg(it->path,&g_progress);
                if(g_last_result==0) scan_packages(&browser);
            } else {
                g_last_result=-200;
                strcpy(g_progress.stage,"Error");
                strcpy(g_progress.message,"X installs MRW-PKG files only");
            }
        }

        if(pressed&SCE_CTRL_TRIANGLE){
            scan_packages(&browser);
            g_last_result=0;
            strcpy(g_progress.stage,"Ready");
            strcpy(g_progress.message,"Package list rescanned");
        }

        if((pressed&SCE_CTRL_SQUARE) && browser.count>0){
            BrowserItem *it=&browser.items[browser.selected];
            if(it->type==ITEM_INSTALLED && it->title_id[0]){
                g_last_result=launch_title_manual(it->title_id);
            } else {
                g_last_result=-201;
                strcpy(g_progress.stage,"Error");
                strcpy(g_progress.message,"Square only launches installed-app entries");
            }
        }

        if(pressed&SCE_CTRL_START) break;
        list_update_scroll(&browser,1.0f/60.0f,settings.smooth_scroll);
        sceKernelDelayThread(16666);
    }

    settings_save(&settings);
    sceKernelExitProcess(0);
    return 0;
}
