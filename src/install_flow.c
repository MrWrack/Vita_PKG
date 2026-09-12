
#include "../include/install_flow.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>

#define PACKAGE_TEMP "ux0:data/MrWrackPKG/package_temp"

int mrw_promote_package_temp(void);

static int remove_tree(const char *path) {
    SceUID d = sceIoDopen(path);
    if (d < 0) {
        sceIoRemove(path);
        return 0;
    }

    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));

    while (sceIoDread(d, &ent) > 0) {
        if (!strcmp(ent.d_name, ".") || !strcmp(ent.d_name, "..")) {
            memset(&ent, 0, sizeof(ent));
            continue;
        }

        char child[512];
        snprintf(child, sizeof(child), "%s/%s", path, ent.d_name);

        SceIoStat st;
        memset(&st, 0, sizeof(st));
        if (sceIoGetstat(child, &st) >= 0) {
            if (SCE_S_ISDIR(st.st_mode))
                remove_tree(child);
            else
                sceIoRemove(child);
        }

        memset(&ent, 0, sizeof(ent));
    }

    sceIoDclose(d);
    sceIoRmdir(path);
    return 0;
}

static int prepare_clean_temp(void) {
    remove_tree(PACKAGE_TEMP);
    return sceIoMkdir(PACKAGE_TEMP, 0777) < 0 ? 0 : 0;
}

int mrw_install_homebrew_pkg(const char *pkg_path, MrwInstallProgress *p) {
    if (p) {
        p->percent = 1;
        snprintf(p->stage, sizeof(p->stage), "Prepare");
        snprintf(p->message, sizeof(p->message), "Cleaning install temp");
    }

    prepare_clean_temp();

    int r = mrw_pkg_extract_v2(pkg_path, PACKAGE_TEMP, p);
    if (r < 0) {
        if (p) {
            snprintf(p->stage, sizeof(p->stage), "Error");
            snprintf(p->message, sizeof(p->message),
                     "Extract/verify failed: %d", r);
        }
        remove_tree(PACKAGE_TEMP);
        return r;
    }

    if (p) {
        p->percent = 82;
        snprintf(p->stage, sizeof(p->stage), "Validate");
        snprintf(p->message, sizeof(p->message), "Validating Vita app");
    }

    r = mrw_promote_package_temp();
    if (r < 0) {
        if (p) {
            snprintf(p->stage, sizeof(p->stage), "Error");
            snprintf(p->message, sizeof(p->message),
                     "Promotion failed: 0x%08X", (unsigned)r);
        }
        remove_tree(PACKAGE_TEMP);
        return r;
    }

    remove_tree(PACKAGE_TEMP);

    if (p) {
        p->percent = 100;
        snprintf(p->stage, sizeof(p->stage), "Done");
        snprintf(p->message, sizeof(p->message),
                 "Installed successfully - launch manually");
    }
    return 0;
}
