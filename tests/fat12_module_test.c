/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent in-memory FAT12 fixture. No host filesystem or block devices. */
#include "fat12_module.h"
#include "../native/filesystems/fat/portable/fatfs_module.h"
#include "../src/infiltratr-common/include/infiltratr/filesystem_module.h"
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
static uint32_t fat_time(void *ctx) { (void)ctx; return (1u << 21) | (1u << 16); }
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
    const IfsFat12Module *api = infiltrator_filesystem_module_v1();
    IfsFat12Io io = {read_at, NULL, sizeof image};
    IfsFat12Volume vol;
    IfsFat12File file;
    uint32_t fragments;
    unsigned char data[1200];

    if (!api || api->abi_version != 1 || api->structure_bytes != sizeof(*api) ||
        strcmp(api->format_id, "fat12") != 0 ||
        (api->capabilities & IFS_FAT12_CAP_FILE_READ) == 0 ||
        (api->capabilities & IFS_FAT12_CAP_ROOT_FRAGMENT_COUNT) == 0)
        return failure("bad module ABI or capabilities");
    fixture();
    if (api->probe(&io, &vol) != 0 || vol.cluster_count == 0 ||
        vol.cluster_count >= 4085 || vol.cluster_bytes != 512)
        return failure("valid FAT12 image not detected");
    if (api->root_file(&io, &vol, 0, &file) != 0 ||
        strcmp(file.name, "HELLO.TXT") != 0 || file.size_bytes != 1200)
        return failure("root directory missing");
    if (api->root_file(&io, &vol, 1, &file) != 1)
        return failure("extra root entry");
    if (api->root_file(&io, &vol, 0, &file) != 0 ||
        api->root_file_fragments(&io, &vol, &file, &fragments) != 0 ||
        fragments != 3u)
        return failure("fragment count wrong");
    if (api->read_file(&io, &vol, &file, 0, data, sizeof data) != 0)
        return failure("read failed");
    for (unsigned i=0; i<1200; ++i)
        if (data[i] != (unsigned char)('A' + (int)(i/512u)))
            return failure("noncontiguous chain was read incorrectly");
    if (api->read_file(&io, &vol, &file, 1199u, data, 2u) == 0)
        return failure("out-of-bounds file read accepted");

    fat(5, 0x0fffu);
    if (api->root_file_fragments(&io, &vol, &file, &fragments) == 0)
        return failure("short/corrupt chain accepted");
    fixture();
    image[13] = 0u; /* invalid sectors per cluster */
    if (api->probe(&io, &vol) == 0)
        return failure("invalid sectors-per-cluster accepted");
    fixture();
    image[SECTOR+2u] = 0; /* corrupt FAT12 reserved packed entries */
    if (api->probe(&io, &vol) == 0)
        return failure("invalid FAT12 reserved entries accepted");
    fixture();
    image[510] = 0;
    if (api->probe(&io, &vol) == 0)
        return failure("broken signature accepted");
    fixture();
    io.device_bytes = 512;
    if (api->probe(&io, &vol) == 0)
        return failure("truncated image accepted");
    /* Common's single format-neutral ABI must be the advertised entry point,
     * not another competing set of per-format capabilities. */
    const IfsFsModuleApiV1 *common = infiltrator_filesystem_api_v1();
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
            IFS_FS_CAP_TRUNCATE | IFS_FS_CAP_FORMAT) ||
        !common->probe || !common->format || !common->mount || common->check ||
        common->recover || common->probe(&host, &variant) != IFS_FS_OK ||
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

    /* Exercise the Common API against bytes written by the existing V3
     * engine, not a separate formatter or duplicated FAT implementation. */
    {
        const IfsFatfsModuleV3 *legacy=infiltrator_filesystem_module_v3();
        IfsFatfsIo disk={NULL,sizeof image,SECTOR,read_at,common_write_at,
                         common_flush_at,fat_time};
        const char payload[]="COMMON-FAT12-PAYLOAD";
        char name[768];
        char bytes[sizeof payload];
        IfsFsEntryV1 entry={sizeof(IfsFsEntryV1),0,0,0,0,name,sizeof name};
        IfsFsVolumeInfoV1 info={0};
        void *mounted=NULL, *second=NULL;
        unsigned writes;
        info.structure_bytes=sizeof info;
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        if (!legacy || legacy->mount(&disk)!=0 ||
            legacy->create("0:/COMMON.TXT")!=0 ||
            legacy->write("0:/COMMON.TXT",0,payload,sizeof payload-1u)!=0 ||
            legacy->unmount()!=0)
            return failure("legacy V3 fixture for Common mount");
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
        host.flags=IFS_FS_IO_EXCLUSIVE;
        host.write_at=common_write_at;
        host.flush=common_flush_at;
        info.structure_bytes=sizeof info;
        if (common->mount(&host,&mounted,&info)!=IFS_FS_OK ||
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

    puts("FAT12-MODULE-READ-AND-FRAGMENT-PASS");
    return 0;
}
