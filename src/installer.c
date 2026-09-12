
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/app.h"

/*
 * MrWrack PKG Converter v1.1 Alpha
 *
 * Homebrew install pipeline based on VitaShell's documented GPL-3.0 installer
 * architecture:
 *   extract -> validate -> make head.bin -> promoter utility -> LiveArea
 *
 * No auto-launch.
 */

#define MRW_PACKAGE_TEMP "ux0:data/MrWrackPKG/package_temp"
#define MRW_HEAD_BIN MRW_PACKAGE_TEMP "/sce_sys/package/head.bin"
#define MRW_HEAD_TEMPLATE "app0:resources/head.bin"

static int exists(const char *p) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    return sceIoGetstat(p, &st) >= 0;
}

static int mkdirs_for_head(void) {
    sceIoMkdir(MRW_PACKAGE_TEMP "/sce_sys", 0777);
    sceIoMkdir(MRW_PACKAGE_TEMP "/sce_sys/package", 0777);
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    SceUID in = sceIoOpen(src, SCE_O_RDONLY, 0);
    if (in < 0) return in;
    SceUID out = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (out < 0) { sceIoClose(in); return out; }

    char buf[64 * 1024];
    int r;
    while ((r = sceIoRead(in, buf, sizeof(buf))) > 0) {
        int w = sceIoWrite(out, buf, r);
        if (w != r) {
            sceIoClose(in); sceIoClose(out);
            return -1;
        }
    }
    sceIoClose(in);
    sceIoClose(out);
    return r < 0 ? r : 0;
}

int mrw_validate_package_temp(void) {
    if (!exists(MRW_PACKAGE_TEMP "/eboot.bin")) return -10;
    if (!exists(MRW_PACKAGE_TEMP "/sce_sys/param.sfo")) return -11;
    return 0;
}

/*
 * Alpha implementation boundary:
 * A valid Vita fake-package head.bin is not just an empty file. VitaShell
 * starts from a known head.bin template and patches package metadata/HMACs.
 *
 * We therefore never create a fake/blank head.bin. If a GPL-compliant template
 * resource is included at app0:resources/head.bin, this copies it into place.
 * Metadata patching/HMAC generation remains the next step.
 */
int mrw_prepare_head_bin(void) {
    if (exists(MRW_HEAD_BIN)) return 0;
    if (!exists(MRW_HEAD_TEMPLATE)) return -20;
    mkdirs_for_head();
    return copy_file(MRW_HEAD_TEMPLATE, MRW_HEAD_BIN);
}

static int load_paf(void) {
    uint32_t argp[] = { 0x180000, (uint32_t)-1, (uint32_t)-1, 1, (uint32_t)-1, (uint32_t)-1 };
    int result = -1;
    uint32_t buf[4];
    buf[0] = sizeof(buf);
    buf[1] = (uint32_t)&result;
    buf[2] = (uint32_t)-1;
    buf[3] = (uint32_t)-1;
    return sceSysmoduleLoadModuleInternalWithArg(
        SCE_SYSMODULE_INTERNAL_PAF, sizeof(argp), argp, buf);
}

static int unload_paf(void) {
    uint32_t buf = 0;
    return sceSysmoduleUnloadModuleInternalWithArg(
        SCE_SYSMODULE_INTERNAL_PAF, 0, NULL, &buf);
}

int mrw_promote_package_temp(void) {
    int r = mrw_validate_package_temp();
    if (r < 0) return r;

    r = mrw_prepare_head_bin();
    if (r < 0) return r;

    r = load_paf();
    if (r < 0) return r;

    r = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (r < 0) {
        unload_paf();
        return r;
    }

    r = scePromoterUtilityInit();
    if (r >= 0)
        r = scePromoterUtilityPromotePkgWithRif(MRW_PACKAGE_TEMP, 1);

    scePromoterUtilityExit();
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    unload_paf();
    return r;
}
