
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
void scan_homebrew(BrowserList *out);
void scan_games(BrowserList *out);
void settings_load(Settings *s);
void settings_save(const Settings *s);
void list_select(BrowserList *list, int index);
int launch_title_manual(const char *title_id);

static MrwInstallProgress g_progress;
static int g_last_result = 0;

/*
 * Keep large runtime state out of main()'s stack.
 * BrowserList is ~170-180 KB and can overflow the Vita main-thread stack.
 */
static BrowserList g_browser;
static Settings g_settings;

static void startup_log(const char *text){
    SceUID fd=sceIoOpen(
        "ux0:/MrWrack-startup.log",
        SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,
        0666
    );
    if(fd>=0){
        sceIoWrite(fd,text,strlen(text));
        sceIoWrite(fd,"\n",1);
        sceIoClose(fd);
    }
}

static void exit_log(const char *reason){
    startup_log(reason);
}


typedef enum {
    SCREEN_HOME = 0,
    SCREEN_VPK,
    SCREEN_PKG,
    SCREEN_SEARCH_MENU,
    SCREEN_HOMEBREW,
    SCREEN_GAMES,
    SCREEN_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_DELETE_CONFIRM
} ScreenMode;

static ScreenMode g_screen = SCREEN_HOME;
static ScreenMode g_delete_return = SCREEN_HOME;
static int g_home_selected = 0;
static int g_file_selected = 0;
static int g_settings_selected = 0;
static int g_search_category_selected = 0;
static int g_delete_index = -1;

static const char *home_items[] = {
    "VPK Files / Convert",
    "PKG Files / Install",
    "Search",
    "Settings",
    "About",
    "Exit"
};
#define HOME_COUNT 6

static void ensure_dirs(void){
    sceIoMkdir(MRW_DATA_ROOT,0777);
    sceIoMkdir(MRW_PKG_ROOT,0777);
    sceIoMkdir(MRW_STAGE_ROOT,0777);
    sceIoMkdir("ux0:/data/MrWrackPKG/package_temp",0777);
}

static const char *type_name(ItemType t){
    switch(t){
        case ITEM_VPK: return "VPK";
        case ITEM_PKG: return "MRW-PKG";
        case ITEM_INSTALLED: return "Installed";
        default: return "File";
    }
}

static void format_size(uint64_t size, char *out, size_t n){
    if(size >= 1024ULL*1024ULL*1024ULL)
        snprintf(out,n,"%.2f GB",(double)size/(1024.0*1024.0*1024.0));
    else if(size >= 1024ULL*1024ULL)
        snprintf(out,n,"%.1f MB",(double)size/(1024.0*1024.0));
    else if(size >= 1024ULL)
        snprintf(out,n,"%.1f KB",(double)size/1024.0);
    else
        snprintf(out,n,"%llu B",(unsigned long long)size);
}

static int filtered_count(const BrowserList *b, ItemType type){
    int count=0;
    for(int i=0;i<b->count;i++)
        if(b->items[i].type==type) count++;
    return count;
}

static int filtered_index(const BrowserList *b, ItemType type, int visual_index){
    int n=0;
    for(int i=0;i<b->count;i++){
        if(b->items[i].type!=type) continue;
        if(n==visual_index) return i;
        n++;
    }
    return -1;
}

static BrowserItem *selected_filtered(BrowserList *b, ItemType type){
    int idx=filtered_index(b,type,g_file_selected);
    if(idx<0) return NULL;
    return &b->items[idx];
}

static void clamp_file_selection(const BrowserList *b, ItemType type){
    int count=filtered_count(b,type);
    if(count<=0){ g_file_selected=0; return; }
    if(g_file_selected<0) g_file_selected=0;
    if(g_file_selected>=count) g_file_selected=count-1;
}

