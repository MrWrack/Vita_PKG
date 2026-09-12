
#include <psp2/appmgr.h>
#include <stdio.h>
#include <string.h>

int launch_title_manual(const char *title_id) {
    if (!title_id || strlen(title_id) < 5) return -1;
    char uri[64];
    snprintf(uri, sizeof(uri), "psgm:play?titleid=%s", title_id);
    return sceAppMgrLaunchAppByUri(0x20000, uri);
}
