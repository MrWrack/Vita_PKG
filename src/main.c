
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
    "VPK CONVERT",
    "PKG INSTALL",
    "GAMES",
    "HOMEBREW",
    "SEARCH",
    "SETTINGS"
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
    printf("MRWRACK PKG CONVERTER  v3.8\n");
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



/* Forward declarations for branded UI helpers.
   Required because loading/header drawing calls these before their definitions. */
static void ui_crown(int cx,int top,uint32_t color);
static void ui_github_octocat_crowned(int cx,int cy);
static void ui_mrwrack_logo(int x,int y);
static void ui_vita_silhouette(int x,int y,int w,int h);
static void ui_status_panel(int x,int y,int w,int h);
static void ui_draw_menu_icon_v36(int kind,int cx,int cy,int selected);
static void ui_card_v36(int x,int y,int w,int h,const char *title,const char *sub,int selected,int icon_kind);

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

        ui_text(102,36,COLOR_NEON,"MRWRACK");
    ui_text(102,60,COLOR_DIM,"PS VITA TOOLS & MORE");
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
    ui_text(700,505,COLOR_DIM,"MrWrack PKG Converter v3.8");
}
static void draw_loading_screen(const char *title,const char *file,int install_mode){
    psvDebugScreenClear(0xFF050705u);

    ui_github_octocat_crowned(54,54);
    ui_mrwrack_logo(96,34);
    psvDebugScreenFillRect(24,96,912,2,COLOR_NEON);

    ui_text(54,132,COLOR_NEON,install_mode?"PKG INSTALL":"VPK CONVERT");
    ui_text(54,166,COLOR_WHITE,title);
    ui_text(54,194,COLOR_DIM,file?file:"");

    psvDebugScreenFillRect(54,242,852,106,0xFF0C100Du);
    psvDebugScreenRect(54,242,852,106,0xFF24452Fu,1);

    ui_text(78,262,COLOR_WHITE,
            install_mode?"Preparing installation...":"Preparing conversion...");

    psvDebugScreenFillRect(78,298,804,10,0xFF202820u);
    psvDebugScreenFillRect(78,298,240,10,COLOR_NEON);

    ui_text(78,322,COLOR_DIM,"Please wait. Do not close the application.");

    psvDebugScreenFillRect(54,388,852,56,0xFF071009u);
    psvDebugScreenRect(54,388,852,56,0xFF24452Fu,1);
    ui_text(78,404,COLOR_NEON,"MRWRACK");
    ui_crown(114,382,COLOR_NEON);
    ui_text(214,404,COLOR_DIM,
            install_mode?"VERIFY  PREPARE  PROMOTE":"READ  HASH  WRITE MRW-PKG");

    psvDebugScreenPresent();
}

