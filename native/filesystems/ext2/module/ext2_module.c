/* SPDX-License-Identifier: GPL-2.0-or-later
 * An actual EXT2.so driver adapter reusing the canonical on-disk engine.
 * Neither EFI nor Linux kernel implementation is imported or impersonated.
 */
#include "ext2_module.h"

static int bounded_read(void *user, ifs_ext2_u64 offset,
                        void *data, ifs_ext2_u32 count)
{
    IfsExt2DropInVolume *volume = (IfsExt2DropInVolume *)user;
    if (!volume || !volume->host.device.read_at ||
        offset > volume->host.volume_bytes ||
        (ifs_ext2_u64)count > volume->host.volume_bytes - offset)
        return -1;
    return volume->host.device.read_at(
        volume->host.device.user, offset, data, count);
}

static int bounded_write(void *user, ifs_ext2_u64 offset,
                         const void *data, ifs_ext2_u32 count)
{
    IfsExt2DropInVolume *volume = (IfsExt2DropInVolume *)user;
    if (!volume || !volume->writable || !volume->host.device.write_at ||
        offset > volume->host.volume_bytes ||
        (ifs_ext2_u64)count > volume->host.volume_bytes - offset)
        return -1;
    return volume->host.device.write_at(
        volume->host.device.user, offset, data, count);
}

static int bounded_flush(void *user)
{
    IfsExt2DropInVolume *volume = (IfsExt2DropInVolume *)user;
    if (!volume || !volume->writable || !volume->host.device.flush)
        return -1;
    return volume->host.device.flush(volume->host.device.user);
}

static IfsExt2Status module_mount(
    IfsExt2DropInVolume *volume, const IfsExt2DropInIo *io, int allow_write)
{
    IfsExt2Io bounded;
    IfsExt2Status status;
    ifs_ext2_u32 risks = 0U;

    if (!volume || !io || !io->device.read_at || io->volume_bytes < 2048U ||
        (allow_write && (!io->device.write_at || !io->device.flush)))
        return IFS_EXT2_ERROR_ARGUMENT;

    volume->mounted = 0U;
    volume->writable = 0U;
    volume->host = *io;
    bounded.read_at = bounded_read;
    bounded.write_at = allow_write ? bounded_write : IFS_EXT2_NULL;
    bounded.flush = allow_write ? bounded_flush : IFS_EXT2_NULL;
    bounded.user = volume;

    /* The write callback stays disabled until assessment passes. */
    status = ifs_ext2_open(&volume->fs, &bounded);
    if (status != IFS_EXT2_OK)
        return status;
    if (volume->fs.byte_size > io->volume_bytes)
        return IFS_EXT2_ERROR_RANGE;
    status = ifs_ext2_readonly_assess(&volume->fs, &risks);
    if (status != IFS_EXT2_OK)
        return status;
    if (allow_write) {
        /* The canonical engine has a separate clean-volume write policy. */
        status = ifs_ext2_write_assess(&volume->fs, &risks);
        if (status != IFS_EXT2_OK)
            return status;
        volume->writable = 1U;
    }
    volume->mounted = 1U;
    return IFS_EXT2_OK;
}

static IfsExt2Status module_root(const IfsExt2DropInVolume *volume,
                                 IfsExt2Inode *root, void *scratch,
                                 ifs_ext2_u32 scratch_bytes)
{
    if (!volume || !volume->mounted)
        return IFS_EXT2_ERROR_ARGUMENT;
    return ifs_ext2_read_inode(&volume->fs, IFS_EXT2_ROOT_INODE,
                               root, scratch, scratch_bytes);
}

static IfsExt2Status module_lookup(
    const IfsExt2DropInVolume *volume, ifs_ext2_u32 directory_inode,
    const char *name, ifs_ext2_u32 name_length,
    IfsExt2Inode *entry, void *scratch, ifs_ext2_u32 scratch_bytes)
{
    IfsExt2Inode directory;
    IfsExt2Status status;
    ifs_ext2_u32 inode_number;
    if (!volume || !volume->mounted || !name || !entry ||
        !name_length || name_length > IFS_EXT2_MAX_NAME_LENGTH)
        return IFS_EXT2_ERROR_ARGUMENT;
    status = ifs_ext2_read_inode(&volume->fs, directory_inode,
                                 &directory, scratch, scratch_bytes);
    if (status != IFS_EXT2_OK)
        return status;
    status = ifs_ext2_lookup(&volume->fs, &directory, name,
                             (ifs_ext2_u8)name_length, &inode_number,
                             scratch, scratch_bytes);
    if (status != IFS_EXT2_OK)
        return status;
    return ifs_ext2_read_inode(&volume->fs, inode_number,
                                entry, scratch, scratch_bytes);
}

