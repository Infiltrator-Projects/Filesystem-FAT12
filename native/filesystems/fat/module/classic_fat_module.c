/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Classic FAT format arithmetic follows the validated BPB and FAT-entry models from
 * Infiltrator-Projects/Defragmenter:
 * defragger/gui/filesystems/fat/native/fat_volume.c and Test Media's
 * test_media_raw_fat.c. This translation removes Linux/Defragmenter runtime
 * dependencies to enable one self-contained, read-only format module.
 */
#include "classic_fat_module.h"
#ifndef IFS_CLASSIC_FAT_BITS
#error "Build one FAT variant at a time via its explicit format wrapper"
#endif
#if IFS_CLASSIC_FAT_BITS == 12
#define IFS_CLASSIC_FAT_ID "fat12"
#define IFS_CLASSIC_FAT_NAME "FAT12"
#elif IFS_CLASSIC_FAT_BITS == 16
#define IFS_CLASSIC_FAT_ID "fat16"
#define IFS_CLASSIC_FAT_NAME "FAT16"
#else
#error "Only independently qualified FAT12 and FAT16 are supported"
#endif



static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int pow2(uint32_t n) { return n && (n & (n - 1u)) == 0; }

static int bounded_read(const IfsClassicFatIo *io, uint64_t offset,
                        void *dst, uint32_t n)
{
    if (!io || !io->read_at || !dst || offset > io->device_bytes ||
        n > io->device_bytes - offset)
        return -1;
    return io->read_at(io->context, offset, dst, n) == 0 ? 0 : -1;
}

