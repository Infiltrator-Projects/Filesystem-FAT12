/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent 35 MiB FAT32 image: packed on-media FAT32 structures only. */
#include "fat32_module.h"
#include <stdio.h>
#include <string.h>
#define SECTOR 512u
#define SECTORS 70000u
#define RESERVED 32u
#define FAT_SECTORS 600u
#define DATA_START (RESERVED + 2u * FAT_SECTORS)
static unsigned char image[SECTORS * SECTOR];
static void p16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
}
static void p32(unsigned char *p, uint32_t v)
{
    p16(p, v & 0xffffu); p16(p+2, v >> 16);
}
static int read_at(void *opaque, uint64_t at, void *out, uint32_t n)
{
    (void)opaque;
    if (at > sizeof image || n > sizeof image - at) return -1;
    memcpy(out, image + (size_t)at, n);
    return 0;
}
static unsigned char *data(unsigned cluster)
{
    return image + (DATA_START + (cluster - 2u)) * SECTOR;
}
static void fat(unsigned cluster, uint32_t value)
{
    for (unsigned copy = 0; copy < 2u; ++copy)
        p32(image + (RESERVED + copy * FAT_SECTORS) * SECTOR + cluster * 4u,
            value);
}
static void fixture(void)
{
    memset(image, 0, sizeof image);
    unsigned char *b = image;
    b[0] = 0xeb; b[2] = 0x90;
    p16(b + 11, SECTOR);
    b[13] = 1u;
    p16(b + 14, RESERVED);
    b[16] = 2u;
    p16(b + 17, 0u);
    p16(b + 19, 0u);
    b[21] = 0xf8u;
    p16(b + 22, 0u);
    p32(b + 32, SECTORS);
    p32(b + 36, FAT_SECTORS);
    p32(b + 44, 2u);
    memcpy(b + 82, "FAT16   ", 8); /* Format label cannot change FAT32 semantics. */
    b[510] = 0x55u; b[511] = 0xaau;
    fat(0u, UINT32_C(0x0ffffff8));
    fat(1u, UINT32_C(0x0fffffff));
    fat(2u, 6u); /* Root directory traverses a real cluster chain. */
    fat(6u, UINT32_C(0x0fffffff));
    fat(3u, 7u); fat(7u, 4u); fat(4u, UINT32_C(0x0fffffff));
    memset(data(2u), 0xe5, SECTOR); /* All 16 entries are deleted. */
    unsigned char *dir = data(6u);
    memcpy(dir, "HELLO   TXT", 11);
    dir[11] = 0x20u;
    p16(dir + 20, 0u); p16(dir + 26, 3u);
    p32(dir + 28, 1200u);
    const unsigned chain[] = {3u, 7u, 4u};
    for (unsigned i = 0; i < 3u; ++i)
        memset(data(chain[i]), (int)('A' + i), SECTOR);
}
static int fail(const char *reason)
{
    fprintf(stderr, "fat32_module_test: %s\n", reason);
    return 1;
}
int main(void)
{
    const IfsFat32Module *api = infiltrator_filesystem_module_v1();
    IfsFat32Io io = {read_at, NULL, sizeof image};
    IfsFat32Volume vol = {0};
    IfsFat32File file = {0};
    uint32_t fragments;
    unsigned char bytes[1200];
    if (!api || api->abi_version != 1u ||
        api->structure_bytes != sizeof(*api) ||
        strcmp(api->format_id, "fat32") != 0 ||
        (api->capabilities & IFS_FAT32_CAP_FILE_READ) == 0 ||
        (api->capabilities & IFS_FAT32_CAP_ROOT_FRAGMENT_COUNT) == 0)
        return fail("ABI/identity missing");
    fixture();
    if (api->probe(&io, &vol) != 0 || vol.root_cluster != 2u ||
        vol.cluster_count < 65525u || vol.cluster_bytes != SECTOR)
        return fail("real FAT32 geometry rejected");
    if (api->root_file(&io, &vol, 0u, &file) != 0 ||
        strcmp(file.name, "HELLO.TXT") != 0 || file.size_bytes != 1200u)
        return fail("root directory cluster traversal failed");
    if (api->root_file(&io, &vol, 1u, &file) != 1)
        return fail("extra root directory entry accepted");
    if (api->root_file_fragments(&io, &vol, &file, &fragments) != 0 ||
        fragments != 3u)
        return fail("fragment count incorrect");
    if (api->read_file(&io, &vol, &file, 0u, bytes, sizeof bytes) != 0)
        return fail("fragmented file read failed");
    for (unsigned i = 0; i < sizeof bytes; ++i)
        if (bytes[i] != (unsigned char)('A' + i / SECTOR))
            return fail("FAT32 file contents incorrect");
    if (api->read_file(&io, &vol, &file, 1199u, bytes, 2u) == 0)
        return fail("out-of-file read accepted");
    fat(7u, UINT32_C(0x0fffffff));
    if (api->root_file_fragments(&io, &vol, &file, &fragments) == 0)
        return fail("short FAT32 file chain accepted");
    fixture();
    fat(2u, 0u);
    if (api->root_file(&io, &vol, 0u, &file) == 0)
        return fail("invalid root FAT chain accepted");
    fixture();
    image[13] = 2u; /* FAT16 cluster range, regardless of FAT32 label. */
    if (api->probe(&io, &vol) == 0)
        return fail("FAT16 sized partition falsely probed as FAT32");
    fixture();
    p32(image + 44, 1u); /* Reserved cluster may not be root. */
    if (api->probe(&io, &vol) == 0)
        return fail("invalid root cluster accepted");
    fixture();
    image[RESERVED * SECTOR] = 0u;
    if (api->probe(&io, &vol) == 0)
        return fail("invalid FAT32 reserved entry accepted");
    fixture();
    io.device_bytes = 512u;
    if (api->probe(&io, &vol) == 0)
        return fail("truncated partition accepted");
    puts("FAT32-MODULE-READ-ROOT-CLUSTER-AND-FRAGMENT-PASS");
    return 0;
}
