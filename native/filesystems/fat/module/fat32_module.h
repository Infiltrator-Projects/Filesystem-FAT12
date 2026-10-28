/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Freestanding, read-only FAT32 drop-in: no libc, kernel or EFI imports. */
#ifndef INFILTRATOR_FAT32_MODULE_H
#define INFILTRATOR_FAT32_MODULE_H
#include <stddef.h>
#include <stdint.h>
#define IFS_FAT32_ABI_VERSION 1u
#define IFS_FAT32_CAP_PROBE UINT64_C(1)
#define IFS_FAT32_CAP_ROOT_83 UINT64_C(2)
#define IFS_FAT32_CAP_FILE_READ UINT64_C(4)
#define IFS_FAT32_CAP_ROOT_FRAGMENT_COUNT UINT64_C(8)
typedef int (*IfsFat32ReadAt)(void *context, uint64_t offset,
                             void *buffer, uint32_t bytes);
typedef struct {
    IfsFat32ReadAt read_at;
    void *context;
    uint64_t device_bytes;
} IfsFat32Io;
typedef struct {
    uint64_t volume_bytes;
    uint64_t fat_offset;
    uint64_t data_offset;
    uint32_t cluster_bytes;
    uint32_t cluster_count;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t total_sectors;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint8_t fat_copies;
} IfsFat32Volume;
typedef struct {
    char name[13];
    uint32_t first_cluster;
    uint32_t size_bytes;
    uint8_t attributes;
} IfsFat32File;
typedef struct {
    uint32_t abi_version;
    uint32_t structure_bytes;
    const char *format_id;
    const char *format_name;
    uint64_t capabilities;
    int (*probe)(const IfsFat32Io *, IfsFat32Volume *);
    int (*root_file)(const IfsFat32Io *, const IfsFat32Volume *,
                     uint32_t nth, IfsFat32File *);
    int (*read_file)(const IfsFat32Io *, const IfsFat32Volume *,
                     const IfsFat32File *, uint64_t position,
                     void *buffer, uint32_t bytes);
    int (*root_file_fragments)(const IfsFat32Io *, const IfsFat32Volume *,
                               const IfsFat32File *, uint32_t *fragments);
} IfsFat32Module;
const IfsFat32Module *infiltrator_filesystem_module_v1(void);
#endif
