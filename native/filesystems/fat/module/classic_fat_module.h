/* SPDX-License-Identifier: GPL-3.0-or-later
 * Internal FAT12 integrity scanner: independent media validation and safe
 * redundant FAT repair. NOT a competing mount/read/write implementation.
 * Public filesystem operations use the Common filesystem API exclusively.
 */
#ifndef INFILTRATOR_CLASSIC_FAT_MODULE_H
#define INFILTRATOR_CLASSIC_FAT_MODULE_H
#include <stddef.h>
#include <stdint.h>
typedef int (*IfsClassicFatReadAt)(void *,uint64_t,void *,uint32_t);
typedef struct {
    IfsClassicFatReadAt read_at;
    void *context;
    uint64_t device_bytes;
} IfsClassicFatIo;
typedef struct {
    uint64_t volume_bytes,fat_offset,root_offset,data_offset;
    uint32_t cluster_bytes,cluster_count,total_sectors,root_entries;
    uint16_t bytes_per_sector,sectors_per_fat;
    uint8_t sectors_per_cluster,fat_copies;
} IfsClassicFatVolume;
int ifs_classic_fat_probe(const IfsClassicFatIo *,IfsClassicFatVolume *);
int ifs_classic_fat_check(const IfsClassicFatIo *,const IfsClassicFatVolume *,
                          uint32_t *);
typedef int (*IfsClassicFatWriteAt)(void *,uint64_t,const void *,uint32_t);
typedef int (*IfsClassicFatFlush)(void *);
int ifs_classic_fat_repair_mirrors(const IfsClassicFatIo *,
    const IfsClassicFatVolume *,IfsClassicFatWriteAt,IfsClassicFatFlush,uint32_t *);
#endif
