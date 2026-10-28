/* SPDX-License-Identifier: GPL-2.0-only */
/* SCO UnixWare BFS on-disk semantics adapted from this repository's original
 * native/filesystems/bfs/linux/{inode,dir,file}.c and Linux bfs_fs.h.
 * Fixed block size 512, inode record 64 bytes, directory record 16 bytes,
 * contiguous file allocations. No libc or Linux VFS dependencies.
 */
#include "bfs_module.h"

#define BFS_MAGIC UINT32_C(0x1badface)
#define BFS_INODE_BYTES 64u
#define BFS_DIRECTORY_BYTES 16u
#define BFS_INODE_START 512u
#define BFS_SUPER_BYTES 512u
#define BFS_BAD_INDEX UINT32_C(0xffffffff)

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int within(const IfsBfsVolume *v, uint64_t offset, uint32_t count)
{
    return v && offset <= v->volume_bytes &&
           (uint64_t)count <= v->volume_bytes - offset;
}

static int read_at(const IfsBfsVolume *v, uint64_t offset,
                   void *target, uint32_t count)
{
    return v && v->io.read_at && target && within(v, offset, count) &&
           v->io.read_at(v->io.user, offset, target, count) == 0 ? 0 : -1;
}

static int fetch_inode(const IfsBfsVolume *, uint32_t, IfsBfsFile *);

static int mount_volume(IfsBfsVolume *v, const IfsBfsIo *io)
{
    unsigned char b[BFS_SUPER_BYTES];
    uint32_t start, end, from, to;
    uint64_t blocks;
    if (!v || !io || !io->read_at || io->device_bytes < 1536u)
        return -1;
    if (io->read_at(io->user, 0u, b, BFS_SUPER_BYTES) != 0 ||
        le32(b) != BFS_MAGIC)
        return -1;
    start = le32(b + 4);
    end = le32(b + 8);
    from = le32(b + 12);
    to = le32(b + 16);
    /* mkfs.bfs stores the byte offset immediately after its inode table.
     * The inode count need not be a multiple of 8; the data blocks begin
     * at the next 512-byte boundary. Do not reject legitimate media.
     */
    if (start < BFS_INODE_START + BFS_INODE_BYTES ||
        (start - BFS_INODE_START) % BFS_INODE_BYTES != 0u ||
        end < start || (uint64_t)end >= io->device_bytes)
        return -1;
    blocks = ((uint64_t)end + 1u) / BFS_SUPER_BYTES;
    if (((uint64_t)end + 1u) % BFS_SUPER_BYTES ||
        blocks < 3u || blocks > UINT32_MAX ||
        ((uint64_t)start + BFS_SUPER_BYTES - 1u) /
            BFS_SUPER_BYTES >= blocks)
        return -1;
    v->io = *io;
    v->volume_bytes = (uint64_t)end + 1u;
    v->data_start_block = (start / BFS_SUPER_BYTES) +
                          ((start % BFS_SUPER_BYTES) != 0u);
    v->total_blocks = (uint32_t)blocks;
    v->max_inode = (start - BFS_INODE_START) / BFS_INODE_BYTES +
                   IFS_BFS_ROOT_INODE - 1u;
    if (v->max_inode > 513u || v->max_inode < IFS_BFS_ROOT_INODE)
        return -1;
    v->clean = from == BFS_BAD_INDEX && to == BFS_BAD_INDEX;
    /* The original Linux driver treats both active relocation markers
     * as a dirty filesystem. This module rejects writes to one.
     */
    {
        IfsBfsFile root;
        if (fetch_inode(v, IFS_BFS_ROOT_INODE, &root) != 0 ||
            root.type != IFS_BFS_TYPE_DIRECTORY ||
            root.size_bytes == 0u ||
            root.size_bytes % BFS_DIRECTORY_BYTES != 0u)
            return -1;
    }
    return 0;
}

static int fetch_inode(const IfsBfsVolume *v, uint32_t inode,
                       IfsBfsFile *file)
{
    unsigned char raw[BFS_INODE_BYTES];
    uint64_t off;
    uint32_t first, last, eoff, type;
    uint64_t size;
    if (!v || !file || inode < IFS_BFS_ROOT_INODE || inode > v->max_inode)
        return -1;
    off = BFS_INODE_START +
          (uint64_t)(inode - IFS_BFS_ROOT_INODE) * BFS_INODE_BYTES;
    if (off + BFS_INODE_BYTES >
            (uint64_t)v->data_start_block * BFS_SUPER_BYTES ||
        read_at(v, off, raw, sizeof(raw)) != 0 || le16(raw) != inode)
        return -1;
    first = le32(raw + 4);
    last = le32(raw + 8);
    eoff = le32(raw + 12);
    type = le32(raw + 16);
    if (type != IFS_BFS_TYPE_DIRECTORY && type != IFS_BFS_TYPE_FILE)
        return -1;
    if (first == 0u) {
        if (last != 0u || type == IFS_BFS_TYPE_DIRECTORY)
            return -1;
        size = 0u;
    } else {
        if (first < v->data_start_block || first >= v->total_blocks ||
            last < first || last >= v->total_blocks ||
            (uint64_t)eoff >= v->volume_bytes ||
            (uint64_t)eoff + 1u < (uint64_t)first * BFS_SUPER_BYTES ||
            (uint64_t)eoff >= ((uint64_t)last + 1u) * BFS_SUPER_BYTES)
            return -1;
        size = (uint64_t)eoff + 1u - (uint64_t)first * BFS_SUPER_BYTES;
        if (size > UINT32_MAX)
            return -1;
    }
    file->inode_number = inode;
    file->type = type;
    file->first_block = first;
    file->last_block = last;
    file->size_bytes = (uint32_t)size;
    file->name[0] = '\0';
    return 0;
}

