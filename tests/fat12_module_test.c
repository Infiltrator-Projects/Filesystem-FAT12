/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent in-memory FAT12 fixture. No host filesystem or block devices. */
#include "../src/infiltratr-common/include/infiltratr/filesystem_module.h"
#include "../src/infiltratr-common/include/infiltratr/filesystem_module_v2.h"
extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
extern const IfsFsModuleApiV1 *infiltrator_filesystem_api_v1(void);
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SECTOR 512u
#define SECTORS 2880u
#define FAT_SECTORS 9u
#define ROOT_SECTORS 14u
#define DATA_START (1u + 2u * FAT_SECTORS + ROOT_SECTORS)
#define CLUSTER_SECTORS 1u
static unsigned char image[SECTORS * SECTOR];

static void p16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}
static void p32(unsigned char *p, uint32_t v)
{
    p16(p, (unsigned)(v & 0xffffu));
    p16(p + 2, (unsigned)(v >> 16));
}
static int read_at(void *ctx, uint64_t offset, void *buf, uint32_t bytes)
{
    (void)ctx;
    if (offset > sizeof image || bytes > sizeof image - offset) return -1;
    memcpy(buf, image + (size_t)offset, bytes);
    return 0;
}
static unsigned common_write_count;
static int common_write_at(void *ctx, uint64_t offset, const void *buf, uint32_t bytes)
{
    (void)ctx;
    if (offset > sizeof image || bytes > sizeof image - offset) return -1;
    memcpy(image + (size_t)offset, buf, bytes);
    ++common_write_count;
    return 0;
}
static int common_flush_at(void *ctx) { (void)ctx; return 0; }
/* Encode exact packed 12-bit entries, covering both even and odd clusters. */
static void fat(unsigned cluster, unsigned next)
{
    for (unsigned copy = 0; copy < 2u; ++copy) {
        unsigned char *table = image + (1u + copy * FAT_SECTORS) * SECTOR;
        const unsigned off = cluster + cluster / 2u;
        if ((cluster & 1u) == 0u) {
            table[off] = (unsigned char)next;
            table[off+1u] = (unsigned char)((table[off+1u] & 0xf0u) |
                                            ((next >> 8) & 0x0fu));
        } else {
            table[off] = (unsigned char)((table[off] & 0x0fu) |
                                          ((next << 4) & 0xf0u));
            table[off+1u] = (unsigned char)(next >> 4);
        }
    }
}
static void fixture(void)
{
    memset(image, 0, sizeof image);
    unsigned char *b = image;
    b[0] = 0xebu;
    b[2] = 0x90u;
    p16(b + 11, SECTOR);
    b[13] = CLUSTER_SECTORS;
    p16(b + 14, 1);
    b[16] = 2;
    p16(b + 17, ROOT_SECTORS * 16u);
    p16(b + 19, SECTORS);
    b[21] = 0xf0u;
    p16(b + 22, FAT_SECTORS);
    memcpy(b + 54, "FAT16   ", 8); /* BPB label cannot override FAT12 cluster-count identity. */
    b[510] = 0x55u;
    b[511] = 0xaau;
    fat(0, 0x0ff0u);
    fat(1, 0x0fffu);
    fat(2, 5u);
    fat(5, 3u);
    fat(3, 0x0fffu);

    unsigned char *dir = image + (1u + 2u * FAT_SECTORS) * SECTOR;
    memcpy(dir, "HELLO   TXT", 11);
    dir[11] = 0x20u;
    p16(dir + 26, 2u);
    p32(dir + 28, 1200u);
    const unsigned clusters[] = {2u,5u,3u};
    for (unsigned i=0; i<3; ++i) {
        memset(image + (DATA_START + (clusters[i]-2u)*CLUSTER_SECTORS)*SECTOR,
               'A'+(int)i, CLUSTER_SECTORS*SECTOR);
    }
}
static int failure(const char *msg)
{
    fprintf(stderr, "FAT12 test failure: %s\n", msg);
    return 1;
}
int main(void)
{
    /* Common's single format-neutral ABI must be the advertised entry point,
     * not another competing set of per-format capabilities. */
    const IfsFsModuleApiV1 *common = infiltrator_filesystem_api_v1();
    const IfsFsModuleApiV2 *v2 = infiltrator_filesystem_api_v2();
    const IfsFsVolumeOpsV1 *v2_volume =
        (const IfsFsVolumeOpsV1 *)ifs_fs_group_v2(v2, IFS_FS_GROUP_VOLUME);
    if (!ifs_fs_api_v2_valid(v2, "fat12") ||
        v2->base_v1 != common ||
        v2->group_mask != IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME) ||
        !v2_volume || !v2_volume->sync || !v2_volume->check ||
        !v2_volume->repair || !v2_volume->format ||
        (v2->runtime_profiles & IFS_FS_PROFILE_KERNEL) ||
        ifs_fs_group_v2(v2, IFS_FS_GROUP_STREAMS))
        return failure("V2 descriptor or honest capability contract");

    IfsFsHostIoV1 host = {
        sizeof(IfsFsHostIoV1), IFS_FS_IO_READ_ONLY, NULL,
        sizeof image, read_at, common_write_at, common_flush_at, NULL, NULL
    };
    uint32_t variant = 0;
    fixture();
    if (!ifs_fs_common_api_valid(common, "fat12") ||
        common->capabilities != (IFS_FS_CAP_PROBE | IFS_FS_CAP_MOUNT |
            IFS_FS_CAP_LIST | IFS_FS_CAP_READ | IFS_FS_CAP_WRITE |
            IFS_FS_CAP_CREATE | IFS_FS_CAP_REMOVE | IFS_FS_CAP_RENAME |
            IFS_FS_CAP_TRUNCATE | IFS_FS_CAP_FORMAT | IFS_FS_CAP_CHECK |
            IFS_FS_CAP_RECOVER) ||
        !common->probe || !common->format || !common->mount || !common->check ||
        !common->recover || common->probe(&host, &variant) != IFS_FS_OK ||
        variant != 12u)
        return failure("Common ABI probe or capability declaration");
    common_write_count = 0u;
    if (common->format(&host, 12u, NULL) != IFS_FS_ACCESS ||
        common_write_count != 0u)
        return failure("read-only Common formatter was not blocked");
    host.flags = IFS_FS_IO_EXCLUSIVE;
    if (common->format(&host, 16u, NULL) != IFS_FS_UNSUPPORTED ||
        common->format(&host, 12u, "UNSUPPORTED-LABEL") != IFS_FS_UNSUPPORTED ||
        common_write_count != 0u)
        return failure("foreign variant or unsupported label wrote to media");
    if (common->format(&host, 12u, NULL) != IFS_FS_OK ||
        common_write_count == 0u || common->probe(&host, &variant) != IFS_FS_OK ||
        variant != 12u)
        return failure("Common format did not produce readable FAT12");
    host.flags = IFS_FS_IO_READ_ONLY;
    host.write_at = NULL;
    if (common->format(&host, 12u, NULL) != IFS_FS_ACCESS)
        return failure("Common formatter accepted missing exclusive write access");

    /* Common creates and reopens its own on-disk fixture; no V3 entry point. */
    {
        const char payload[]="COMMON-FAT12-PAYLOAD";
        char name[768];
        char bytes[sizeof payload];
        IfsFsEntryV1 entry={sizeof(IfsFsEntryV1),0,0,0,0,name,sizeof name};
        IfsFsVolumeInfoV1 info={0};
        void *mounted=NULL, *second=NULL, *stale=NULL, *seed=NULL;
        IfsFsVolumeInfoV1 seed_info={0};
        unsigned writes;
        info.structure_bytes=sizeof info;
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        seed_info.structure_bytes=sizeof seed_info;
        if (common->mount(&host,&seed,&seed_info)!=IFS_FS_OK ||
            common->create(seed,"/COMMON.TXT",IFS_FS_ENTRY_FILE)!=IFS_FS_OK ||
            common->write(seed,"/COMMON.TXT",0,payload,sizeof payload-1u)!=IFS_FS_OK ||
            common->unmount(seed)!=IFS_FS_OK)
            return failure("Common fixture create/write/unmount");
        host.flags=IFS_FS_IO_READ_ONLY;
        host.write_at=NULL;
        host.flush=NULL;
        writes=common_write_count;
        if (common->mount(&host,&mounted,&info)!=IFS_FS_OK || !mounted ||
            info.variant!=12u || !info.block_bytes ||
            info.free_bytes>info.total_bytes ||
            common->mount(&host,&second,&info)!=IFS_FS_BUSY ||
            common->stat(mounted,"/COMMON.TXT",&entry)!=IFS_FS_OK ||
            entry.type!=IFS_FS_ENTRY_FILE ||
            entry.size_bytes!=sizeof payload-1u ||
            strcmp(entry.name,"COMMON.TXT")!=0 ||
            common->directory_entry(mounted,"/",0,&entry)!=IFS_FS_OK ||
            strcmp(entry.name,"COMMON.TXT")!=0 ||
            common->directory_entry(mounted,"/",1,&entry)!=IFS_FS_END ||
            common->read(mounted,"/COMMON.TXT",0,bytes,sizeof payload-1u)!=IFS_FS_OK ||
            memcmp(bytes,payload,sizeof payload-1u)!=0 ||
            common->write(mounted,"/COMMON.TXT",0,"X",1)!=IFS_FS_ACCESS ||
            common->create(mounted,"/NOGO.TXT",IFS_FS_ENTRY_FILE)!=IFS_FS_ACCESS ||
            writes!=common_write_count ||
            common->stat(mounted,"/../COMMON.TXT",&entry)!=IFS_FS_INVALID ||
            common->read(mounted,"0:/COMMON.TXT",0,bytes,1)!=IFS_FS_INVALID ||
            common->unmount(mounted)!=IFS_FS_OK)
            return failure("Common read-only mount/stat/list/read and rejection");
        stale=mounted; /* The first mount has ended; retain its token. */
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        info.structure_bytes=sizeof info;
        if (common->mount(&host,&mounted,&info)!=IFS_FS_OK ||
            mounted==stale ||
            common->stat(stale,"/COMMON.TXT",&entry)!=IFS_FS_INVALID ||
            common->read(stale,"/COMMON.TXT",0,bytes,1)!=IFS_FS_INVALID ||
            common->create(stale,"/WRONG.TXT",IFS_FS_ENTRY_FILE)!=IFS_FS_INVALID ||
            common->unmount(stale)!=IFS_FS_INVALID ||
            common->read(mounted,"/COMMON.TXT",UINT64_MAX,bytes,1)!=IFS_FS_UNSUPPORTED ||
            common->create(mounted,"/DIR",IFS_FS_ENTRY_DIRECTORY)!=IFS_FS_OK ||
            common->create(mounted,"/DIR/FILE.TXT",IFS_FS_ENTRY_FILE)!=IFS_FS_OK ||
            common->write(mounted,"/DIR/FILE.TXT",0,payload,sizeof payload-1u)!=IFS_FS_OK ||
            common->rename(mounted,"/DIR/FILE.TXT","/DIR/RENAMED.TXT")!=IFS_FS_OK ||
            common->truncate(mounted,"/DIR/RENAMED.TXT",6)!=IFS_FS_OK ||
            common->stat(mounted,"/DIR/RENAMED.TXT",&entry)!=IFS_FS_OK ||
            entry.size_bytes!=6u ||
            common->remove(mounted,"/DIR/RENAMED.TXT")!=IFS_FS_OK ||
            common->remove(mounted,"/DIR")!=IFS_FS_OK ||
            common->unmount(mounted)!=IFS_FS_OK)
            return failure("Common exclusive create/write/rename/truncate/delete");
        if (common->unmount(mounted)!=IFS_FS_INVALID)
            return failure("Common stale/unmounted handle accepted");
    }


    /* Compare allocation references, mirrored FATs, and orphan detection
     * using only read callbacks. Corrupt fixtures must never be modified. */
    {
        void *session=NULL;
        uint32_t errors=UINT32_MAX,before=common_write_count;
        IfsFsVolumeInfoV1 info={0};
        info.structure_bytes=sizeof info;
        fixture();
        host.flags=IFS_FS_IO_READ_ONLY;
        host.write_at=NULL;host.flush=NULL;
        if(common->mount(&host,&session,&info)!=IFS_FS_OK ||
           !common->check || !common->recover ||
           common->check(session,&errors)!=IFS_FS_OK || errors!=0u ||
           common_write_count!=before ||
           common->unmount(session)!=IFS_FS_OK)
            return failure("read-only full FAT12 allocation check");
        fat(9u,0xfffu);
        info.structure_bytes=sizeof info;
        if(common->mount(&host,&session,&info)!=IFS_FS_OK ||
           common->check(session,&errors)!=IFS_FS_OK || errors==0u ||
           common->unmount(session)!=IFS_FS_OK ||
           common_write_count!=before)
            return failure("orphaned cluster was not detected");
        fixture();
        image[(1u+FAT_SECTORS)*SECTOR+12u]^=1u;
        info.structure_bytes=sizeof info;
        if(common->mount(&host,&session,&info)!=IFS_FS_OK ||
           common->check(session,&errors)!=IFS_FS_OK || errors==0u ||
           common->unmount(session)!=IFS_FS_OK ||
           common_write_count!=before)
            return failure("mismatched FAT mirrors were not detected");
    }


    /* Blank target format must be expressly told its logical sector size.
     * Verify that invalid geometry and unprivileged writes remain no-ops,
     * then prove formatted media can be recognised and mounted. */
    {
        IfsFsHostFormatIoV1 formatted={0};
        uint32_t detected=0;
        void *mounted=NULL;
        IfsFsVolumeInfoV1 info={0};
        unsigned before;
        formatted.host=host;
        formatted.host.structure_bytes=sizeof formatted;
        formatted.host.flags=IFS_FS_IO_EXCLUSIVE;
        formatted.host.write_at=common_write_at;
        formatted.host.flush=common_flush_at;
        formatted.logical_sector_bytes=256u;
        memset(image,0,sizeof image);
        before=common_write_count;
        if (common->format(&formatted.host,12u,NULL)!=IFS_FS_INVALID ||
            common_write_count!=before)
            return failure("blank format accepted invalid logical sector width");
        formatted.logical_sector_bytes=512u;
        formatted.host.flags=IFS_FS_IO_READ_ONLY;
        if (common->format(&formatted.host,12u,NULL)!=IFS_FS_ACCESS ||
            common_write_count!=before)
            return failure("blank format accepted read-only host");
        formatted.host.flags=IFS_FS_IO_EXCLUSIVE;
        if (common->probe(&formatted.host,&detected)!=IFS_FS_NOT_FOUND ||
            common->format(&formatted.host,12u,NULL)!=IFS_FS_OK ||
            common_write_count==before ||
            common->probe(&formatted.host,&detected)!=IFS_FS_OK ||
            detected!=12u)
            return failure("explicit-sector blank FAT12 formatter");
        formatted.host.flags=IFS_FS_IO_READ_ONLY;
        formatted.host.write_at=NULL;
        info.structure_bytes=sizeof info;
        if (common->mount(&formatted.host,&mounted,&info)!=IFS_FS_OK ||
            !mounted || info.variant!=12u ||
            common->unmount(mounted)!=IFS_FS_OK)
            return failure("blank FAT12 formatted media could not mount");
    }


    /* Never try to repair a chain with orphaned data. Valid primary FAT
     * must survive interrupted mirror writes; read-only hosts cannot repair. */
    {
        void *session=NULL;
        uint32_t errors=0,changes=0,before;
        IfsFsVolumeInfoV1 info={0};
        fixture();
        fat(9u,0xfffu); /* orphan present in both FATs */
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        info.structure_bytes=sizeof info;
        before=common_write_count;
        if(common->mount(&host,&session,&info)!=IFS_FS_OK ||
           common->recover(session,&changes)!=IFS_FS_CORRUPT ||
           common_write_count!=before ||
           common->unmount(session)!=IFS_FS_OK)
            return failure("unsafe orphan recovery was not rejected");
        fixture();
        image[(1u+FAT_SECTORS)*SECTOR+12u]^=1u;
        host.flags=IFS_FS_IO_READ_ONLY;
        host.write_at=NULL;host.flush=NULL;
        info.structure_bytes=sizeof info;
        before=common_write_count;
        if(common->mount(&host,&session,&info)!=IFS_FS_OK ||
           common->recover(session,&changes)!=IFS_FS_ACCESS ||
           common_write_count!=before ||
           common->unmount(session)!=IFS_FS_OK)
            return failure("read-only FAT mirror repair was not blocked");
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        info.structure_bytes=sizeof info;
        if(common->mount(&host,&session,&info)!=IFS_FS_OK ||
           common->recover(session,&changes)!=IFS_FS_OK || changes==0u ||
           common_write_count==before ||
           common->check(session,&errors)!=IFS_FS_OK || errors!=0u ||
           common->unmount(session)!=IFS_FS_OK)
            return failure("safe FAT12 secondary mirror repair failed");
    }


    /* Use the V2 volume group on real disposable FAT12 media. */
    {
        void *volume=NULL;
        uint64_t errors=UINT64_MAX,repairs=UINT64_MAX;
        unsigned before=common_write_count;
        IfsFsVolumeInfoV1 info={0};
        info.structure_bytes=sizeof info;
        fixture();
        host.flags=IFS_FS_IO_READ_ONLY;
        host.write_at=NULL;host.flush=NULL;
        if (common->mount(&host,&volume,&info)!=IFS_FS_OK ||
            v2_volume->check(volume,0u,&errors)!=IFS_FS_OK ||
            errors!=0u ||
            v2_volume->sync(volume)!=IFS_FS_OK ||
            v2_volume->repair(volume,0u,&repairs)!=IFS_FS_ACCESS ||
            v2_volume->check(volume,1u,&errors)!=IFS_FS_UNSUPPORTED ||
            common_write_count!=before ||
            common->unmount(volume)!=IFS_FS_OK)
            return failure("V2 read-only fsck/sync/repair safety");
    }
    puts("FAT12-COMMON-API-CHECK-AND-RECOVER-PASS");
    return 0;
}