static void ui_draw_menu_icon(int kind,int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    uint32_t d=selected?0xFF0B2212u:0xFF0D120Eu;

    psvDebugScreenCircle(cx,cy,26,0xFF24452Fu);
    psvDebugScreenFillRect(cx-22,cy-22,44,44,d);

    if(kind==0){
        /* VPK Convert: document + arrows */
        psvDebugScreenRect(cx-10,cy-14,20,28,c,2);
        psvDebugScreenLine(cx+2,cy-14,cx+10,cy-6,c);
        psvDebugScreenLine(cx-3,cy-1,cx+7,cy-1,c);
        psvDebugScreenLine(cx+7,cy-1,cx+3,cy-5,c);
        psvDebugScreenLine(cx+7,cy-1,cx+3,cy+3,c);
        psvDebugScreenLine(cx+3,cy+8,cx-7,cy+8,c);
        psvDebugScreenLine(cx-7,cy+8,cx-3,cy+4,c);
        psvDebugScreenLine(cx-7,cy+8,cx-3,cy+12,c);
    } else if(kind==1){
        /* PKG Install: package + down arrow */
        psvDebugScreenRect(cx-12,cy-12,24,22,c,2);
        psvDebugScreenLine(cx-12,cy-12,cx,cy-20,c);
        psvDebugScreenLine(cx,cy-20,cx+12,cy-12,c);
        psvDebugScreenLine(cx,cy-20,cx,cy+10,c);
        psvDebugScreenLine(cx+18,cy+2,cx+18,cy+16,c);
        psvDebugScreenLine(cx+12,cy+10,cx+18,cy+16,c);
        psvDebugScreenLine(cx+24,cy+10,cx+18,cy+16,c);
    } else if(kind==2){
        /* Games: controller */
        psvDebugScreenCircle(cx-9,cy,10,c);
        psvDebugScreenCircle(cx+9,cy,10,c);
        psvDebugScreenFillRect(cx-10,cy-6,20,12,d);
        psvDebugScreenLine(cx-14,cy,cx-4,cy,c);
        psvDebugScreenLine(cx-9,cy-5,cx-9,cy+5,c);
        psvDebugScreenCircle(cx+8,cy-3,2,c);
        psvDebugScreenCircle(cx+14,cy+3,2,c);
    } else if(kind==3){
        /* Homebrew: house */
        psvDebugScreenLine(cx-16,cy-2,cx,cy-18,c);
        psvDebugScreenLine(cx,cy-18,cx+16,cy-2,c);
        psvDebugScreenRect(cx-11,cy-2,22,19,c,2);
        psvDebugScreenRect(cx-3,cy+7,6,10,c,1);
    } else if(kind==4){
        /* Search: magnifier */
        psvDebugScreenCircle(cx-5,cy-5,12,c);
        psvDebugScreenLine(cx+4,cy+4,cx+17,cy+17,c);
        psvDebugScreenLine(cx+5,cy+3,cx+18,cy+16,c);
    } else if(kind==5){
        /* Settings: simple gear */
        psvDebugScreenCircle(cx,cy,11,c);
        psvDebugScreenCircle(cx,cy,4,c);
        psvDebugScreenFillRect(cx-2,cy-19,4,7,c);
        psvDebugScreenFillRect(cx-2,cy+12,4,7,c);
        psvDebugScreenFillRect(cx-19,cy-2,7,4,c);
        psvDebugScreenFillRect(cx+12,cy-2,7,4,c);
    }
}

static void ui_card_icon(int x,int y,int w,int h,const char *title,const char *sub,int selected,int icon_kind){
    uint32_t border=selected?COLOR_NEON:0xFF24452Fu;
    uint32_t fill=selected?0xFF0B2212u:0xFF101411u;

    psvDebugScreenFillRect(x,y,w,h,fill);
    psvDebugScreenRect(x,y,w,h,border,selected?3:1);
    if(selected) psvDebugScreenFillRect(x,y,5,h,COLOR_NEON);

    ui_text(x+22,y+18,selected?COLOR_NEON:COLOR_WHITE,title);
    ui_text(x+22,y+44,COLOR_DIM,sub);

    /* Requested icon on the RIGHT side of every clickable card. */
    ui_draw_menu_icon(icon_kind,x+w-48,y+h/2,selected);

    /* Small chevron beside the icon. */
    ui_text(x+w-18,y+31,selected?COLOR_NEON:COLOR_DIM,">");
}

static void ui_crown(int cx,int top,uint32_t color){
    /* Compact 5-point crown, sized for Vita's 960x544 framebuffer. */
    psvDebugScreenLine(cx-16,top+13,cx-12,top+2,color);
    psvDebugScreenLine(cx-12,top+2,cx-5,top+9,color);
    psvDebugScreenLine(cx-5,top+9,cx,top-2,color);
    psvDebugScreenLine(cx,top-2,cx+5,top+9,color);
    psvDebugScreenLine(cx+5,top+9,cx+12,top+2,color);
    psvDebugScreenLine(cx+12,top+2,cx+16,top+13,color);
    psvDebugScreenLine(cx-16,top+13,cx+16,top+13,color);
    psvDebugScreenLine(cx-13,top+18,cx+13,top+18,color);
    psvDebugScreenCircle(cx-12,top+1,1,color);
    psvDebugScreenCircle(cx,top-3,1,color);
    psvDebugScreenCircle(cx+12,top+1,1,color);
}

