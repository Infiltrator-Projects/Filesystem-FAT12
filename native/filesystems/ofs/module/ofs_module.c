/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Single-file OFS.so adapter of the already-qualified OFS EFI reader,
 * shared canonical OFS primitives and DOS type classification.
 * No EFI protocol or Linux kernel symbols are imported.
 */
#include "ofs_module.h"

static int block_read(void *opaque, uint32_t block, uint8_t data[512])
{
    const IfsOfsDropInIo *io = (const IfsOfsDropInIo *)opaque;
    const uint64_t offset = (uint64_t)block * UINT64_C(512);
    if (!io || !io->read_at || offset > io->volume_bytes ||
        UINT64_C(512) > io->volume_bytes - offset)
        return -1;
    return io->read_at(io->context, offset, data, 512u) == 0 ? 0 : -1;
}
static int module_mount(IfsOfsDropInVolume *volume,
                        const IfsOfsDropInIo *io)
{
    uint64_t blocks;
    if (!volume || !io || !io->read_at || io->volume_bytes % 512u ||
        io->volume_bytes < 16u * UINT64_C(512))
        return -1;
    blocks = io->volume_bytes / 512u;
    if (blocks > UINT32_MAX)
        return -1;
    volume->io = *io;
    return ifs_ofs_efi_mount(&volume->reader, (uint32_t)blocks,
                             block_read, &volume->io);
}
static IfsOfsEfiEntry module_root(const IfsOfsDropInVolume *v)
{
    return ifs_ofs_efi_root(v ? &v->reader : 0);
}
static int module_lookup(const IfsOfsDropInVolume *v,
                         const IfsOfsEfiEntry *dir,
                         const uint8_t *name, uint32_t length,
                         IfsOfsEfiEntry *out)
{
    return v ? ifs_ofs_efi_lookup(&v->reader, dir, name, length, out) : -1;
}
static int module_next(const IfsOfsDropInVolume *v,
                       const IfsOfsEfiEntry *dir, IfsOfsEfiDirCursor *cursor,
                       IfsOfsEfiEntry *out, uint8_t name[31])
{
    return v ? ifs_ofs_efi_next(&v->reader, dir, cursor, out, name) : -1;
}
static int module_read(const IfsOfsDropInVolume *v,
                       const IfsOfsEfiEntry *file, uint32_t offset,
                       void *buffer, uint32_t length,
                       IfsOfsEfiFileCursor *cursor)
{
    return v ? ifs_ofs_efi_read(&v->reader, file, offset, buffer, length,
                                cursor) : -1;
}
/* Non-executed format identity for safe single-file selection. */
__attribute__((used,section(".ifsmeta")))
static const char ifs_module_format_identity[] = "IFS-MODULE:ofs:1";
static const IfsOfsDropInModule module = {
    IFS_OFS_DROPIN_ABI_VERSION,
    sizeof(IfsOfsDropInModule),
    "ofs",
    "Amiga Original File System",
    IFS_OFS_CAP_PROBE | IFS_OFS_CAP_LOOKUP |
    IFS_OFS_CAP_ENUMERATE | IFS_OFS_CAP_FILE_READ |
    IFS_OFS_CAP_DOS0 | IFS_OFS_CAP_DOS2 |
    IFS_OFS_CAP_DOS4 | IFS_OFS_CAP_BLOCK512,
    module_mount, module_root, module_lookup, module_next, module_read
};
__attribute__((visibility("default")))
const IfsOfsDropInModule *infiltrator_filesystem_module_v1(void)
{
    return &module;
}
