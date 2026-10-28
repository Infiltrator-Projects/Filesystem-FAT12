/* SPDX-License-Identifier: GPL-2.0-only */
/* Disposable SCO BFS image; independently constructs genuine on-disk records.
 * Neither root nor media block devices are touched.
 */
#include "bfs_module.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SECTOR 512u
#define SECTORS 16u
#define IMAGE_BYTES (SECTOR * SECTORS)
static unsigned char disk[IMAGE_BYTES];
static unsigned writes, flushes;

static void put16(unsigned char *p, uint32_t x)
{
    p[0] = (unsigned char)x;
    p[1] = (unsigned char)(x >> 8);
}
static void put32(unsigned char *p, uint32_t x)
{
    p[0] = (unsigned char)x;
    p[1] = (unsigned char)(x >> 8);
    p[2] = (unsigned char)(x >> 16);
    p[3] = (unsigned char)(x >> 24);
}
static int rd(void *context, uint64_t off, void *target, uint32_t len)
{
    (void)context;
    if (off > IMAGE_BYTES || len > IMAGE_BYTES - off) return -1;
    memcpy(target, disk + (size_t)off, len);
    return 0;
}
static int wr(void *context, uint64_t off, const void *source, uint32_t len)
{
    (void)context;
    if (off > IMAGE_BYTES || len > IMAGE_BYTES - off) return -1;
    memcpy(disk + (size_t)off, source, len);
    ++writes;
    return 0;
}
static int flush(void *context)
{
    (void)context;
    ++flushes;
    return 0;
}
static void fixture(void)
{
    unsigned char *root = disk + SECTOR;
    unsigned char *file = root + 64u;
    unsigned char *dir = disk + 3u * SECTOR;
    memset(disk, 0, sizeof(disk));
    writes = flushes = 0u;
    put32(disk + 0, 0x1badfaceu);
    put32(disk + 4, 2u * SECTOR + 64u); /* 9 inodes, unaligned data offset */
    put32(disk + 8, IMAGE_BYTES - 1u);
    put32(disk + 12, UINT32_MAX); /* No interrupted relocation */
    put32(disk + 16, UINT32_MAX);

    put16(root + 0, 2u);
    put32(root + 4, 3u);
    put32(root + 8, 3u);
    put32(root + 12, 3u * SECTOR + 16u - 1u);
    put32(root + 16, IFS_BFS_TYPE_DIRECTORY);

    put16(file + 0, 3u);
    put32(file + 4, 4u);
    put32(file + 8, 5u);
    put32(file + 12, 4u * SECTOR + 600u - 1u);
    put32(file + 16, IFS_BFS_TYPE_FILE);

    put16(dir, 3u);
    memcpy(dir + 2, "HELLO.TXT", 9u);
    memset(disk + 4u * SECTOR, 'A', 512u);
    memset(disk + 5u * SECTOR, 'B', 512u);
}
static int fail(const char *message)
{
    fprintf(stderr, "BFS.so: %s\n", message);
    return 1;
}
int main(void)
{
    IfsBfsVolume vol;
    IfsBfsIo io = {rd, wr, flush, NULL, IMAGE_BYTES};
    IfsBfsFile file;
    const IfsBfsModule *api = infiltrator_filesystem_module_v1();
    unsigned char data[600];
    if (!api || api->abi_version != 1u ||
        api->structure_bytes != sizeof(*api) ||
        strcmp(api->format_id, "bfs") ||
        (api->capabilities & IFS_BFS_CAP_EXISTING_WRITE) == 0u)
        return fail("module identity/capabilities");
    fixture();
    if (api->mount(&vol, &io) ||
        api->root_file(&vol, 0u, &file) ||
        file.inode_number != 3u ||
        file.size_bytes != 600u ||
        strcmp(file.name, "HELLO.TXT") ||
        api->root_file(&vol, 1u, &file) != 1)
        return fail("real BFS mount and root enumeration");
    if (api->lookup(&vol, "HELLO.TXT", 9u, &file) ||
        api->lookup(&vol, "hello.txt", 9u, &file) != 1 ||
        api->lookup(&vol, "HELLO", 5u, &file) != 1)
        return fail("case-sensitive exact-name lookup");
    if (api->read(&vol, &file, 0u, data, sizeof(data)))
        return fail("read");
    for (unsigned i = 0u; i < sizeof(data); ++i)
        if (data[i] != (unsigned char)(i < SECTOR ? 'A' : 'B'))
            return fail("contiguous data across blocks");
    if (api->overwrite_existing(&vol, &file, 510u, "xyz", 3u) ||
        writes != 1u || flushes != 1u ||
        api->read(&vol, &file, 510u, data, 3u) ||
        memcmp(data, "xyz", 3u))
        return fail("cross-block overwrite and flush");
    if (api->overwrite_existing(&vol, &file, 599u, "XX", 2u) == 0 ||
        writes != 1u || flushes != 1u ||
        api->read(&vol, &file, 599u, data, 2u) == 0)
        return fail("file end limits");

    /* Tampering with caller's inode description cannot authorize writes. */
    ++file.last_block;
    if (api->overwrite_existing(&vol, &file, 0u, "X", 1u) == 0)
        return fail("forged file allocation was accepted");
    --file.last_block;

    io.write_at = NULL;
    if (api->mount(&vol, &io) ||
        api->overwrite_existing(&vol, &file, 0u, "X", 1u) == 0)
        return fail("missing writer not rejected");
    io.write_at = wr;
    io.flush = NULL;
    if (api->mount(&vol, &io) ||
        api->overwrite_existing(&vol, &file, 0u, "X", 1u) == 0)
        return fail("missing flush not rejected");
    io.flush = flush;
    put32(disk + 12u, 8u);
    put32(disk + 16u, 9u);
    if (api->mount(&vol, &io) ||
        api->overwrite_existing(&vol, &file, 0u, "X", 1u) == 0)
        return fail("dirty relocation not protected");
    /* A single unfinished relocation marker is unsafe, not clean. */
    fixture();
    put32(disk + 12u, 8u);
    if (api->mount(&vol, &io) ||
        api->overwrite_existing(&vol, &file, 0u, "X", 1u) == 0)
        return fail("relocation source marker alone allowed a write");
    fixture();
    put32(disk + 16u, 9u);
    if (api->mount(&vol, &io) ||
        api->overwrite_existing(&vol, &file, 0u, "X", 1u) == 0)
        return fail("relocation target marker alone allowed a write");
    fixture();
    io.device_bytes = 1024u;
    if (api->mount(&vol, &io) == 0)
        return fail("truncated partition accepted");
    io.device_bytes = IMAGE_BYTES;
    disk[0] = 0;
    if (api->mount(&vol, &io) == 0)
        return fail("invalid magic accepted");
    fixture();
    put32(disk + 512u + 12u, UINT32_MAX); /* Corrupt root size */
    if (api->mount(&vol, &io) == 0)
        return fail("corrupt root geometry accepted");
    puts("BFS-STANDALONE-READ-OVERWRITE-AND-BOUNDS-PASS");
    return 0;
}