static void ui_github_octocat_crowned(int cx,int cy){
    uint32_t c=COLOR_NEON;

    /* Outer badge */
    psvDebugScreenCircle(cx,cy,24,c);
    psvDebugScreenCircle(cx,cy,23,c);

    /* Octocat-like silhouette */
    psvDebugScreenFillRect(cx-11,cy-8,22,18,c);
    psvDebugScreenLine(cx-11,cy-8,cx-17,cy-16,c);
    psvDebugScreenLine(cx-17,cy-16,cx-16,cy-3,c);
    psvDebugScreenLine(cx+11,cy-8,cx+17,cy-16,c);
    psvDebugScreenLine(cx+17,cy-16,cx+16,cy-3,c);
    psvDebugScreenFillRect(cx-6,cy+9,12,10,c);
    psvDebugScreenLine(cx-6,cy+16,cx-14,cy+19,c);
    psvDebugScreenLine(cx-14,cy+19,cx-20,cy+15,c);

    /* Crown is clearly separated above the badge. */
    ui_crown(cx,cy-43,c);
}

static void ui_mrwrack_logo(int x,int y){
    ui_crown(x+38,y-20,COLOR_NEON);
    ui_text(x,y,COLOR_NEON,"MRWRACK");
    ui_text(x,y+24,COLOR_DIM,"PS VITA TOOLS & MORE");
}

static void ui_vita_silhouette(int x,int y,int w,int h){
    /* Clean right-side Vita silhouette instead of the old distorted line background. */
    uint32_t edge=0xFF1C7A38u;
    uint32_t glow=0xFF0B3518u;

    psvDebugScreenFillRect(x,y,w,h,0xFF071009u);
    psvDebugScreenRect(x,y,w,h,0xFF24452Fu,1);

    int bx=x+22, by=y+42, bw=w-44, bh=h-118;
    psvDebugScreenRect(bx,by,bw,bh,edge,2);

    /* screen */
    psvDebugScreenRect(bx+34,by+22,bw-68,bh-44,glow,1);
    ui_text(bx+68,by+70,COLOR_DIM,"PS VITA");

    /* left/right controls */
    psvDebugScreenCircle(bx+16,by+bh/2,10,edge);
    psvDebugScreenCircle(bx+bw-16,by+bh/2,10,edge);
    psvDebugScreenCircle(bx+bw-18,by+22,3,edge);
    psvDebugScreenCircle(bx+bw-9,by+31,3,edge);
    psvDebugScreenCircle(bx+bw-27,by+31,3,edge);
    psvDebugScreenCircle(bx+bw-18,by+40,3,edge);

    ui_text(x+32,y+h-58,COLOR_NEON,"MRWRACK");
    ui_crown(x+w/2,y+h-86,COLOR_NEON);
    ui_text(x+62,y+h-32,COLOR_DIM,"TOOLS & MORE");
}

static void ui_status_panel(int x,int y,int w,int h){
    /* Purpose-built right panel: no fake console outline, no distorted geometry. */
    psvDebugScreenFillRect(x,y,w,h,0xFF071009u);
    psvDebugScreenRect(x,y,w,h,0xFF24452Fu,1);

    ui_text(x+22,y+18,COLOR_NEON,"MRWRACK");
    ui_crown(x+62,y+3,COLOR_NEON);
    ui_text(x+22,y+48,COLOR_WHITE,"PKG CONVERTER");
    ui_text(x+22,y+72,COLOR_DIM,"HOME  /  STATUS");

    psvDebugScreenFillRect(x+20,y+102,w-40,1,0xFF24452Fu);

    ui_text(x+22,y+122,COLOR_DIM,"APP");
    ui_text(x+118,y+122,COLOR_WHITE,"v3.8");

    ui_text(x+22,y+150,COLOR_DIM,"FORMAT");
    ui_text(x+118,y+150,COLOR_WHITE,"MRW-PKG v2");

    ui_text(x+22,y+178,COLOR_DIM,"VPK");
    ui_text(x+118,y+178,COLOR_WHITE,"CONVERT READY");

    ui_text(x+22,y+206,COLOR_DIM,"PKG");
    ui_text(x+118,y+206,COLOR_WHITE,"INSTALL READY");

    psvDebugScreenFillRect(x+20,y+240,w-40,1,0xFF24452Fu);
    ui_text(x+22,y+258,COLOR_NEON,"PLAY  MOD  EXPLORE");
    ui_text(x+58,y+280,COLOR_NEON,"CREATE");
}


