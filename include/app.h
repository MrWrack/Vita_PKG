
#pragma once
#include <stdint.h>

#define MRW_TITLE_ID "MRWPKG001"
#define MRW_DATA_ROOT "ux0:/data/MrWrackPKG"
#define MRW_PKG_ROOT  "ux0:/data/MrWrackPKG/pkg"
#define MRW_STAGE_ROOT "ux0:/data/MrWrackPKG/stage"

#define MRW_MAX_ITEMS 256
#define MRW_MAX_PATH  512

typedef enum {
    ITEM_VPK = 0,
    ITEM_PKG = 1,
    ITEM_INSTALLED = 2
} ItemType;

typedef struct {
    ItemType type;
    char path[MRW_MAX_PATH];
    char name[128];
    char title_id[16];
    char version[16];
    uint64_t size;
} BrowserItem;

typedef struct {
    BrowserItem items[MRW_MAX_ITEMS];
    int count;
    int selected;
    float scroll_y;
    float target_scroll_y;
} BrowserList;

typedef struct {
    uint8_t r, g, b;
} AccentColor;

typedef struct {
    AccentColor accent;
    int smooth_scroll;
} Settings;

/* Install progress is declared in mrw_pkg.h. */
