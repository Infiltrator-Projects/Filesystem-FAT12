// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INFILTRATR_EXT2_EFI_READER_H
#define INFILTRATR_EXT2_EFI_READER_H

/*
 * Firmware-facing read-only EXT2 file access using the canonical EXT2 engine.
 * This is not a loadable UEFI driver: protocol binding remains separate.
 */
#include "../core/ext2_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IFS_EXT2_EFI_MAX_DEPTH 32U

typedef struct IfsExt2EfiReader {
    IfsExt2Volume volume;
    IfsExt2Io physical_io;
    ifs_ext2_u64 device_bytes;
} IfsExt2EfiReader;

typedef struct IfsExt2EfiFile {
    IfsExt2Inode inode;
    ifs_ext2_u64 position;
} IfsExt2EfiFile;

IfsExt2Status ifs_ext2_efi_open(
    IfsExt2EfiReader *reader, const IfsExt2Io *io,
    ifs_ext2_u64 device_bytes);

IfsExt2Status ifs_ext2_efi_open_file(
    const IfsExt2EfiReader *reader, const char *absolute_path,
    IfsExt2EfiFile *file, void *scratch, ifs_ext2_u32 scratch_size);

IfsExt2Status ifs_ext2_efi_read(
    const IfsExt2EfiReader *reader, IfsExt2EfiFile *file,
    void *destination, ifs_ext2_u32 length, void *scratch,
    ifs_ext2_u32 scratch_size, ifs_ext2_u32 *bytes_read);

IfsExt2Status ifs_ext2_efi_seek(
    IfsExt2EfiFile *file, ifs_ext2_u64 position);

#ifdef __cplusplus
}
#endif
#endif
