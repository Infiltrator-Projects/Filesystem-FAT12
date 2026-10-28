// SPDX-License-Identifier: GPL-3.0-or-later
#include "fat_defrag_abi.h"
#include "fat_volume.h"

#if IFS_FAT_DEFRAG_BITS != 12 && IFS_FAT_DEFRAG_BITS != 16 && IFS_FAT_DEFRAG_BITS != 32
#error "IFS_FAT_DEFRAG_BITS must select exactly one FAT width"
#endif

/* The legacy worker has process-wide signal, exit-handler and I/O state.
 * Its host must execute it once, in a child process, and keep the DSO loaded. */
const IfsFatDefragHostV1 *ifs_fat_defrag_host;
int ifs_fat_defrag_worker_main(int argc, char **argv);

static int ifs_fat_host_valid(const IfsFatDefragHostV1 *host) {
    if (host == NULL || host->abi_version != IFS_FAT_DEFRAG_ABI_VERSION ||
        host->structure_bytes < sizeof(*host) || host->program_version == NULL ||
        host->out == NULL || host->err == NULL || ifs_fat_defrag_host != NULL)
        return 0;
#define IFS_FAT_REQUIRE_HOST(name) if (host->name == NULL) return 0;
    IFS_FAT_DEFRAG_HOST_FUNCTIONS(IFS_FAT_REQUIRE_HOST)
#undef IFS_FAT_REQUIRE_HOST
    return 1;
}

static int ifs_fat_defrag_probe(const IfsFatDefragHostV1 *host, const char *path) {
    if (!ifs_fat_host_valid(host) || path == NULL) return -1;
    ifs_fat_defrag_host = host;
    FatType actual;
    int matched = fat_geometry_probe_path(path, &actual) &&
                  (unsigned)actual == IFS_FAT_DEFRAG_BITS;
    ifs_fat_defrag_host = NULL;
    return matched;
}

static int ifs_fat_defrag_run(const IfsFatDefragHostV1 *host,
                              int argc, char **argv) {
    if (!ifs_fat_host_valid(host) || argc < 1 || argv == NULL) return -1;
    ifs_fat_defrag_host = host;
    return ifs_fat_defrag_worker_main(argc, argv);
}

static const IfsFatDefragV1 ifs_fat_defrag_api = {
    IFS_FAT_DEFRAG_ABI_VERSION, sizeof(IfsFatDefragV1),
    IFS_FAT_DEFRAG_BITS, ifs_fat_defrag_probe, ifs_fat_defrag_run
};

__attribute__((visibility("default")))
const IfsFatDefragV1 *infiltrator_filesystem_defrag_v1(void) {
    return &ifs_fat_defrag_api;
}
