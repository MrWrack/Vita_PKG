
#pragma once
#include <stddef.h>

typedef struct {
    int percent;
    char stage[48];
    char message[160];
} MrwInstallProgress;

int mrw_pkg_extract_v2(const char *pkg_path, const char *dest_root, MrwInstallProgress *progress);
