/* SPDX-License-Identifier: GPL-2.0-only */
/* Inspect a real mkfs.bfs-created ordinary image, without mounting any disk. */
#include "bfs_module.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct HostFile {
    FILE *f;
    uint64_t size;
} HostFile;

static int read_at(void *p, uint64_t offset, void *dst, uint32_t count)
{
    HostFile *h = (HostFile *)p;
    if (!h || !h->f || offset > h->size ||
        (uint64_t)count > h->size - offset ||
        offset > 0x7fffffffu)
        return -1;
    if (fseek(h->f, (long)offset, SEEK_SET) != 0)
        return -1;
    return fread(dst, 1u, count, h->f) == count ? 0 : -1;
}
static int failed(const char *why)
{
    fprintf(stderr, "mkfs.bfs interop: %s\n", why);
    return 1;
}
int main(int argc, char **argv)
{
    HostFile host;
    IfsBfsIo io;
    IfsBfsVolume volume;
    IfsBfsFile entry;
    const IfsBfsModule *module = infiltrator_filesystem_module_v1();
    unsigned char sb[20];
    uint32_t start;
    int rc = 1;
    if (argc != 2 || !module)
        return failed("expected mkfs.bfs image argument");
    host.f = fopen(argv[1], "rb");
    if (!host.f)
        return failed("cannot open image");
    if (fseek(host.f, 0, SEEK_END) != 0)
        goto done;
    {
        long size = ftell(host.f);
        if (size < 1536L)
            goto done;
        host.size = (uint64_t)size;
    }
    io.read_at = read_at;
    io.write_at = NULL;
    io.flush = NULL;
    io.user = &host;
    io.device_bytes = host.size;
    if (read_at(&host, 0, sb, sizeof(sb)) != 0)
        goto done;
    start = (uint32_t)sb[4] | ((uint32_t)sb[5] << 8) |
            ((uint32_t)sb[6] << 16) | ((uint32_t)sb[7] << 24);
    if (module->mount(&volume, &io) != 0 ||
        volume.data_start_block != start / 512u +
            ((start % 512u) != 0u) ||
        module->root_file(&volume, 0u, &entry) != 0 ||
        strcmp(entry.name, ".") != 0 ||
        entry.type != IFS_BFS_TYPE_DIRECTORY ||
        module->root_file(&volume, 1u, &entry) != 0 ||
        strcmp(entry.name, "..") != 0 ||
        module->root_file(&volume, 2u, &entry) != 1 ||
        module->lookup(&volume, ".", 1u, &entry) != 0 ||
        module->read(&volume, &entry, 0, sb, 1u) == 0 ||
        module->overwrite_existing(&volume, &entry, 0u, sb, 1u) == 0)
        goto done;
    puts("BFS-REAL-MKFS-INTEROP-PASS");
    rc = 0;
done:
    if (rc)
        fprintf(stderr, "mkfs.bfs native reader failed image interoperability\n");
    fclose(host.f);
    return rc;
}