static void ui_icon_vpk(int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    psvDebugScreenRect(cx-12,cy-16,22,30,c,2);
    psvDebugScreenLine(cx+2,cy-16,cx+10,cy-8,c);
    psvDebugScreenLine(cx-5,cy-2,cx+5,cy-2,c);
    psvDebugScreenLine(cx+5,cy-2,cx+1,cy-6,c);
    psvDebugScreenLine(cx+5,cy-2,cx+1,cy+2,c);
    psvDebugScreenLine(cx+1,cy+8,cx-9,cy+8,c);
    psvDebugScreenLine(cx-9,cy+8,cx-5,cy+4,c);
    psvDebugScreenLine(cx-9,cy+8,cx-5,cy+12,c);
}
static void ui_icon_pkg(int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    psvDebugScreenRect(cx-13,cy-11,26,22,c,2);
    psvDebugScreenLine(cx-13,cy-11,cx,cy-19,c);
    psvDebugScreenLine(cx,cy-19,cx+13,cy-11,c);
    psvDebugScreenLine(cx,cy-19,cx,cy+11,c);
    psvDebugScreenLine(cx+17,cy+1,cx+17,cy+16,c);
    psvDebugScreenLine(cx+11,cy+10,cx+17,cy+16,c);
    psvDebugScreenLine(cx+23,cy+10,cx+17,cy+16,c);
}
static void ui_icon_game(int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    psvDebugScreenCircle(cx-10,cy,11,c);
    psvDebugScreenCircle(cx+10,cy,11,c);
    psvDebugScreenLine(cx-16,cy,cx-6,cy,c);
    psvDebugScreenLine(cx-11,cy-5,cx-11,cy+5,c);
    psvDebugScreenCircle(cx+8,cy-3,2,c);
    psvDebugScreenCircle(cx+14,cy+3,2,c);
}
static void ui_icon_homebrew(int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    psvDebugScreenLine(cx-17,cy-2,cx,cy-19,c);
    psvDebugScreenLine(cx,cy-19,cx+17,cy-2,c);
    psvDebugScreenRect(cx-12,cy-2,24,20,c,2);
    psvDebugScreenRect(cx-4,cy+7,8,11,c,1);
}
static void ui_icon_search(int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    psvDebugScreenCircle(cx-5,cy-5,13,c);
    psvDebugScreenLine(cx+5,cy+5,cx+18,cy+18,c);
    psvDebugScreenLine(cx+7,cy+3,cx+20,cy+16,c);
}
static void ui_icon_settings(int cx,int cy,int selected){
    uint32_t c=selected?COLOR_NEON:COLOR_WHITE;
    psvDebugScreenCircle(cx,cy,12,c);
    psvDebugScreenCircle(cx,cy,4,c);
    psvDebugScreenFillRect(cx-2,cy-20,4,7,c);
    psvDebugScreenFillRect(cx-2,cy+13,4,7,c);
    psvDebugScreenFillRect(cx-20,cy-2,7,4,c);
    psvDebugScreenFillRect(cx+13,cy-2,7,4,c);
    psvDebugScreenLine(cx-13,cy-13,cx-8,cy-8,c);
    psvDebugScreenLine(cx+13,cy-13,cx+8,cy-8,c);
    psvDebugScreenLine(cx-13,cy+13,cx-8,cy+8,c);
    psvDebugScreenLine(cx+13,cy+13,cx+8,cy+8,c);
}

static void ui_draw_menu_icon_v36(int kind,int cx,int cy,int selected){
    /* Circular icon container. */
    uint32_t ring=selected?COLOR_NEON:0xFF2C6E3Du;
    psvDebugScreenCircle(cx,cy,25,ring);
    psvDebugScreenCircle(cx,cy,24,ring);

    if(kind==0) ui_icon_vpk(cx,cy,selected);
    else if(kind==1) ui_icon_pkg(cx,cy,selected);
    else if(kind==2) ui_icon_game(cx,cy,selected);
    else if(kind==3) ui_icon_homebrew(cx,cy,selected);
    else if(kind==4) ui_icon_search(cx,cy,selected);
    else ui_icon_settings(cx,cy,selected);
}