static int classic_fat_probe(const IfsClassicFatIo *io, IfsClassicFatVolume *v)
{
    uint8_t boot[512];
    if (!v || bounded_read(io, 0, boot, sizeof boot) != 0 ||
        boot[510] != 0x55u || boot[511] != 0xaau)
        return -1;
    const uint32_t bps = le16(boot + 11);
    const uint32_t spc = boot[13];
    const uint32_t reserved = le16(boot + 14);
    const uint32_t copies = boot[16];
    const uint32_t roots = le16(boot + 17);
    const uint32_t spf = le16(boot + 22);
    const uint32_t total = le16(boot + 19) ? le16(boot + 19) : le32(boot + 32);
    if ((bps != 512u && bps != 1024u && bps != 2048u && bps != 4096u) ||
        !pow2(spc) || spc > 128u || !reserved || !copies || copies > 4u ||
        !roots || !spf || !total || le16(boot + 11) != bps)
        return -1;

    const uint64_t root_sectors = ((uint64_t)roots * 32u + bps - 1u) / bps;
    const uint64_t before_data = (uint64_t)reserved +
                                 (uint64_t)copies * spf + root_sectors;
    if (before_data >= total || (uint64_t)total * bps > io->device_bytes)
        return -1;

    const uint64_t clusters = (total - before_data) / spc;
    /* Format classification depends on cluster COUNT, never FAT16 string. */
#if IFS_CLASSIC_FAT_BITS == 12
    if (clusters == 0 || clusters >= 4085u ||
        ((uint64_t)spf * bps * 2u) / 3u <= clusters + 1u)
        return -1;
#else
    if (clusters < 4085u || clusters >= 65525u ||
        (uint64_t)spf * bps / 2u <= clusters + 1u)
        return -1;
#endif
    const uint64_t root_start =
        ((uint64_t)reserved + (uint64_t)copies * spf) * bps;
    const uint64_t data_start = before_data * bps;
    const uint64_t volume_size = (uint64_t)total * bps;
    if (data_start >= volume_size || root_start + (uint64_t)roots * 32u > data_start)
        return -1;

    IfsClassicFatVolume result = {0};
    result.volume_bytes = volume_size;
    result.fat_offset = (uint64_t)reserved * bps;
    result.root_offset = root_start;
    result.data_offset = data_start;
    result.cluster_bytes = bps * spc;
    result.cluster_count = (uint32_t)clusters;
    result.total_sectors = total;
    result.root_entries = roots;
    result.bytes_per_sector = (uint16_t)bps;
    result.sectors_per_fat = (uint16_t)spf;
    result.sectors_per_cluster = (uint8_t)spc;
    result.fat_copies = (uint8_t)copies;

    /* Reject uninitialised tables: reserved entries encode media and EOC. */
    uint8_t first[4];
    if (bounded_read(io, result.fat_offset, first, sizeof first) != 0 ||
        first[0] != boot[21] || first[1] != 0xffu)
        return -1;
#if IFS_CLASSIC_FAT_BITS == 12
    if (first[2] != 0xffu) return -1;
#else
    if (le16(first + 2) < 0xfff8u) return -1;
#endif
    *v = result;
    return 0;
}
static int fat_entry(const IfsClassicFatIo *io, const IfsClassicFatVolume *v,
                     uint32_t cluster, uint16_t *next)
{
    if (!v || !next || cluster < 2u || cluster > v->cluster_count + 1u)
        return -1;
#if IFS_CLASSIC_FAT_BITS == 12
    const uint64_t offset = v->fat_offset +
                            (uint64_t)cluster + (uint64_t)cluster / 2u;
#else
    const uint64_t offset = v->fat_offset + (uint64_t)cluster * 2u;
#endif
    if (offset + 2u > v->fat_offset +
                     (uint64_t)v->sectors_per_fat * v->bytes_per_sector)
        return -1;
    uint8_t pair[2];
    if (bounded_read(io, offset, pair, sizeof pair) != 0)
        return -1;
#if IFS_CLASSIC_FAT_BITS == 12
    *next = (cluster & 1u)
        ? (uint16_t)(((uint16_t)pair[0] >> 4) |
                     ((uint16_t)pair[1] << 4))
        : (uint16_t)((uint16_t)pair[0] |
                     (((uint16_t)pair[1] & 0x0fu) << 8));
#else
    *next = le16(pair);
#endif
    return 0;
}
static int classic_fat_root_file(const IfsClassicFatIo *io, const IfsClassicFatVolume *v,
                           uint32_t nth, IfsClassicFatFile *file)
{
    if (!v || !file) return -1;
    if (nth >= v->root_entries) return 1;
    uint32_t visible = 0;
    for (uint32_t i = 0; i < v->root_entries; ++i) {
        uint8_t dir[32];
        if (bounded_read(io, v->root_offset + (uint64_t)i * 32u,
                         dir, sizeof dir) != 0)
            return -1;
        if (dir[0] == 0) return 1;
        if (dir[0] == 0xe5u || dir[11] == 0x0fu ||
            (dir[11] & 0x08u) != 0)
            continue; /* tombstone, LFN, volume label */
        if (visible++ != nth) continue;
        IfsClassicFatFile out = {{0}, 0, 0, dir[11]};
        uint32_t n = 0;
        for (uint32_t j = 0; j < 8 && dir[j] != ' '; ++j) {
            unsigned char c = dir[j];
            if ((c < 0x20u && !(j == 0u && c == 0x05u)) || c > 0x7eu) return -1;
            out.name[n++] = (char)(j == 0 && c == 0x05u ? 0xe5u : c);
        }
        uint32_t extension = 0;
        for (uint32_t j = 8; j < 11 && dir[j] != ' '; ++j) ++extension;
        if (extension) {
            out.name[n++] = '.';
            for (uint32_t j = 8; j < 11 && dir[j] != ' '; ++j) {
                unsigned char c = dir[j];
                if (c < 0x20u || c > 0x7eu) return -1;
                out.name[n++] = (char)c;
            }
        }
        out.name[n] = '\0';
        out.first_cluster = le16(dir + 26);
        out.size_bytes = le32(dir + 28);
        *file = out;
        return 0;
    }
    return 1;
}
static int validate_chain(const IfsClassicFatIo *io, const IfsClassicFatVolume *v,
                          const IfsClassicFatFile *f, uint32_t *fragments)
{
    if (!v || !f || !fragments || (f->attributes & 0x10u))
        return -1;
    const uint32_t count = f->size_bytes / v->cluster_bytes +
                           (f->size_bytes % v->cluster_bytes != 0);
    if (!count) {
        if (f->first_cluster != 0u) return -1;
        *fragments = 0;
        return 0;
    }
    if (count > v->cluster_count || f->first_cluster < 2u ||
        f->first_cluster > v->cluster_count + 1u)
        return -1;
    uint32_t cluster = f->first_cluster;
    uint32_t runs = 1;
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t next;
        if (fat_entry(io, v, cluster, &next) != 0) return -1;
        if (i + 1u == count) {
#if IFS_CLASSIC_FAT_BITS == 12
            if (next < 0x0ff8u) return -1;
#else
            if (next < 0xfff8u) return -1;
#endif
            *fragments = runs;
            return 0;
        }
#if IFS_CLASSIC_FAT_BITS == 12
        if (next < 2u || next > v->cluster_count + 1u || next >= 0x0ff0u)
#else
        if (next < 2u || next > v->cluster_count + 1u || next >= 0xfff0u)
#endif
            return -1;
        if (next != cluster + 1u) ++runs;
        cluster = next;
    }
    return -1;
}
static int classic_fat_read_file(const IfsClassicFatIo *io, const IfsClassicFatVolume *v,
                           const IfsClassicFatFile *f, uint64_t position,
                           void *buffer, uint32_t length)
{
    uint32_t fragments;
    if (!buffer || validate_chain(io, v, f, &fragments) != 0 ||
        position > f->size_bytes || length > f->size_bytes - position)
        return -1;
    if (length == 0) return 0;
    uint32_t cluster = f->first_cluster;
    uint64_t skip = position / v->cluster_bytes;
    for (uint64_t i = 0; i < skip; ++i) {
        uint16_t next;
        if (fat_entry(io, v, cluster, &next) != 0 ||
            next < 2u || next > v->cluster_count + 1u)
            return -1;
        cluster = next;
    }
    uint32_t within = (uint32_t)(position % v->cluster_bytes);
    uint8_t *dst = (uint8_t *)buffer;
    while (length != 0) {
        uint32_t take = v->cluster_bytes - within;
        if (take > length) take = length;
        uint64_t pos = v->data_offset +
                       (uint64_t)(cluster - 2u) * v->cluster_bytes + within;
        if (pos + take > v->volume_bytes ||
            bounded_read(io, pos, dst, take) != 0)
            return -1;
        dst += take;
        length -= take;
        within = 0;
        if (length != 0) {
            uint16_t next;
            if (fat_entry(io, v, cluster, &next) != 0 ||
                next < 2u || next > v->cluster_count + 1u)
                return -1;
            cluster = next;
        }
    }
    return 0;
}
/* Non-executed format identity, tied to the compiled variant. */
__attribute__((used,section(".ifsmeta")))
static const char ifs_module_format_identity[] =
    "IFS-MODULE:" IFS_CLASSIC_FAT_ID ":1";
static const IfsClassicFatModule module = {
    IFS_CLASSIC_FAT_ABI_VERSION, sizeof(IfsClassicFatModule),
    IFS_CLASSIC_FAT_ID, IFS_CLASSIC_FAT_NAME,
    IFS_CLASSIC_FAT_CAP_PROBE | IFS_CLASSIC_FAT_CAP_ROOT_83 |
    IFS_CLASSIC_FAT_CAP_FILE_READ | IFS_CLASSIC_FAT_CAP_ROOT_FRAGMENT_COUNT,
    classic_fat_probe, classic_fat_root_file, classic_fat_read_file, validate_chain
};
__attribute__((visibility("default")))
const IfsClassicFatModule *infiltrator_filesystem_module_v1(void)
{
    return &module;
}
