/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Standalone FAT32 reader derived from classic FAT format logic. The
 * 28-bit FAT entries, cluster-chain root and FAT32 BPB have their own rules.
 * No unsafe writes, formatting, LFN reconstruction or directory mutation.
 */
#include "fat32_module.h"
static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int power2(uint32_t v) { return v != 0u && (v & (v - 1u)) == 0u; }
static int read_at(const IfsFat32Io *io, uint64_t pos,
                   void *out, uint32_t size)
{
    if (!io || !io->read_at || !out || pos > io->device_bytes ||
        size > io->device_bytes - pos)
        return -1;
    return io->read_at(io->context, pos, out, size) == 0 ? 0 : -1;
}
static int probe(const IfsFat32Io *io, IfsFat32Volume *out)
{
    uint8_t boot[512], reserved[8];
    if (!io || !out || read_at(io, 0, boot, sizeof boot) != 0 ||
        boot[510] != 0x55u || boot[511] != 0xaau)
        return -1;
    const uint32_t bps = rd16(boot + 11), spc = boot[13];
    const uint32_t reserved_count = rd16(boot + 14);
    const uint32_t copies = boot[16], root_entries = rd16(boot + 17);
    const uint32_t total16 = rd16(boot + 19), fat16 = rd16(boot + 22);
    const uint32_t total32 = rd32(boot + 32), fat32 = rd32(boot + 36);
    const uint32_t root = rd32(boot + 44);
    if ((bps != 512u && bps != 1024u && bps != 2048u && bps != 4096u) ||
        !power2(spc) || spc > 128u || !reserved_count ||
        !copies || copies > 4u || root_entries || total16 || fat16 ||
        !total32 || !fat32 || rd16(boot + 42) != 0u)
        return -1;
    const uint64_t data_sector = (uint64_t)reserved_count +
                                 (uint64_t)copies * fat32;
    if (data_sector >= total32 || (uint64_t)total32 * bps > io->device_bytes)
        return -1;
    const uint64_t clusters = (total32 - data_sector) / spc;
    /* FAT32 classification uses cluster count, never the BPB type string. */
    if (clusters < 65525u || clusters > UINT32_C(0x0fffffef) ||
        ((uint64_t)fat32 * bps) / 4u < clusters + 2u ||
        root < 2u || root > clusters + 1u)
        return -1;
    const uint64_t fat_offset = (uint64_t)reserved_count * bps;
    if (read_at(io, fat_offset, reserved, sizeof reserved) != 0 ||
        reserved[0] != boot[21] ||
        (rd32(reserved) & UINT32_C(0x0fffff00)) != UINT32_C(0x0fffff00) ||
        (rd32(reserved + 4) & UINT32_C(0x0fffffff)) <
            UINT32_C(0x0ffffff8))
        return -1;
    IfsFat32Volume v = {0};
    v.volume_bytes = (uint64_t)total32 * bps;
    v.fat_offset = fat_offset;
    v.data_offset = data_sector * bps;
    v.cluster_bytes = bps * spc;
    v.cluster_count = (uint32_t)clusters;
    v.sectors_per_fat = fat32;
    v.root_cluster = root;
    v.total_sectors = total32;
    v.bytes_per_sector = (uint16_t)bps;
    v.sectors_per_cluster = (uint8_t)spc;
    v.fat_copies = (uint8_t)copies;
    *out = v;
    return 0;
}
static int cluster_valid(const IfsFat32Volume *v, uint32_t cluster)
{
    return v && cluster >= 2u && cluster <= v->cluster_count + 1u &&
        cluster < UINT32_C(0x0ffffff0);
}
static int fat_entry(const IfsFat32Io *io, const IfsFat32Volume *v,
                     uint32_t cluster, uint32_t *next)
{
    if (!next || !cluster_valid(v, cluster)) return -1;
    const uint64_t offset = v->fat_offset + (uint64_t)cluster * 4u;
    if (offset + 4u > v->fat_offset +
        (uint64_t)v->sectors_per_fat * v->bytes_per_sector)
        return -1;
    uint8_t p[4];
    if (read_at(io, offset, p, sizeof p) != 0) return -1;
    *next = rd32(p) & UINT32_C(0x0fffffff);
    return 0;
}
static int cluster_read(const IfsFat32Io *io, const IfsFat32Volume *v,
                        uint32_t cluster, uint32_t within,
                        void *buffer, uint32_t size)
{
    if (!cluster_valid(v, cluster) || within > v->cluster_bytes ||
        size > v->cluster_bytes - within)
        return -1;
    const uint64_t at = v->data_offset +
        (uint64_t)(cluster - 2u) * v->cluster_bytes + within;
    if (at > v->volume_bytes || size > v->volume_bytes - at)
        return -1;
    return read_at(io, at, buffer, size);
}
static int next_data_cluster(const IfsFat32Io *io,
                             const IfsFat32Volume *v,
                             uint32_t current, uint32_t *next)
{
    return fat_entry(io, v, current, next) == 0 &&
           cluster_valid(v, *next) ? 0 : -1;
}
static int root_file(const IfsFat32Io *io, const IfsFat32Volume *v,
                     uint32_t nth, IfsFat32File *file)
{
    if (!io || !v || !file || v->cluster_bytes % 32u) return -1;
    uint32_t current = v->root_cluster, seen = 0u;
    const uint32_t per_cluster = v->cluster_bytes / 32u;
    for (uint32_t step = 0; step < v->cluster_count; ++step) {
        for (uint32_t index = 0; index < per_cluster; ++index) {
            uint8_t d[32];
            if (cluster_read(io, v, current, index * 32u,
                             d, sizeof d) != 0)
                return -1;
            if (!d[0]) return 1;
            if (d[0] == 0xe5u || d[11] == 0x0fu ||
                (d[11] & 0x08u) != 0u)
                continue;
            if (seen++ != nth) continue;
            IfsFat32File f = {{0}, 0, 0, d[11]};
            uint32_t len = 0;
            for (uint32_t j = 0; j < 8u && d[j] != ' '; ++j) {
                const uint8_t ch = d[j];
                if ((ch < 0x20u && !(j == 0u && ch == 0x05u)) ||
                    ch > 0x7eu)
                    return -1;
                f.name[len++] = (char)(j == 0u && ch == 0x05u ? 0xe5u : ch);
            }
            uint32_t ext = 0;
            for (uint32_t j = 8u; j < 11u && d[j] != ' '; ++j) ++ext;
            if (ext) {
                f.name[len++] = '.';
                for (uint32_t j = 8u; j < 11u && d[j] != ' '; ++j) {
                    const uint8_t ch = d[j];
                    if (ch < 0x20u || ch > 0x7eu) return -1;
                    f.name[len++] = (char)ch;
                }
            }
            f.name[len] = '\0';
            f.first_cluster = ((uint32_t)rd16(d + 20) << 16) | rd16(d + 26);
            f.size_bytes = rd32(d + 28);
            *file = f;
            return 0;
        }
        uint32_t next;
        if (fat_entry(io, v, current, &next) != 0) return -1;
        if (next >= UINT32_C(0x0ffffff8)) return 1;
        if (!cluster_valid(v, next)) return -1;
        current = next;
    }
    return -1; /* cyclic / implausibly long root directory chain */
}
static int validate_chain(const IfsFat32Io *io, const IfsFat32Volume *v,
                          const IfsFat32File *f, uint32_t *fragments)
{
    if (!io || !v || !f || !fragments || (f->attributes & 0x10u)) return -1;
    uint64_t count = ((uint64_t)f->size_bytes + v->cluster_bytes - 1u) /
                      v->cluster_bytes;
    if (count == 0u) {
        if (f->first_cluster != 0u) return -1;
        *fragments = 0;
        return 0;
    }
    if (count > v->cluster_count || !cluster_valid(v, f->first_cluster))
        return -1;
    uint32_t cluster = f->first_cluster, runs = 1u;
    for (uint64_t i = 0; i < count; ++i) {
        uint32_t next;
        if (fat_entry(io, v, cluster, &next) != 0) return -1;
        if (i + 1u == count) {
            if (next < UINT32_C(0x0ffffff8)) return -1;
            *fragments = runs;
            return 0;
        }
        if (!cluster_valid(v, next)) return -1;
        if (next != cluster + 1u) ++runs;
        cluster = next;
    }
    return -1;
}
static int read_file(const IfsFat32Io *io, const IfsFat32Volume *v,
                     const IfsFat32File *f, uint64_t position,
                     void *buffer, uint32_t length)
{
    uint32_t fragments;
    if (!buffer || validate_chain(io, v, f, &fragments) != 0 ||
        position > f->size_bytes || length > (uint64_t)f->size_bytes - position)
        return -1;
    if (!length) return 0;
    uint32_t cluster = f->first_cluster;
    uint64_t skip = position / v->cluster_bytes;
    for (uint64_t i = 0; i < skip; ++i) {
        uint32_t next;
        if (next_data_cluster(io, v, cluster, &next) != 0) return -1;
        cluster = next;
    }
    uint32_t within = (uint32_t)(position % v->cluster_bytes);
    uint8_t *p = (uint8_t *)buffer;
    while (length) {
        uint32_t take = v->cluster_bytes - within;
        if (take > length) take = length;
        if (cluster_read(io, v, cluster, within, p, take) != 0) return -1;
        length -= take;
        p += take;
        within = 0u;
        if (length) {
            uint32_t next;
            if (next_data_cluster(io, v, cluster, &next) != 0) return -1;
            cluster = next;
        }
    }
    return 0;
}
/* Non-executed format identity for safe single-file selection. */
__attribute__((used,section(".ifsmeta")))
static const char ifs_module_format_identity[] = "IFS-MODULE:fat32:1";
static const IfsFat32Module module = {
    IFS_FAT32_ABI_VERSION, sizeof(IfsFat32Module), "fat32", "FAT32",
    IFS_FAT32_CAP_PROBE | IFS_FAT32_CAP_ROOT_83 |
    IFS_FAT32_CAP_FILE_READ | IFS_FAT32_CAP_ROOT_FRAGMENT_COUNT,
    probe, root_file, read_file, validate_chain
};
__attribute__((visibility("default")))
const IfsFat32Module *infiltrator_filesystem_module_v1(void)
{
    return &module;
}