static IfsExt2Status module_enumerate(
    const IfsExt2DropInVolume *volume, ifs_ext2_u32 directory_inode,
    IfsExt2DirectoryCallback callback, void *context,
    void *scratch, ifs_ext2_u32 scratch_bytes)
{
    IfsExt2Inode directory;
    IfsExt2Status status;
    if (!volume || !volume->mounted || !callback)
        return IFS_EXT2_ERROR_ARGUMENT;
    status = ifs_ext2_read_inode(&volume->fs, directory_inode,
                                 &directory, scratch, scratch_bytes);
    if (status != IFS_EXT2_OK)
        return status;
    return ifs_ext2_iterate_directory(&volume->fs, &directory,
                                      callback, context, scratch, scratch_bytes);
}

static IfsExt2Status module_read(
    const IfsExt2DropInVolume *volume, ifs_ext2_u32 inode_number,
    ifs_ext2_u64 offset, void *data, ifs_ext2_u32 count,
    void *scratch, ifs_ext2_u32 scratch_bytes, ifs_ext2_u32 *read_count)
{
    IfsExt2Inode inode;
    IfsExt2Status status;
    if (!volume || !volume->mounted || !read_count)
        return IFS_EXT2_ERROR_ARGUMENT;
    *read_count = 0U;
    status = ifs_ext2_read_inode(&volume->fs, inode_number,
                                 &inode, scratch, scratch_bytes);
    if (status != IFS_EXT2_OK)
        return status;
    if (ifs_ext2_inode_type(&inode) != IFS_EXT2_NODE_REGULAR)
        return IFS_EXT2_ERROR_UNSUPPORTED;
    return ifs_ext2_read_file(&volume->fs, &inode, offset, data, count,
                              scratch, scratch_bytes, read_count);
}

static IfsExt2Status module_write_existing(
    const IfsExt2DropInVolume *volume, ifs_ext2_u32 inode_number,
    ifs_ext2_u64 offset, const void *data, ifs_ext2_u32 count,
    void *scratch, ifs_ext2_u32 scratch_bytes, ifs_ext2_u32 *written)
{
    IfsExt2Inode inode;
    IfsExt2Status status;
    if (!volume || !volume->mounted || !volume->writable || !written)
        return IFS_EXT2_ERROR_UNSUPPORTED;
    *written = 0U;
    /* Re-read inode metadata; never trust a host-supplied block map. */
    status = ifs_ext2_read_inode(&volume->fs, inode_number,
                                 &inode, scratch, scratch_bytes);
    if (status != IFS_EXT2_OK)
        return status;
    status = ifs_ext2_write_file_existing(
        &volume->fs, &inode, offset, data, count,
        scratch, scratch_bytes, written);
    if (status != IFS_EXT2_OK)
        return status;
    if (*written != 0U && bounded_flush((void *)volume) != 0)
        return IFS_EXT2_ERROR_IO;
    return IFS_EXT2_OK;
}

__attribute__((used, section(".ifsmeta")))
static const char module_identity[] = "IFS-MODULE:ext2:1";

static const IfsExt2DropInModule module = {
    IFS_EXT2_DROPIN_ABI_VERSION,
    sizeof(IfsExt2DropInModule),
    "ext2", "Second Extended File System",
    IFS_EXT2_CAP_MOUNT | IFS_EXT2_CAP_LOOKUP |
    IFS_EXT2_CAP_ENUMERATE | IFS_EXT2_CAP_FILE_READ |
    IFS_EXT2_CAP_WRITE_EXISTING,
    module_mount, module_root, module_lookup, module_enumerate,
    module_read, module_write_existing
};

__attribute__((visibility("default")))
const IfsExt2DropInModule *infiltrator_filesystem_module_v1(void)
{
    return &module;
}
