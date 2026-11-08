/* SPDX-License-Identifier: GPL-3.0-or-later
 * Format-independent discovery contract for the single distributed filesystem
 * module. Pilot implementation: FAT12.so. No format parser belongs here.
 *
 * The descriptor describes interfaces actually implemented by this file.
 * Returning a pointer is not authorisation to mutate media: consumers must
 * independently enforce privileges, identity and exclusive device ownership.
 * Versioned interface pointers remain valid only while the module is loaded.
 */
#ifndef INFILTRATR_FS_UNIFIED_MODULE_H
#define INFILTRATR_FS_UNIFIED_MODULE_H
#include <stdint.h>

#define IFS_UNIFIED_MODULE_ABI_V1 1u
#define IFS_UNIFIED_MODULE_SYMBOL "infiltrator_filesystem_unified_v1"

/* IDs identify existing, versioned interfaces, not filesystem formats. */
#define IFS_UNIFIED_INTERFACE_PROBE_V1   0x00010001u
#define IFS_UNIFIED_INTERFACE_FILES_V3   0x00010003u
#define IFS_UNIFIED_INTERFACE_VOLUME_V4  0x00010004u
#define IFS_UNIFIED_INTERFACE_DEFRAG_V1  0x00020001u
#define IFS_UNIFIED_INTERFACE_BOOT_V1    0x00030001u

/* Explicitly leave CHECK and REPAIR absent until independently qualified. */
#define IFS_UNIFIED_CAP_PROBE       (UINT64_C(1) << 0)
#define IFS_UNIFIED_CAP_FILE_READ   (UINT64_C(1) << 1)
#define IFS_UNIFIED_CAP_FILE_WRITE  (UINT64_C(1) << 2)
#define IFS_UNIFIED_CAP_FORMAT      (UINT64_C(1) << 3)
#define IFS_UNIFIED_CAP_DEFRAG      (UINT64_C(1) << 4)
#define IFS_UNIFIED_CAP_BOOT_READ   (UINT64_C(1) << 5)
#define IFS_UNIFIED_CAP_CHECK       (UINT64_C(1) << 6)
#define IFS_UNIFIED_CAP_REPAIR      (UINT64_C(1) << 7)

/* SysV x86-64 ABI on every host, including Boot's generic ELF loader.
 * No libc, allocation, constructors, imported symbols or per-format host
 * dependencies are introduced by querying this descriptor. */
typedef struct {
    uint32_t abi_version;
    uint32_t structure_bytes;
    char format_id[16];
    char display_name[32];
    uint64_t capabilities;
    /* Returns NULL for unimplemented/unknown interface IDs.
     * Cast to the corresponding ABI type only after validating its own
     * abi_version, structure_bytes, format identity and required callbacks. */
    const void *(*query_interface)(uint32_t interface_id);
} IfsUnifiedModuleV1;

typedef const IfsUnifiedModuleV1 *(*IfsUnifiedEntryPoint)(void);
const IfsUnifiedModuleV1 *infiltrator_filesystem_unified_v1(void);

#endif
