/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../native/filesystems/ext2/core/ext2_engine.h"
#include "../native/filesystems/ext2/module/ext2_module.h"

#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE 1024U
#define BLOCK_COUNT 33U

typedef struct MemoryDisk {
    unsigned char bytes[BLOCK_SIZE * BLOCK_COUNT];
    unsigned int writes;
} MemoryDisk;

static void store_le16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffU);
    p[1] = (unsigned char)((v >> 8) & 0xffU);
}

static void store_le32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 0xffU);
    p[1] = (unsigned char)((v >> 8) & 0xffU);
    p[2] = (unsigned char)((v >> 16) & 0xffU);
    p[3] = (unsigned char)((v >> 24) & 0xffU);
}

static int memory_read(
    void *user, ifs_ext2_u64 offset, void *destination, ifs_ext2_u32 count)
{
    MemoryDisk *disk = (MemoryDisk *)user;
    if (offset > sizeof(disk->bytes) ||
        count > sizeof(disk->bytes) - (size_t)offset)
        return -1;
    memcpy(destination, disk->bytes + (size_t)offset, count);
    return 0;
}

static int memory_write(
    void *user, ifs_ext2_u64 offset, const void *source, ifs_ext2_u32 count)
{
    MemoryDisk *disk = (MemoryDisk *)user;
    if (offset > sizeof(disk->bytes) ||
        count > sizeof(disk->bytes) - (size_t)offset)
        return -1;
    memcpy(disk->bytes + (size_t)offset, source, count);
    ++disk->writes;
    return 0;
}

static int fail(const char *message)
{
    fprintf(stderr, "ext2_engine_test: %s\n", message);
    return 1;
}

static void make_inode(
    unsigned char *raw, unsigned int mode, unsigned long size,
    unsigned int first_block)
{
    memset(raw, 0, 128U);
    store_le16(raw + 0x00U, mode);
    store_le16(raw + 0x02U, 1000U);
    store_le32(raw + 0x04U, size);
    store_le16(raw + 0x18U, 1000U);
    store_le16(raw + 0x1AU, 1U);
    store_le32(raw + 0x28U, first_block);
}


