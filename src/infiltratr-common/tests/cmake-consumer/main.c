// SPDX-License-Identifier: GPL-3.0-or-later
#include "infiltratr/core.h"

#include <stdio.h>
#include <string.h>

#ifndef INFILTRATR_COMMON_EXPECTED_VERSION
#error "INFILTRATR_COMMON_EXPECTED_VERSION must be supplied by the package consumer build"
#endif

int main(void)
{
    if (strcmp(INFILTRATR_COMMON_VERSION,
               INFILTRATR_COMMON_EXPECTED_VERSION) != 0) {
        fprintf(stderr,
                "Libraries package/header version mismatch: package=%s header=%s\n",
                INFILTRATR_COMMON_EXPECTED_VERSION,
                INFILTRATR_COMMON_VERSION);
        return 1;
    }

    puts("Libraries package consumer passed");
    return 0;
}