static int root_file(const IfsBfsVolume *v, uint32_t nth, IfsBfsFile *out)
{
    IfsBfsFile root;
    unsigned char dir[BFS_DIRECTORY_BYTES];
    uint32_t i, seen = 0u;
    if (!v || !out || fetch_inode(v, IFS_BFS_ROOT_INODE, &root) != 0 ||
        root.type != IFS_BFS_TYPE_DIRECTORY ||
        root.size_bytes % BFS_DIRECTORY_BYTES != 0u)
        return -1;
    for (i = 0u; i < root.size_bytes / BFS_DIRECTORY_BYTES; ++i) {
        uint32_t n, j;
        uint64_t offset = (uint64_t)root.first_block *
            BFS_SUPER_BYTES + (uint64_t)i * BFS_DIRECTORY_BYTES;
        if (read_at(v, offset, dir, sizeof(dir)) != 0)
            return -1;
        n = le16(dir);
        if (n == 0u)
            continue;
        if (n < IFS_BFS_ROOT_INODE || n > v->max_inode)
            return -1;
        if (seen++ != nth)
            continue;
        if (fetch_inode(v, n, out) != 0)
            return -1;
        for (j = 0u; j < IFS_BFS_MAX_NAME && dir[2u + j]; ++j)
            out->name[j] = (char)dir[2u + j];
        out->name[j] = '\0';
        return j != 0u ? 0 : -1;
    }
    return 1;
}

static int lookup(const IfsBfsVolume *v, const char *name,
                  uint32_t length, IfsBfsFile *out)
{
    IfsBfsFile root;
    unsigned char dir[BFS_DIRECTORY_BYTES];
    uint32_t i;
    if (!v || !out || !name || !length || length > IFS_BFS_MAX_NAME ||
        fetch_inode(v, IFS_BFS_ROOT_INODE, &root) != 0 ||
        root.size_bytes % BFS_DIRECTORY_BYTES != 0u)
        return -1;
    for (i = 0u; i < root.size_bytes / BFS_DIRECTORY_BYTES; ++i) {
        uint32_t j, n;
        uint64_t offset = (uint64_t)root.first_block * BFS_SUPER_BYTES +
                          (uint64_t)i * BFS_DIRECTORY_BYTES;
        if (read_at(v, offset, dir, sizeof(dir)) != 0)
            return -1;
        n = le16(dir);
        if (!n)
            continue;
        if (n > v->max_inode || n < IFS_BFS_ROOT_INODE)
            return -1;
        for (j = 0u; j < length && dir[2u + j] == (unsigned char)name[j]; ++j)
            ;
        if (j == length &&
            (length == IFS_BFS_MAX_NAME || dir[2u + length] == 0u)) {
            if (fetch_inode(v, n, out) != 0)
                return -1;
            for (j = 0u; j < length; ++j)
                out->name[j] = name[j];
            out->name[length] = '\0';
            return 0;
        }
    }
    return 1;
}

static int valid_file(const IfsBfsVolume *v, const IfsBfsFile *f)
{
    IfsBfsFile current;
    return f && f->type == IFS_BFS_TYPE_FILE &&
           fetch_inode(v, f->inode_number, &current) == 0 &&
           current.type == f->type &&
           current.first_block == f->first_block &&
           current.last_block == f->last_block &&
           current.size_bytes == f->size_bytes;
}

static int read_file(const IfsBfsVolume *v, const IfsBfsFile *f,
                     uint64_t position, void *buf, uint32_t bytes)
{
    uint64_t offset;
    if (!valid_file(v, f) || (!buf && bytes != 0u) ||
        position > f->size_bytes ||
        (uint64_t)bytes > f->size_bytes - position)
        return -1;
    if (!bytes)
        return 0;
    offset = (uint64_t)f->first_block * BFS_SUPER_BYTES + position;
    return read_at(v, offset, buf, bytes);
}

static int overwrite_file(const IfsBfsVolume *v, const IfsBfsFile *f,
                          uint64_t position, const void *src, uint32_t bytes)
{
    uint64_t offset;
    if (!v || !v->clean || !v->io.write_at || !v->io.flush ||
        !valid_file(v, f) || (!src && bytes != 0u) ||
        position > f->size_bytes ||
        (uint64_t)bytes > f->size_bytes - position)
        return -1;
    if (!bytes)
        return 0;
    offset = (uint64_t)f->first_block * BFS_SUPER_BYTES + position;
    if (!within(v, offset, bytes) ||
        v->io.write_at(v->io.user, offset, src, bytes) != 0)
        return -1;
    return v->io.flush(v->io.user) == 0 ? 0 : -1;
}

__attribute__((used, section(".ifsmeta")))
static const char format_identity[] = "IFS-MODULE:bfs:1";

static const IfsBfsModule module = {
    IFS_BFS_ABI_VERSION, sizeof(IfsBfsModule),
    "bfs", "SCO UnixWare Boot File System",
    IFS_BFS_CAP_PROBE | IFS_BFS_CAP_ROOT_DIRECTORY |
    IFS_BFS_CAP_FILE_READ | IFS_BFS_CAP_EXISTING_WRITE,
    mount_volume, root_file, lookup, read_file, overwrite_file
};

__attribute__((visibility("default")))
const IfsBfsModule *infiltrator_filesystem_module_v1(void)
{
    return &module;
}
