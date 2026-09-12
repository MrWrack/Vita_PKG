#pragma once
#include "mrw_pkg.h"

/*
 * Converts a standard homebrew VPK (ZIP container) into MRW-PKG v2.
 * Output is created in ux0:/data/MrWrackPKG/pkg/.
 */
int mrw_convert_vpk_to_pkg(
    const char *vpk_path,
    char *output_path,
    unsigned output_path_size,
    MrwInstallProgress *progress
);
