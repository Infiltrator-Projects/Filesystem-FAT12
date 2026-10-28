/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef IFS_FAT_VARIANT_GUARD_H
#define IFS_FAT_VARIANT_GUARD_H

static inline int ifs_fat_is_dos1_bpb(const unsigned char *b)
{
    unsigned int i;
    if (b[0] != 0xebu || b[2] != 0x90u)
        return 0;
    /* Match the Linux core's DOS 1.x check of the absent BPB fields. */
    for (i = 11u; i <= 27u; ++i)
        if (b[i] != 0u)
            return 0;
    return 1;
}

/* Width from the BPB, before the Linux FAT core touches writable media.
 * Return 0 for invalid/unknown geometry, otherwise 12, 16 or 32. */
static inline unsigned int ifs_fat_bpb_width(const unsigned char *b)
{
    unsigned int sector_size = b[11] | ((unsigned int)b[12] << 8);
    unsigned int cluster_sectors = b[13];
    unsigned int reserved = b[14] | ((unsigned int)b[15] << 8);
    unsigned int fats = b[16];
    unsigned int root_entries = b[17] | ((unsigned int)b[18] << 8);
    unsigned int sectors = b[19] | ((unsigned int)b[20] << 8);
    unsigned int fat_sectors = b[22] | ((unsigned int)b[23] << 8);
    unsigned int overhead, clusters;

    if (sector_size < 512u || sector_size > 4096u ||
        (sector_size & (sector_size - 1u)) || !cluster_sectors ||
        (cluster_sectors & (cluster_sectors - 1u)) || !reserved || !fats)
        return 0;
    if (!sectors)
        sectors = b[32] | ((unsigned int)b[33] << 8) |
                  ((unsigned int)b[34] << 16) | ((unsigned int)b[35] << 24);
    if (!sectors)
        return 0;
    if (!fat_sectors) {
        /* A FAT32 BPB has a zero FAT16 length and a 32-bit FAT length. */
        return (b[36] || b[37] || b[38] || b[39]) ? 32u : 0u;
    }
    if (!root_entries || root_entries % (sector_size / 32u))
        return 0;
    /* All operands are bounded by BPB field widths; this fits in u32. */
    overhead = reserved + fats * fat_sectors + root_entries * 32u / sector_size;
    if (sectors <= overhead)
        return 0;
    clusters = (sectors - overhead) / cluster_sectors;
    if (!clusters || clusters >= 65525u)
        return 0;
    return clusters < 4085u ? 12u : 16u;
}
#endif
