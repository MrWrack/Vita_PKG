
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../include/app.h"
#include "../include/sfo.h"

static int has_ext(const char *name, const char *ext) {
    size_t ln = strlen(name), le = strlen(ext);
    if (ln < le) return 0;
    return strcasecmp(name + ln - le, ext) == 0;
}

static uint64_t item_size(const char *path) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    if (sceIoGetstat(path, &st) < 0) return 0;
    return st.st_size;
}

static void add_file(BrowserList *out, ItemType type, const char *dir, const char *name) {
    if (!out || out->count >= MRW_MAX_ITEMS) return;
    BrowserItem *it = &out->items[out->count++];
    memset(it, 0, sizeof(*it));
    it->type = type;
    snprintf(it->path, sizeof(it->path), "%s/%s", dir, name);
    snprintf(it->name, sizeof(it->name), "%s", name);
    it->size = item_size(it->path);
}

static void scan_dir(BrowserList *out, const char *dir) {
    SceUID d = sceIoDopen(dir);
    if (d < 0) return;
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));

    while (sceIoDread(d, &ent) > 0) {
        if (ent.d_name[0] == '.') {
            memset(&ent, 0, sizeof(ent));
            continue;
        }

        if (has_ext(ent.d_name, ".vpk"))
            add_file(out, ITEM_VPK, dir, ent.d_name);
        else if (has_ext(ent.d_name, ".pkg"))
            add_file(out, ITEM_PKG, dir, ent.d_name);

        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
}

void scan_packages(BrowserList *out) {
    memset(out, 0, sizeof(*out));

    // User-selected paths. Intentionally no ux0:/download/.
    scan_dir(out, "ux0:/downloads");
    scan_dir(out, "ux0:/pkg");
    scan_dir(out, MRW_PKG_ROOT);
}


static int is_official_game_title_id(const char *title_id) {
    /* Retail/digital Vita game IDs normally begin with PCS (PCSA/PCSB/PCSE/etc).
       Everything else is shown under Homebrew/Other. */
    return title_id &&
           title_id[0]=='P' &&
           title_id[1]=='C' &&
           title_id[2]=='S';
}

static void add_installed_app(BrowserList *out, const char *title_id, int want_games) {
    if (!out || !title_id || out->count >= MRW_MAX_ITEMS) return;

    int game = is_official_game_title_id(title_id);
    if (want_games && !game) return;
    if (!want_games && game) return;

    BrowserItem *it = &out->items[out->count++];
    memset(it, 0, sizeof(*it));
    it->type = ITEM_INSTALLED;
    snprintf(it->title_id, sizeof(it->title_id), "%s", title_id);
    snprintf(it->path, sizeof(it->path), "ux0:/app/%s", title_id);

    char sfo[MRW_MAX_PATH];
    snprintf(sfo, sizeof(sfo), "%s/sce_sys/param.sfo", it->path);

    if (mrw_sfo_get_string(sfo, "TITLE", it->name, sizeof(it->name)) < 0 ||
        !it->name[0]) {
        snprintf(it->name, sizeof(it->name), "%s", title_id);
    }

    mrw_sfo_get_string(sfo, "APP_VER", it->version, sizeof(it->version));
}

static void scan_installed_category(BrowserList *out, int want_games, int append_mode) {
    if (!append_mode) memset(out, 0, sizeof(*out));

    SceUID d = sceIoDopen("ux0:/app");
    if (d < 0) return;

    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));

    while (sceIoDread(d, &ent) > 0) {
        if (ent.d_name[0] != '.')
            add_installed_app(out, ent.d_name, want_games);
        memset(&ent, 0, sizeof(ent));
    }

    sceIoDclose(d);
}

void scan_homebrew(BrowserList *out) {
    /* Homebrew category is for INSTALLED homebrew only.
       Loose .vpk/.pkg files belong exclusively to the Convert/Install menus. */
    scan_installed_category(out, 0, 0);
}

void scan_games(BrowserList *out) {
    /* Games category:
       installed Vita game titles (PCS* IDs) from ux0:/app */
    scan_installed_category(out, 1, 0);
}
