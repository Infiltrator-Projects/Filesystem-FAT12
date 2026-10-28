// SPDX-License-Identifier: GPL-2.0-or-later
#include "reader.h"

static int is_separator(char character)
{
    return character == '/' || character == '\\';
}

static int bounded_read(void *context, ifs_ext2_u64 offset,
                        void *destination, ifs_ext2_u32 count)
{
    IfsExt2EfiReader *reader = (IfsExt2EfiReader *)context;
    if (reader == IFS_EXT2_NULL ||
        reader->physical_io.read_at == IFS_EXT2_NULL ||
        offset > reader->device_bytes ||
        (ifs_ext2_u64)count > reader->device_bytes - offset)
        return -1;
    return reader->physical_io.read_at(
        reader->physical_io.user, offset, destination, count);
}

IfsExt2Status ifs_ext2_efi_open(
    IfsExt2EfiReader *reader, const IfsExt2Io *io,
    ifs_ext2_u64 device_bytes)
{
    IfsExt2Io readonly;
    IfsExt2Status status;
    ifs_ext2_u32 risks = 0U;

    if (reader == IFS_EXT2_NULL || io == IFS_EXT2_NULL ||
        io->read_at == IFS_EXT2_NULL || device_bytes < 2048U)
        return IFS_EXT2_ERROR_ARGUMENT;

    reader->device_bytes = device_bytes;
    reader->physical_io = *io;
    readonly.read_at = bounded_read;
    readonly.write_at = IFS_EXT2_NULL;
    readonly.flush = IFS_EXT2_NULL;
    readonly.user = reader;
    status = ifs_ext2_open(&reader->volume, &readonly);
    if (status != IFS_EXT2_OK)
        return status;
    if (reader->volume.byte_size > device_bytes)
        return IFS_EXT2_ERROR_RANGE;
    /* Firmware does not attempt journal replay or dirty-volume repair. */
    return ifs_ext2_readonly_assess(&reader->volume, &risks);
}

IfsExt2Status ifs_ext2_efi_open_file(
    const IfsExt2EfiReader *reader, const char *path,
    IfsExt2EfiFile *file, void *scratch, ifs_ext2_u32 scratch_size)
{
    IfsExt2Inode inode;
    IfsExt2Status status;
    const char *cursor;
    unsigned int depth = 0U;

    if (reader == IFS_EXT2_NULL || path == IFS_EXT2_NULL ||
        file == IFS_EXT2_NULL || scratch == IFS_EXT2_NULL ||
        scratch_size < reader->volume.block_size || !is_separator(path[0]))
        return IFS_EXT2_ERROR_ARGUMENT;

    cursor = path + 1;
    if (*cursor == '\0' || is_separator(*cursor))
        return IFS_EXT2_ERROR_ARGUMENT;
    status = ifs_ext2_read_inode(&reader->volume, IFS_EXT2_ROOT_INODE,
                                &inode, scratch, scratch_size);
    if (status != IFS_EXT2_OK)
        return status;

    while (*cursor != '\0') {
        const char *component = cursor;
        unsigned int length = 0U;
        ifs_ext2_u32 number = 0U;

        if (++depth > IFS_EXT2_EFI_MAX_DEPTH)
            return IFS_EXT2_ERROR_RANGE;
        while (*cursor != '\0' && !is_separator(*cursor)) {
            if (++length > IFS_EXT2_MAX_NAME_LENGTH)
                return IFS_EXT2_ERROR_RANGE;
            ++cursor;
        }
        if (length == 0U ||
            (length == 1U && component[0] == '.') ||
            (length == 2U && component[0] == '.' && component[1] == '.'))
            return IFS_EXT2_ERROR_ARGUMENT;
        if (ifs_ext2_inode_type(&inode) != IFS_EXT2_NODE_DIRECTORY)
            return IFS_EXT2_ERROR_NOT_DIRECTORY;

        status = ifs_ext2_lookup(&reader->volume, &inode, component,
                                 (ifs_ext2_u8)length, &number,
                                 scratch, scratch_size);
        if (status != IFS_EXT2_OK)
            return status;
        status = ifs_ext2_read_inode(&reader->volume, number,
                                     &inode, scratch, scratch_size);
        if (status != IFS_EXT2_OK)
            return status;
        /* Symlink resolution needs separate cycle/escape qualification. */
        if (ifs_ext2_inode_type(&inode) == IFS_EXT2_NODE_SYMLINK)
            return IFS_EXT2_ERROR_UNSUPPORTED;
        if (*cursor != '\0') {
            ++cursor;
            if (*cursor == '\0' || is_separator(*cursor))
                return IFS_EXT2_ERROR_ARGUMENT;
        }
    }

    if (ifs_ext2_inode_type(&inode) == IFS_EXT2_NODE_DIRECTORY)
        return IFS_EXT2_ERROR_IS_DIRECTORY;
    if (ifs_ext2_inode_type(&inode) != IFS_EXT2_NODE_REGULAR)
        return IFS_EXT2_ERROR_UNSUPPORTED;
    file->inode = inode;
    file->position = 0U;
    return IFS_EXT2_OK;
}

IfsExt2Status ifs_ext2_efi_read(
    const IfsExt2EfiReader *reader, IfsExt2EfiFile *file,
    void *destination, ifs_ext2_u32 length, void *scratch,
    ifs_ext2_u32 scratch_size, ifs_ext2_u32 *bytes_read)
{
    IfsExt2Status status;
    ifs_ext2_u32 count = 0U;

    if (bytes_read != IFS_EXT2_NULL)
        *bytes_read = 0U;
    if (reader == IFS_EXT2_NULL || file == IFS_EXT2_NULL ||
        bytes_read == IFS_EXT2_NULL || scratch == IFS_EXT2_NULL ||
        scratch_size < reader->volume.block_size ||
        (length != 0U && destination == IFS_EXT2_NULL) ||
        file->position > file->inode.size)
        return IFS_EXT2_ERROR_ARGUMENT;
    if (length == 0U || file->position == file->inode.size)
        return IFS_EXT2_OK;

    status = ifs_ext2_read_file(&reader->volume, &file->inode,
        file->position, destination, length, scratch, scratch_size, &count);
    if (status != IFS_EXT2_OK)
        return status;
    if (count > length ||
        (ifs_ext2_u64)count > file->inode.size - file->position)
        return IFS_EXT2_ERROR_CORRUPT;
    file->position += count;
    *bytes_read = count;
    return IFS_EXT2_OK;
}

IfsExt2Status ifs_ext2_efi_seek(
    IfsExt2EfiFile *file, ifs_ext2_u64 position)
{
    if (file == IFS_EXT2_NULL)
        return IFS_EXT2_ERROR_ARGUMENT;
    if (position > file->inode.size)
        return IFS_EXT2_ERROR_RANGE;
    file->position = position;
    return IFS_EXT2_OK;
}
