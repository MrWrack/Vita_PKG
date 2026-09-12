
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/ctrl.h>
#include "debugScreen.h"
#include <stdio.h>
#include <string.h>
#include "../include/app.h"
#include "../include/install_flow.h"
#include "../include/vpk_convert.h"

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
    printf("MRWRACK PKG CONVERTER  v1.9 TEST\n");
    psvDebugScreenSetFgColor(0x00FFFFFF);
    printf("Homebrew VPK -> MRW-PKG converter / installer\n");
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
    printf("X Select   TRI Delete   SQUARE Refresh   CIRCLE Back   START Exit\n");

    psvDebugScreenSetFgColor(
        !strcmp(g_progress.stage,"Error") ? 0x000000FF :
        !strcmp(g_progress.stage,"Done")  ? 0x0000FF00 : 0x0000FFFF
    );
    printf("\nStatus: %s  %d%%\n",g_progress.stage,g_progress.percent);
    printf("%s\n",g_progress.message);
    if(g_last_result)
        printf("Result: %d / 0x%08X\n",g_last_result,(unsigned)g_last_result);
    psvDebugScreenPresent();
}

int main(void){
    ensure_dirs();
    psvDebugScreenInit();

    Settings settings; settings_load(&settings);
    BrowserList browser; memset(&browser,0,sizeof(browser)); scan_packages(&browser);

    memset(&g_progress,0,sizeof(g_progress));
    strcpy(g_progress.stage,"Ready");
    strcpy(g_progress.message,"Select VPK to convert or MRW-PKG to install");

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    unsigned last=0;
    int redraw=1;

    while(1){
        if(redraw){
            draw_ui(&browser);
            redraw=0;
        }

        SceCtrlData pad; memset(&pad,0,sizeof(pad));
        sceCtrlPeekBufferPositive(0,&pad,1);
        unsigned pressed=pad.buttons & ~last; last=pad.buttons;

        if(pressed&SCE_CTRL_UP){ list_select(&browser,browser.selected-1); redraw=1; }
        if(pressed&SCE_CTRL_DOWN){ list_select(&browser,browser.selected+1); redraw=1; }

        if((pressed&SCE_CTRL_CROSS) && browser.count>0){
            BrowserItem *it=&browser.items[browser.selected];

            if(it->type==ITEM_VPK){
                char created[MRW_MAX_PATH];
                memset(created,0,sizeof(created));

                g_last_result=mrw_convert_vpk_to_pkg(
                    it->path,
                    created,
                    sizeof(created),
                    &g_progress
                );

                if(g_last_result==0){
                    scan_packages(&browser);
                    snprintf(
                        g_progress.message,
                        sizeof(g_progress.message),
                        "Created: %s",
                        created
                    );
                }
            } else if(it->type==ITEM_PKG){
                g_last_result=mrw_install_homebrew_pkg(
                    it->path,
                    &g_progress
                );
                if(g_last_result==0)
                    scan_packages(&browser);
            } else {
                g_last_result=-200;
                strcpy(g_progress.stage,"Error");
                strcpy(g_progress.message,"Unsupported item");
            }
            redraw=1;
        }

        if(pressed&SCE_CTRL_SQUARE){
            scan_packages(&browser);
            g_last_result=0;
            strcpy(g_progress.stage,"Ready");
            strcpy(g_progress.message,"Package list refreshed");
            redraw=1;
        }

        if((pressed&SCE_CTRL_TRIANGLE) && browser.count>0){
            BrowserItem *it=&browser.items[browser.selected];

            if(it->type==ITEM_VPK || it->type==ITEM_PKG){
                int r=sceIoRemove(it->path);
                g_last_result=r;

                if(r>=0){
                    strcpy(g_progress.stage,"Done");
                    strcpy(g_progress.message,"Selected file deleted");
                    scan_packages(&browser);
                    redraw=1;
                } else {
                    strcpy(g_progress.stage,"Error");
                    snprintf(
                        g_progress.message,
                        sizeof(g_progress.message),
                        "Delete failed: 0x%08X",
                        (unsigned)r
                    );
                    redraw=1;
                }
            } else {
                g_last_result=-202;
                strcpy(g_progress.stage,"Error");
                strcpy(g_progress.message,"Delete is only for VPK/PKG files");
                redraw=1;
            }
        }

        if(pressed&SCE_CTRL_CIRCLE){
            g_last_result=0;
            strcpy(g_progress.stage,"Ready");
            strcpy(g_progress.message,"Back");
            redraw=1;
        }

        if(pressed&SCE_CTRL_START) break;
        list_update_scroll(&browser,1.0f/60.0f,settings.smooth_scroll);

        /*
         * Keep the input loop responsive without forcing a full framebuffer
         * redraw every frame. The displayed front buffer stays untouched
         * until redraw is requested.
         */
        sceKernelDelayThread(5000);
    }

    settings_save(&settings);
    psvDebugScreenShutdown();
    sceKernelExitProcess(0);
    return 0;
}