static void ui_card_v36(int x,int y,int w,int h,const char *title,const char *sub,int selected,int icon_kind){
    uint32_t border=selected?COLOR_NEON:0xFF24452Fu;
    uint32_t fill=selected?0xFF0A1C10u:0xFF0C100Du;

    psvDebugScreenFillRect(x,y,w,h,fill);
    psvDebugScreenRect(x,y,w,h,border,selected?3:1);

    if(selected)
        psvDebugScreenFillRect(x,y,4,h,COLOR_NEON);

    /* More room between text and right-side icon. */
    ui_text(x+18,y+17,selected?COLOR_NEON:COLOR_WHITE,title);
    ui_text(x+18,y+43,COLOR_DIM,sub);

    ui_draw_menu_icon_v36(icon_kind,x+w-40,y+h/2,selected);
}
static void draw_ui(BrowserList *b, const Settings *s){
    psvDebugScreenClear(0xFF050705u);

    /* Balanced header */
    ui_github_octocat_crowned(54,54);
    ui_mrwrack_logo(96,34);
    ui_text(675,30,COLOR_WHITE,"PLAY   MOD   EXPLORE   CREATE");
    ui_text(722,56,COLOR_NEON,"BUILT BY PLAYERS");
    psvDebugScreenFillRect(24,96,912,2,COLOR_NEON);

    if(g_screen==SCREEN_HOME){
        const char *titles[6]={
            "VPK CONVERT","PKG INSTALL","GAMES",
            "HOMEBREW","SEARCH","SETTINGS"
        };
        const char *subs[6]={
            "Convert VPK to MRW-PKG",
            "Install MRW-PKG files",
            "Installed games",
            "Installed homebrew",
            "Games & homebrew",
            "App settings"
        };

        /* Cards are narrower so the right status panel has breathing room. */
        int xs[6]={28,300,28,300,28,300};
        int ys[6]={120,120,226,226,332,332};

        for(int i=0;i<6;i++)
            ui_card_v36(xs[i],ys[i],252,82,titles[i],subs[i],i==g_home_selected,i);

        ui_status_panel(580,120,350,294);

        psvDebugScreenFillRect(28,432,902,48,0xFF0A0E0Bu);
        psvDebugScreenRect(28,432,902,48,0xFF24452Fu,1);
        ui_text(46,444,COLOR_WHITE,"MrWrack PKG Converter");
        ui_text(46,462,COLOR_DIM,"Convert, install and browse PS Vita homebrew content.");
        ui_text(722,444,COLOR_NEON,"READY");

        ui_footer();
    }
    else if(g_screen==SCREEN_VPK || g_screen==SCREEN_PKG){
        ItemType type=(g_screen==SCREEN_VPK)?ITEM_VPK:ITEM_PKG;
        int count=filtered_count(b,type);

        ui_text(36,122,COLOR_NEON,type==ITEM_VPK?"VPK CONVERT":"PKG INSTALL");
        ui_text(36,146,COLOR_DIM,type==ITEM_VPK?"Select a VPK to convert":"Select a PKG to install");

        psvDebugScreenFillRect(30,178,900,266,0xFF0C100Du);
        psvDebugScreenRect(30,178,900,266,0xFF24452Fu,1);

        if(count<=0){
            ui_text(54,210,COLOR_WHITE,type==ITEM_VPK?"No VPK files found.":"No PKG files found.");
        } else {
            int first=g_file_selected-6;
            if(first<0) first=0;
            int last=first+12;
            if(last>count) last=count;

            int row=0;
            for(int n=first;n<last;n++,row++){
                int idx=filtered_index(b,type,n);
                if(idx<0) continue;

                BrowserItem *e=&b->items[idx];
                int y=192+row*19;

                if(n==g_file_selected)
                    psvDebugScreenFillRect(44,y-2,870,18,0xFF0A1C10u);

                ui_text(54,y,n==g_file_selected?COLOR_NEON:COLOR_WHITE,e->name);
            }
        }
        ui_footer();
    }
    else if(g_screen==SCREEN_SEARCH_MENU){
        ui_text(36,122,COLOR_NEON,"SEARCH");
        ui_text(36,146,COLOR_DIM,"Choose category");

        ui_card_v36(70,206,380,100,"HOMEBREW","Installed homebrew",
                    g_search_category_selected==0,3);
        ui_card_v36(510,206,380,100,"GAMES","Installed games",
                    g_search_category_selected==1,2);

        ui_footer();
    }
    else if(g_screen==SCREEN_HOMEBREW || g_screen==SCREEN_GAMES){
        ui_text(36,122,COLOR_NEON,g_screen==SCREEN_HOMEBREW?"HOMEBREW":"GAMES");
        ui_text(36,146,COLOR_DIM,"Installed content");

        psvDebugScreenFillRect(30,178,900,266,0xFF0C100Du);
        psvDebugScreenRect(30,178,900,266,0xFF24452Fu,1);

        int count=b->count;
        if(count<=0){
            ui_text(54,210,COLOR_WHITE,"Nothing found.");
        } else {
            if(g_file_selected>=count) g_file_selected=count-1;
            if(g_file_selected<0) g_file_selected=0;

            int first=g_file_selected-6;
            if(first<0) first=0;
            int last=first+12;
            if(last>count) last=count;

            int row=0;
            for(int i=first;i<last;i++,row++){
                BrowserItem *e=&b->items[i];
                int y=192+row*19;

                if(i==g_file_selected)
                    psvDebugScreenFillRect(44,y-2,870,18,0xFF0A1C10u);

                ui_text(54,y,i==g_file_selected?COLOR_NEON:COLOR_WHITE,e->name);
                if(e->title_id[0])
                    ui_text(768,y,COLOR_DIM,e->title_id);
            }
        }
        ui_footer();
    }
    else if(g_screen==SCREEN_SETTINGS){
        ui_text(36,122,COLOR_NEON,"SETTINGS");
        ui_card_v36(70,196,820,92,"SMOOTH SCROLLING",
                    s->smooth_scroll?"Enabled":"Disabled",
                    g_settings_selected==0,5);
        ui_text(70,326,COLOR_DIM,"MrWrack PKG Converter v3.8");
        ui_footer();
    }
    else if(g_screen==SCREEN_ABOUT){
        ui_text(36,122,COLOR_NEON,"ABOUT");
        ui_text(36,166,COLOR_WHITE,"MRWRACK PKG CONVERTER");
        ui_crown(118,134,COLOR_NEON);
        ui_text(36,196,COLOR_DIM,"PS Vita homebrew package tools.");
        ui_text(36,224,COLOR_WHITE,"Version 3.8");
        ui_footer();
    }

    if(g_progress.message[0] && g_screen!=SCREEN_HOME)
        ui_text(36,466,g_last_result<0?COLOR_RED:COLOR_NEON,g_progress.message);

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
            if(pressed&SCE_CTRL_LEFT){
                if((g_home_selected%2)==1) g_home_selected--;
                redraw=1;
            }
            if(pressed&SCE_CTRL_RIGHT){
                if((g_home_selected%2)==0 && g_home_selected<5) g_home_selected++;
                redraw=1;
            }
            if(pressed&SCE_CTRL_UP){
                if(g_home_selected>=2) g_home_selected-=2;
                redraw=1;
            }
            if(pressed&SCE_CTRL_DOWN){
                if(g_home_selected<=3) g_home_selected+=2;
                redraw=1;
            }

            if(pressed&SCE_CTRL_CROSS){
                if(g_home_selected==0){
                    scan_packages(&g_browser);
                    g_screen=SCREEN_VPK;
                    g_file_selected=0;
                }
                else if(g_home_selected==1){
                    scan_packages(&g_browser);
                    g_screen=SCREEN_PKG;
                    g_file_selected=0;
                }
                else if(g_home_selected==2){
                    scan_games(&g_browser);
                    g_screen=SCREEN_GAMES;
                    g_file_selected=0;
                }
                else if(g_home_selected==3){
                    scan_homebrew(&g_browser);
                    g_screen=SCREEN_HOMEBREW;
                    g_file_selected=0;
                }
                else if(g_home_selected==4){
                    g_screen=SCREEN_SEARCH_MENU;
                    g_search_category_selected=0;
                }
                else if(g_home_selected==5){
                    g_screen=SCREEN_SETTINGS;
                }
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
            exit_log("INPUT: START pressed (ignored in v3.8)");
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