static void draw_header(const char *section){
    psvDebugScreenClear(COLOR_BLACK);
    psvDebugScreenSetFgColor(COLOR_GREEN);
    printf("MRWRACK PKG CONVERTER  v3.4\n");
    psvDebugScreenSetFgColor(COLOR_WHITE);
    printf("VPK -> MRW-PKG   |   %s\n", section);
    printf("============================================================\n\n");
}

static void draw_footer(const char *controls){
    psvDebugScreenSetFgColor(COLOR_WHITE);
    printf("\n============================================================\n");
    printf("%s\n",controls);

    psvDebugScreenSetFgColor(
        !strcmp(g_progress.stage,"Error") ? COLOR_RED :
        !strcmp(g_progress.stage,"Done")  ? COLOR_GREEN : COLOR_YELLOW
    );
    printf("\nStatus: %s",g_progress.stage);
    if(g_progress.percent>0) printf("  %d%%",g_progress.percent);
    printf("\n%s\n",g_progress.message);
    if(g_last_result)
        printf("Result: %d / 0x%08X\n",g_last_result,(unsigned)g_last_result);
}

static void draw_home(void){
    draw_header("HOME");
    psvDebugScreenSetFgColor(COLOR_WHITE);
    printf("Choose a section:\n\n");

    for(int i=0;i<HOME_COUNT;i++){
        if(i==g_home_selected){
            psvDebugScreenSetFgColor(COLOR_GREEN);
            printf("  >  %s\n\n",home_items[i]);
        } else {
            psvDebugScreenSetFgColor(COLOR_WHITE);
            printf("     %s\n\n",home_items[i]);
        }
    }

    draw_footer("D-PAD Navigate   X Select   SQUARE Refresh   START Exit");
}

static void draw_file_screen(BrowserList *b, ItemType type){
    const char *title = type==ITEM_VPK ? "VPK FILES / CONVERT" : "PKG FILES / INSTALL";
    draw_header(title);

    int count=filtered_count(b,type);
    clamp_file_selection(b,type);

    if(count<=0){
        psvDebugScreenSetFgColor(COLOR_YELLOW);
        printf("No %s files found.\n\n",type==ITEM_VPK ? "VPK" : "MRW-PKG");
        psvDebugScreenSetFgColor(COLOR_WHITE);
        printf("Scanning:\n");
        printf("  ux0:/downloads/\n");
        printf("  ux0:/pkg/\n");
        printf("  ux0:/data/MrWrackPKG/pkg/\n");
    } else {
        int start=g_file_selected-4;
        if(start<0) start=0;
        int end=start+9;
        if(end>count) end=count;

        for(int vi=start;vi<end;vi++){
            int idx=filtered_index(b,type,vi);
            BrowserItem *it=&b->items[idx];
            char sz[32]; format_size(it->size,sz,sizeof(sz));

            if(vi==g_file_selected){
                psvDebugScreenSetFgColor(COLOR_GREEN);
                printf(" > %-38s %10s\n",it->name,sz);
            } else {
                psvDebugScreenSetFgColor(COLOR_WHITE);
                printf("   %-38s %10s\n",it->name,sz);
            }
        }

        BrowserItem *sel=selected_filtered(b,type);
        if(sel){
            char sz[32]; format_size(sel->size,sz,sizeof(sz));
            psvDebugScreenSetFgColor(COLOR_WHITE);
            printf("\nSelected:\n");
            printf("  Type: %s\n",type_name(sel->type));
            printf("  Size: %s\n",sz);
            printf("  Path: %s\n",sel->path);
        }
    }

    if(type==ITEM_VPK)
        draw_footer("X Convert   TRIANGLE Delete   SQUARE Refresh   CIRCLE Back");
    else
        draw_footer("X Install   TRIANGLE Delete   SQUARE Refresh   CIRCLE Back");
}