static unsigned int flush_count = 0U;
static int memory_flush(void *user)
{
    (void)user;
    ++flush_count;
    return 0;
}
static int count_hello(void *user, ifs_ext2_u32 number,
                       IfsExt2NodeType type, const char *name,
                       ifs_ext2_u8 length)
{
    unsigned int *count = (unsigned int *)user;
    if (number == 3U && type == IFS_EXT2_NODE_REGULAR &&
        length == 5U && memcmp(name, "hello", 5U) == 0)
        ++*count;
    return 0;
}
int main(void)
{
    MemoryDisk disk;
    IfsExt2DropInVolume volume;
    IfsExt2DropInIo io;
    const IfsExt2DropInModule *api = infiltrator_filesystem_module_v1();
    IfsExt2Inode inode;
    unsigned char scratch[BLOCK_SIZE];
    unsigned char data[16] = {0};
    unsigned int read_count = 0U, written = 0U, found = 0U;
    unsigned char *sb, *gd, *inode_table, *dir;
    IfsExt2Status status;
    memset(&disk, 0, sizeof(disk));
    sb = disk.bytes + BLOCK_SIZE;
    gd = disk.bytes + BLOCK_SIZE * 2U;
    inode_table = disk.bytes + BLOCK_SIZE * 5U;
    dir = disk.bytes + BLOCK_SIZE * 6U;

    store_le32(sb + 0x00U, 8U);
    store_le32(sb + 0x04U, BLOCK_COUNT);
    store_le32(sb + 0x0CU, 25U);
    store_le32(sb + 0x10U, 5U);
    store_le32(sb + 0x14U, 1U);
    store_le32(sb + 0x18U, 0U);
    store_le32(sb + 0x1CU, 0U);
    store_le32(sb + 0x20U, 32U);
    store_le32(sb + 0x24U, 32U);
    store_le32(sb + 0x28U, 8U);
    store_le16(sb + 0x38U, IFS_EXT2_SUPER_MAGIC);
    store_le16(sb + 0x3AU, 1U);
    store_le32(sb + 0x4CU, 1U);
    store_le32(sb + 0x54U, 11U);
    store_le16(sb + 0x58U, 128U);
    store_le32(sb + 0x60U, IFS_EXT2_FEATURE_INCOMPAT_FILETYPE);
    memcpy(sb + 0x68U, "0123456789abcdef", 16U);
    memcpy(sb + 0x78U, "engine-test", 11U);

    store_le32(gd + 0x00U, 3U);
    store_le32(gd + 0x04U, 4U);
    store_le32(gd + 0x08U, 5U);

    make_inode(inode_table + 128U, 0x41EDU, BLOCK_SIZE, 6U);
    make_inode(inode_table + 256U, 0x81A4U, 5U, 7U);

    store_le32(dir + 0U, 2U);
    store_le16(dir + 4U, 12U);
    dir[6U] = 1U;
    dir[7U] = 2U;
    dir[8U] = '.';

    store_le32(dir + 12U, 2U);
    store_le16(dir + 16U, 12U);
    dir[18U] = 2U;
    dir[19U] = 2U;
    dir[20U] = '.';
    dir[21U] = '.';

    store_le32(dir + 24U, 3U);
    store_le16(dir + 28U, 1000U);
    dir[30U] = 5U;
    dir[31U] = 1U;
    memcpy(dir + 32U, "hello", 5U);
    memcpy(disk.bytes + BLOCK_SIZE * 7U, "hello", 5U);


    if (!api || api->abi_version != IFS_EXT2_DROPIN_ABI_VERSION ||
        api->structure_bytes != sizeof(*api) ||
        strcmp(api->format_id, "ext2") ||
        (api->capabilities & IFS_EXT2_CAP_WRITE_EXISTING) == 0U)
        return fail("incorrect EXT2.so ABI or capability table");

    memset(&volume, 0, sizeof(volume));
    io.device.read_at = memory_read;
    io.device.write_at = memory_write;
    io.device.flush = memory_flush;
    io.device.user = &disk;
    io.volume_bytes = sizeof(disk.bytes);

    status = api->mount(&volume, &io, 0);
    if (status != IFS_EXT2_OK || volume.writable != 0U)
        return fail("read-only mount failed");
    if (api->root(&volume, &inode, scratch, sizeof(scratch)) != IFS_EXT2_OK ||
        inode.number != IFS_EXT2_ROOT_INODE)
        return fail("root inode operation failed");
    if (api->lookup(&volume, 2U, "hello", 5U,
                    &inode, scratch, sizeof(scratch)) != IFS_EXT2_OK ||
        inode.number != 3U)
        return fail("lookup did not find real file inode");
    if (api->enumerate(&volume, 2U, count_hello, &found,
                       scratch, sizeof(scratch)) != IFS_EXT2_OK ||
        found != 1U)
        return fail("directory iteration failed");
    if (api->read(&volume, 3U, 0U, data, 5U, scratch,
                  sizeof(scratch), &read_count) != IFS_EXT2_OK ||
        read_count != 5U || memcmp(data, "hello", 5U) != 0)
        return fail("file read returned unexpected bytes");
    if (api->write_existing(&volume, 3U, 1U, "A", 1U,
                            scratch, sizeof(scratch), &written) !=
            IFS_EXT2_ERROR_UNSUPPORTED || disk.writes != 0U)
        return fail("read-only mount permitted a write");

    status = api->mount(&volume, &io, 1);
    if (status != IFS_EXT2_OK || volume.writable != 1U)
        return fail("clean EXT2 writable mount rejected");
    if (api->write_existing(&volume, 3U, 1U, "A", 1U,
                            scratch, sizeof(scratch), &written) != IFS_EXT2_OK ||
        written != 1U || disk.writes != 1U || flush_count != 1U)
        return fail("constrained overwrite or flush failed");
    if (api->write_existing(&volume, 3U, 4U, "AB", 2U,
                            scratch, sizeof(scratch), &written) !=
        IFS_EXT2_ERROR_RANGE || disk.writes != 1U)
        return fail("extending a file was not refused");
    if (api->write_existing(&volume, 2U, 0U, "Q", 1U,
                            scratch, sizeof(scratch), &written) !=
        IFS_EXT2_ERROR_UNSUPPORTED || disk.writes != 1U)
        return fail("directory write was not refused");
    memset(data, 0, sizeof(data));
    if (api->read(&volume, 3U, 0U, data, 5U, scratch,
                  sizeof(scratch), &read_count) != IFS_EXT2_OK ||
        memcmp(data, "hAllo", 5U) != 0)
        return fail("written bytes did not read back");

    io.volume_bytes = sizeof(disk.bytes) - 1U;
    if (api->mount(&volume, &io, 1) == IFS_EXT2_OK)
        return fail("partition shorter than superblock geometry accepted");
    io.volume_bytes = sizeof(disk.bytes);
    store_le16(sb + 0x3AU, 0U);
    if (api->mount(&volume, &io, 1) == IFS_EXT2_OK)
        return fail("dirty filesystem admitted for writes");
    store_le16(sb + 0x3AU, 1U);
    io.device.flush = NULL;
    if (api->mount(&volume, &io, 1) == IFS_EXT2_OK)
        return fail("write mount admitted without flush callback");
    return 0;
}
