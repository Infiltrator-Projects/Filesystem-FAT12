/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Infiltrator Filesystems — experimental freestanding CLASSIC_FAT module ABI.
 * Portable source derived from the format rules used by Defragmenter's
 * fat_volume.c and FAT Test Media, with no Linux VFS or libc dependencies.
 * Version 1 is deliberately read-only: no unsafe write/defrag claim.
 */
#ifndef INFILTRATOR_CLASSIC_FAT_MODULE_H
#define INFILTRATOR_CLASSIC_FAT_MODULE_H

#include <stddef.h>
#include <stdint.h>

#define IFS_CLASSIC_FAT_ABI_VERSION 1u
#define IFS_CLASSIC_FAT_CAP_PROBE UINT64_C(1)
#define IFS_CLASSIC_FAT_CAP_ROOT_83 UINT64_C(2)
#define IFS_CLASSIC_FAT_CAP_FILE_READ UINT64_C(4)
#define IFS_CLASSIC_FAT_CAP_ROOT_FRAGMENT_COUNT UINT64_C(8)

typedef int (*IfsClassicFatReadAt)(void *context, uint64_t offset, void *buffer,
                             uint32_t bytes);
typedef struct {
    IfsClassicFatReadAt read_at; /* Returns zero for a complete read. */
    void *context;
    uint64_t device_bytes; /* Length of volume, NOT size of whole disk. */
} IfsClassicFatIo;

typedef struct {
    uint64_t volume_bytes;
    uint64_t fat_offset;
    uint64_t root_offset;
    uint64_t data_offset;
    uint32_t cluster_bytes;
    uint32_t cluster_count;
    uint32_t total_sectors;
    uint32_t root_entries;
    uint16_t bytes_per_sector;
    uint16_t sectors_per_fat;
    uint8_t sectors_per_cluster;
    uint8_t fat_copies;
} IfsClassicFatVolume;

typedef struct {
    char name[13]; /* Canonical 8.3 ASCII name, NUL terminated. No LFN claim. */
    uint32_t first_cluster;
    uint32_t size_bytes;
    uint8_t attributes;
} IfsClassicFatFile;

typedef struct {
    uint32_t abi_version;
    uint32_t structure_bytes;
    const char *format_id;
    const char *format_name;
    uint64_t capabilities;
    int (*probe)(const IfsClassicFatIo *, IfsClassicFatVolume *);
    /* nth visible 8.3 root entry; 0 = success, 1 = end, -1 = invalid. */
    int (*root_file)(const IfsClassicFatIo *, const IfsClassicFatVolume *,
                     uint32_t nth, IfsClassicFatFile *);
    /* Read file data. 0 = success, -1 = invalid/corrupt/out of range. */
    int (*read_file)(const IfsClassicFatIo *, const IfsClassicFatVolume *,
                     const IfsClassicFatFile *, uint64_t position,
                     void *buffer, uint32_t length);
    /* Check a root file's complete allocation chain, counting fragments. */
    int (*root_file_fragments)(const IfsClassicFatIo *, const IfsClassicFatVolume *,
                               const IfsClassicFatFile *, uint32_t *fragments);
} IfsClassicFatModule;

/* Exported discovery symbol. Not a Linux .ko, EFI image, or full defrag ABI. */
const IfsClassicFatModule *infiltrator_filesystem_module_v1(void);
#endif