static void draw_settings(const Settings *s){
    draw_header("SETTINGS");
    const char *items[]={"Smooth scrolling","Accent info","Back"};
    for(int i=0;i<3;i++){
        if(i==g_settings_selected){
            psvDebugScreenSetFgColor(COLOR_GREEN);
            printf(" > %s",items[i]);
        } else {
            psvDebugScreenSetFgColor(COLOR_WHITE);
            printf("   %s",items[i]);
        }

        if(i==0) printf(": %s",s->smooth_scroll ? "ON":"OFF");
        if(i==1) printf(": RGB(%u,%u,%u)",
            (unsigned)s->accent.r,(unsigned)s->accent.g,(unsigned)s->accent.b);
        printf("\n\n");
    }
    draw_footer("D-PAD Navigate   X Change/Select   CIRCLE Back");
}

static void draw_about(void){
    draw_header("ABOUT");
    psvDebugScreenSetFgColor(COLOR_GREEN);
    printf("MrWrack PKG Converter\n\n");
    psvDebugScreenSetFgColor(COLOR_WHITE);
    printf("Version: 2.6\n");
    printf("Title ID: MRWPKG001\n\n");
    printf("Features:\n");
    printf("  - VPK -> MRW-PKG v2 conversion\n");
    printf("  - SHA-256 verification\n");
    printf("  - MRW-PKG installation\n");
    printf("  - File delete / refresh\n");
    printf("  - LiveArea branding\n\n");
    printf("MRW-PKG is a custom homebrew format, not Sony retail PKG.\n");
    draw_footer("CIRCLE Back");
}

static void draw_delete_confirm(BrowserList *b){
    ItemType type = g_delete_return==SCREEN_VPK ? ITEM_VPK : ITEM_PKG;
    BrowserItem *it=NULL;
    if(g_delete_index>=0 && g_delete_index<b->count)
        it=&b->items[g_delete_index];

    draw_header("DELETE FILE?");
    psvDebugScreenSetFgColor(COLOR_YELLOW);
    printf("This cannot be undone.\n\n");
    psvDebugScreenSetFgColor(COLOR_WHITE);
    if(it){
        printf("File:\n  %s\n\n",it->name);
        printf("Path:\n  %s\n\n",it->path);
    }
    printf("X = Confirm delete\n");
    printf("CIRCLE = Cancel\n");
    draw_footer("X Delete   CIRCLE Cancel");
}


