
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../include/app.h"

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