static void ui_text(int x,int y,uint32_t color,const char *text){
    psvDebugScreenSetXY(x,y);
    psvDebugScreenSetFgColor(color);
    psvDebugScreenPrintf("%s",text);
}
static void ui_background(void){
    psvDebugScreenClear(0xFF050705u);
    psvDebugScreenFillRect(620,0,340,544,0xFF071009u);
    for(int i=0;i<12;i++)
        psvDebugScreenLine(650+i*25,65,515+i*25,520,0xFF0A3518u);
    psvDebugScreenFillRect(28,92,904,2,COLOR_NEON);
    psvDebugScreenFillRect(28,482,904,1,0xFF2A3E2Eu);

    psvDebugScreenCircle(68,47,24,COLOR_WHITE);
    psvDebugScreenFillRect(57,38,22,18,COLOR_WHITE);
    psvDebugScreenLine(58,39,53,31,COLOR_WHITE);
    psvDebugScreenLine(78,39,83,31,COLOR_WHITE);

    ui_text(102,28,COLOR_WHITE,"GitHub");
    ui_text(102,48,COLOR_NEON,"MRWRACK");
    ui_text(102,66,COLOR_DIM,"PS VITA TOOLS & MORE");
    ui_text(720,28,COLOR_WHITE,"HOMEBREW  GAMES  TOOLS");
    ui_text(754,50,COLOR_NEON,"BY MRWRACK");
}
static void ui_card(int x,int y,int w,int h,const char *title,const char *sub,int selected){
    uint32_t border=selected?COLOR_NEON:0xFF24452Fu;
    uint32_t fill=selected?0xFF0B2212u:0xFF101411u;
    psvDebugScreenFillRect(x,y,w,h,fill);
    psvDebugScreenRect(x,y,w,h,border,selected?3:1);
    if(selected) psvDebugScreenFillRect(x,y,5,h,COLOR_NEON);
    ui_text(x+24,y+18,selected?COLOR_NEON:COLOR_WHITE,title);
    ui_text(x+24,y+42,COLOR_DIM,sub);
    ui_text(x+w-28,y+29,selected?COLOR_NEON:COLOR_DIM,">");
}
static void ui_footer(void){
    ui_text(34,505,COLOR_WHITE,"X Select");
    ui_text(172,505,COLOR_WHITE,"O Back");
    ui_text(286,505,COLOR_WHITE,"[] Refresh");
    ui_text(438,505,COLOR_WHITE,"△ Options");
    ui_text(700,505,COLOR_DIM,"MrWrack PKG Converter v3.4");
}
static void draw_loading_screen(const char *title,const char *file,int install_mode){
    ui_background();
    ui_text(54,132,COLOR_NEON,install_mode?"PKG INSTALL":"VPK CONVERT");
    ui_text(54,166,COLOR_WHITE,title);
    ui_text(54,196,COLOR_DIM,file?file:"");
    psvDebugScreenFillRect(54,252,852,72,0xFF101411u);
    psvDebugScreenRect(54,252,852,72,0xFF24452Fu,1);
    psvDebugScreenFillRect(78,286,804,10,0xFF202820u);
    psvDebugScreenFillRect(78,286,206,10,COLOR_NEON);
    ui_text(78,266,COLOR_WHITE,install_mode?"Preparing package installation...":"Reading and converting VPK...");
    ui_text(78,311,COLOR_DIM,"Please wait. Do not close the application.");
    psvDebugScreenPresent();
}
static void draw_ui(BrowserList *b, const Settings *s){
    ui_background();

    if(g_screen==SCREEN_HOME){
        const char *titles[5]={"VPK CONVERT","PKG INSTALL","SETTINGS","ABOUT","EXIT"};
        const char *subs[5]={"Convert VPK to MRW-PKG","Install MRW-PKG files","App settings","About MrWrack","Close app"};
        int xs[5]={36,318,36,318,36};
        int ys[5]={116,116,226,226,336};
        for(int i=0;i<5;i++) ui_card(xs[i],ys[i],258,88,titles[i],subs[i],i==g_home_selected);

        psvDebugScreenFillRect(620,116,304,308,0xFF071009u);
        psvDebugScreenRect(620,116,304,308,0xFF24452Fu,1);
        ui_text(658,148,COLOR_NEON,"MRWRACK");
        ui_text(658,176,COLOR_WHITE,"PKG CONVERTER");
        ui_text(658,210,COLOR_DIM,"Homebrew  Games  Tools");
        ui_text(658,270,COLOR_WHITE,"PLAY");
        ui_text(700,294,COLOR_WHITE,"MOD");
        ui_text(742,318,COLOR_WHITE,"EXPLORE");
        ui_text(784,342,COLOR_WHITE,"CREATE");
        ui_text(658,388,COLOR_NEON,"BUILT BY PLAYERS");
        ui_text(690,408,COLOR_NEON,"FOR PLAYERS");
        ui_footer();
    }
    else if(g_screen==SCREEN_VPK || g_screen==SCREEN_PKG){
        ItemType type=(g_screen==SCREEN_VPK)?ITEM_VPK:ITEM_PKG;
        int count=filtered_count(b,type);

        ui_text(40,116,COLOR_NEON,type==ITEM_VPK?"VPK CONVERT":"PKG INSTALL");
        ui_text(40,140,COLOR_DIM,type==ITEM_VPK?"Select a VPK to convert":"Select a PKG to install");

        psvDebugScreenFillRect(36,176,888,264,0xFF101411u);
        psvDebugScreenRect(36,176,888,264,0xFF24452Fu,1);

        if(count<=0){
            ui_text(62,208,COLOR_WHITE,type==ITEM_VPK?"No VPK files found.":"No PKG files found.");
        }else{
            int first=g_file_selected-6;if(first<0)first=0;
            int last=first+12;if(last>count)last=count;
            int row=0;
            for(int n=first;n<last;n++,row++){
                int idx=filtered_index(b,type,n);
                if(idx<0)continue;
                BrowserItem *e=&b->items[idx];
                int y=190+row*18;
                if(n==g_file_selected) psvDebugScreenFillRect(48,y-2,852,17,0xFF0B2212u);
                ui_text(58,y,n==g_file_selected?COLOR_NEON:COLOR_WHITE,e->name);
            }
        }
        ui_footer();
    }
    else if(g_screen==SCREEN_SETTINGS){
        ui_text(40,118,COLOR_NEON,"SETTINGS");
        ui_card(60,190,820,90,"SMOOTH SCROLLING",
                s->smooth_scroll?"Enabled":"Disabled",
                g_settings_selected==0);
        ui_footer();
    }
    else if(g_screen==SCREEN_ABOUT){
        ui_text(40,118,COLOR_NEON,"ABOUT");
        ui_text(40,160,COLOR_WHITE,"MrWrack PKG Converter v3.4");
        ui_text(40,190,COLOR_DIM,"GitHub  MrWrack");
        ui_footer();
    }

    if(g_progress.message[0] && g_screen!=SCREEN_HOME)
        ui_text(40,466,g_last_result<0?COLOR_RED:COLOR_NEON,g_progress.message);

    psvDebugScreenPresent();
}

static void refresh_files(BrowserList *browser){
    scan_packages(browser);
    g_last_result=0;
    strcpy(g_progress.stage,"Ready");
    strcpy(g_progress.message,"File list refreshed");
}

int main(void){
    ensure_dirs();
    sceIoRemove("ux0:/MrWrack-startup.log");
    startup_log("1: main entered");
    startup_log("1b: root logger working");

    int screen_r=psvDebugScreenInit();
    if(screen_r<0){
        startup_log("2: framebuffer init FAILED");
        sceKernelDelayThread(3000000);
        sceKernelExitProcess(screen_r);
    }
    startup_log("2: framebuffer init OK");

    memset(&g_progress,0,sizeof(g_progress));
    strcpy(g_progress.stage,"Starting");
    strcpy(g_progress.message,"Loading MrWrack PKG Converter");

    memset(&g_browser,0,sizeof(g_browser));
    memset(&g_settings,0,sizeof(g_settings));

    /*
     * Draw the Home screen BEFORE loading settings or scanning storage.
     * This keeps startup independent of filesystem contents.
     */
    draw_ui(&g_browser,&g_settings);
    startup_log("3: first UI frame shown");

    settings_load(&g_settings);
    startup_log("4: settings loaded");

    strcpy(g_progress.stage,"Ready");
    strcpy(g_progress.message,"Choose VPK Files or PKG Files");

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    /*
     * Flush any stale controller state inherited around app launch.
     * This prevents a held/stale button from triggering Exit immediately.
     */
    SceCtrlData startup_pad;
    memset(&startup_pad,0,sizeof(startup_pad));
    for(int i=0;i<30;i++){
        sceCtrlPeekBufferPositive(0,&startup_pad,1);
        sceKernelDelayThread(16666);
    }

    unsigned last=startup_pad.buttons;
    int redraw=1;
    int running=1;
    unsigned heartbeat=0;
    startup_log("7: entering main loop");

    while(running){
        heartbeat++;
        if((heartbeat % 1000)==0) startup_log("HEARTBEAT: main loop alive");
        if(redraw){
            draw_ui(&g_browser,&g_settings);
            redraw=0;
        }

        SceCtrlData pad;
        memset(&pad,0,sizeof(pad));
        sceCtrlPeekBufferPositive(0,&pad,1);

        unsigned pressed=pad.buttons & ~last;
        last=pad.buttons;

        if(g_screen==SCREEN_HOME){
            if(pressed&SCE_CTRL_UP){
                if(g_home_selected>0) g_home_selected--;
                redraw=1;
            }
            if(pressed&SCE_CTRL_DOWN){
                if(g_home_selected<HOME_COUNT-1) g_home_selected++;
                redraw=1;
            }
            if(pressed&SCE_CTRL_CROSS){
                if(g_home_selected==0){
                    startup_log("5: opening VPK menu");
                    scan_packages(&g_browser);
                    startup_log("6: VPK scan complete");
                    g_screen=SCREEN_VPK;
                    g_file_selected=0;
                }
                else if(g_home_selected==1){
                    startup_log("5: opening PKG menu");
                    scan_packages(&g_browser);
                    startup_log("6: PKG scan complete");
                    g_screen=SCREEN_PKG;
                    g_file_selected=0;
                }
                else if(g_home_selected==2){
                    g_screen=SCREEN_SEARCH_MENU;
                    g_search_category_selected=0;
                }
                else if(g_home_selected==3){ g_screen=SCREEN_SETTINGS; }
                else if(g_home_selected==4){ g_screen=SCREEN_ABOUT; }
                else running=0;
                redraw=1;
            }
            if(pressed&SCE_CTRL_SQUARE){
                refresh_files(&g_browser);
                redraw=1;
            }
        }
        else if(g_screen==SCREEN_VPK || g_screen==SCREEN_PKG){
            ItemType type=g_screen==SCREEN_VPK ? ITEM_VPK : ITEM_PKG;
            int count=filtered_count(&g_browser,type);

            if(pressed&SCE_CTRL_UP){
                if(g_file_selected>0) g_file_selected--;
                redraw=1;
            }
            if(pressed&SCE_CTRL_DOWN){
                if(g_file_selected<count-1) g_file_selected++;
                redraw=1;
            }
            if(pressed&SCE_CTRL_SQUARE){
                refresh_files(&g_browser);
                clamp_file_selection(&g_browser,type);
                redraw=1;
            }
            if(pressed&SCE_CTRL_CIRCLE){
                g_screen=SCREEN_HOME;
                g_last_result=0;
                strcpy(g_progress.stage,"Ready");
                strcpy(g_progress.message,"Back to Home");
                redraw=1;
            }
            if((pressed&SCE_CTRL_TRIANGLE) && count>0){
                g_delete_index=filtered_index(&g_browser,type,g_file_selected);
                g_delete_return=g_screen;
                g_screen=SCREEN_DELETE_CONFIRM;
                redraw=1;
            }
            if((pressed&SCE_CTRL_CROSS) && count>0){
                BrowserItem *it=selected_filtered(&g_browser,type);
                if(it){
                    if(type==ITEM_VPK){
                        draw_loading_screen("Converting VPK...",it->name,0);
                        char created[MRW_MAX_PATH];
                        memset(created,0,sizeof(created));
                        g_last_result=mrw_convert_vpk_to_pkg(
                            it->path,created,sizeof(created),&g_progress
                        );
                        if(g_last_result==0){
                            scan_packages(&g_browser);
                            snprintf(g_progress.message,sizeof(g_progress.message),
                                "Created: %s",created);
                        }
                    } else {
                        draw_loading_screen("Installing package...",it->name,1);
                        g_last_result=mrw_install_homebrew_pkg(
                            it->path,&g_progress
                        );
                        if(g_last_result==0)
                            scan_packages(&g_browser);
                    }
                }
                redraw=1;
            }
        }
        else if(g_screen==SCREEN_SEARCH_MENU){
            if(pressed&SCE_CTRL_UP){
                if(g_search_category_selected>0) g_search_category_selected--;
                redraw=1;
            }
            if(pressed&SCE_CTRL_DOWN){
                if(g_search_category_selected<1) g_search_category_selected++;
                redraw=1;
            }
            if(pressed&SCE_CTRL_CIRCLE){
                g_screen=SCREEN_HOME;
                redraw=1;
            }
            if(pressed&SCE_CTRL_CROSS){
                g_file_selected=0;
                if(g_search_category_selected==0){
                    scan_homebrew(&g_browser);
                    g_screen=SCREEN_HOMEBREW;
                } else {
                    scan_games(&g_browser);
                    g_screen=SCREEN_GAMES;
                }
                redraw=1;
            }
        }
        else if(g_screen==SCREEN_HOMEBREW || g_screen==SCREEN_GAMES){
            int count=g_browser.count;
            if(pressed&SCE_CTRL_UP){
                if(g_file_selected>0) g_file_selected--;
                redraw=1;
            }
            if(pressed&SCE_CTRL_DOWN){
                if(g_file_selected<count-1) g_file_selected++;
                redraw=1;
            }
            if(pressed&SCE_CTRL_SQUARE){
                if(g_screen==SCREEN_HOMEBREW) scan_homebrew(&g_browser);
                else scan_games(&g_browser);
                if(g_file_selected>=g_browser.count) g_file_selected=g_browser.count-1;
                if(g_file_selected<0) g_file_selected=0;
                redraw=1;
            }
            if(pressed&SCE_CTRL_CIRCLE){
                g_screen=SCREEN_SEARCH_MENU;
                redraw=1;
            }
        }
        else if(g_screen==SCREEN_DELETE_CONFIRM){
            if(pressed&SCE_CTRL_CIRCLE){
                g_screen=g_delete_return;
                g_delete_index=-1;
                strcpy(g_progress.stage,"Ready");
                strcpy(g_progress.message,"Delete cancelled");
                redraw=1;
            }
            if(pressed&SCE_CTRL_CROSS){
                if(g_delete_index>=0 && g_delete_index<g_browser.count){
                    BrowserItem *it=&g_browser.items[g_delete_index];
                    int r=sceIoRemove(it->path);
                    g_last_result=r;
                    if(r>=0){
                        strcpy(g_progress.stage,"Done");
                        strcpy(g_progress.message,"File deleted");
                        scan_packages(&g_browser);
                    } else {
                        strcpy(g_progress.stage,"Error");
                        snprintf(g_progress.message,sizeof(g_progress.message),
                            "Delete failed: 0x%08X",(unsigned)r);
                    }
                }
                g_screen=g_delete_return;
                g_delete_index=-1;
                redraw=1;
            }
        }
        else if(g_screen==SCREEN_SETTINGS){
            if(pressed&SCE_CTRL_UP){
                if(g_settings_selected>0) g_settings_selected--;
                redraw=1;
            }
            if(pressed&SCE_CTRL_DOWN){
                if(g_settings_selected<2) g_settings_selected++;
                redraw=1;
            }
            if(pressed&SCE_CTRL_CIRCLE){
                g_screen=SCREEN_HOME;
                settings_save(&g_settings);
                redraw=1;
            }
            if(pressed&SCE_CTRL_CROSS){
                if(g_settings_selected==0){
                    g_settings.smooth_scroll=!g_settings.smooth_scroll;
                    settings_save(&g_settings);
                } else if(g_settings_selected==2){
                    g_screen=SCREEN_HOME;
                    settings_save(&g_settings);
                }
                redraw=1;
            }
        }
        else if(g_screen==SCREEN_ABOUT){
            if(pressed&SCE_CTRL_CIRCLE){
                g_screen=SCREEN_HOME;
                redraw=1;
            }
        }

        if(pressed&SCE_CTRL_START){
            exit_log("INPUT: START pressed (ignored in v3.4)");
        }

        /*
         * No continuous framebuffer redraw.
         * UI is rendered only after a state/input change.
         */
        sceKernelDelayThread(5000);
    }

    settings_save(&g_settings);
    exit_log("8: left main loop");
    psvDebugScreenShutdown();
    exit_log("9: framebuffer shutdown complete");
    sceKernelDelayThread(1000000);
    sceKernelExitProcess(0);
    return 0;
}
