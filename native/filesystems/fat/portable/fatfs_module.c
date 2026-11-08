/* SPDX-License-Identifier: GPL-3.0-or-later
 * Adapter for source-owned ChaN FatFs R0.16 patch2. One active image per
 * standalone module: caller must ensure exclusive media ownership and never
 * concurrently invoke this ABI. A .so is not a Linux VFS mount or EFI driver.
 * V4 separately exposes explicitly requested partition formatting.
 */
#include "fatfs_module.h"
#include "ff.h"
#include "diskio.h"
#include "fat12_module.h"
#include "infiltratr/filesystem_module.h"

#if IFS_FATFS_ONLY != 12
#error "The Common filesystem ABI pilot is owned by FAT12 only"
#endif

#if defined(IFS_FATFS_ONLY) && IFS_FATFS_ONLY == 12
#define REQUIRED_FS FS_FAT12
#define FS_ID "fat12"
#elif defined(IFS_FATFS_ONLY) && IFS_FATFS_ONLY == 16
#define REQUIRED_FS FS_FAT16
#define FS_ID "fat16"
#else
#error "Compile one FAT12 or FAT16 standalone module only"
#endif

static IfsFatfsIo device;
static FATFS volume;
static int selected;
static int bound;
static int format_preview;
static int preview_boot_seen;
static uint8_t preview_boot[512];
static int sector_valid(void)
{
    return device.read_at && device.write_at && device.flush &&
           device.fat_timestamp &&
           device.sector_bytes >= 512u && device.sector_bytes <= 4096u &&
           (device.sector_bytes & (device.sector_bytes - 1u)) == 0u &&
           device.partition_bytes >= 16u * device.sector_bytes &&
           device.partition_bytes % device.sector_bytes == 0u &&
           device.partition_bytes / device.sector_bytes <= UINT32_MAX;
}
static int io_valid(uint64_t sector, UINT count)
{
    uint64_t n = (uint64_t)count * device.sector_bytes;
    return bound && sector_valid() && count &&
           sector <= device.partition_bytes / device.sector_bytes &&
           n <= device.partition_bytes - sector * device.sector_bytes &&
           n <= UINT32_MAX;
}
DSTATUS disk_initialize(BYTE pdrv)
{
    return pdrv == 0 && bound && sector_valid() ? 0 : STA_NOINIT;
}
DSTATUS disk_status(BYTE pdrv)
{
    return disk_initialize(pdrv);
}
DRESULT disk_read(BYTE pdrv, BYTE *dst, LBA_t sector, UINT count)
{
    if (pdrv || !dst || !io_valid((uint64_t)sector, count))
        return RES_PARERR;
    /* Formatting a partition-relative SFD should never read existing media.
     * Abort the dry-run if a future engine change violates that assumption. */
    if (format_preview) return RES_ERROR;
    return device.read_at(device.user, (uint64_t)sector*device.sector_bytes,
                          dst, count*device.sector_bytes) == 0 ?
           RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE pdrv, const BYTE *src, LBA_t sector, UINT count)
{
    if (pdrv || !src || !io_valid((uint64_t)sector, count))
        return RES_PARERR;
    if (format_preview) {
        if (sector==0) {
            for (unsigned i=0;i<sizeof preview_boot;++i) preview_boot[i]=src[i];
            preview_boot_seen=1;
        }
        return RES_OK;
    }
    return device.write_at(device.user, (uint64_t)sector*device.sector_bytes,
                           src, count*device.sector_bytes) == 0 ?
           RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE command, void *out)
{
    if (pdrv || !bound || !sector_valid()) return RES_PARERR;
    switch (command) {
    case CTRL_SYNC:
        if (format_preview) return RES_OK;
        return device.flush(device.user) == 0 ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT:
        if (!out) return RES_PARERR;
        *(LBA_t *)out = (LBA_t)(device.partition_bytes/device.sector_bytes);
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (!out) return RES_PARERR;
        *(WORD *)out = (WORD)device.sector_bytes;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (!out) return RES_PARERR;
        *(DWORD *)out = 1u;
        return RES_OK;
    default: return RES_PARERR;
    }
}
DWORD get_fattime(void)
{
    return bound && device.fat_timestamp ?
           (DWORD)device.fat_timestamp(device.user) : 0u;
}
static int mounted_bounds_valid(void)
{
    uint8_t bpb[4096];
    uint64_t sectors,limit=device.partition_bytes/device.sector_bytes;
    uint64_t data_end;
    if ((uint64_t)volume.volbase>=limit ||
        volume.fatbase<volume.volbase || volume.dirbase<=volume.fatbase ||
        volume.database<=volume.dirbase ||
        disk_read(0,bpb,volume.volbase,1)!=RES_OK) return 0;
    sectors=(uint64_t)bpb[19] | (uint64_t)bpb[20]<<8;
    if (!sectors)
        sectors=(uint64_t)bpb[32] | (uint64_t)bpb[33]<<8 |
                (uint64_t)bpb[34]<<16 | (uint64_t)bpb[35]<<24;
    if (!sectors || sectors>limit-(uint64_t)volume.volbase) return 0;
    data_end=(uint64_t)volume.database +
             (uint64_t)(volume.n_fatent-2u)*volume.csize;
    return data_end<=(uint64_t)volume.volbase+sectors;
}
static int attach(const IfsFatfsIo *io)
{
    if (bound || selected || !io) return -1;
    device = *io;
    bound = 1;
    if (!sector_valid() || f_mount(&volume, "0:", 1) != FR_OK ||
        volume.fs_type != REQUIRED_FS || volume.n_fatent<3u ||
        !mounted_bounds_valid() ||
#if IFS_FATFS_ONLY == 12
        volume.n_fatent-2u>=4085u) {
#else
        volume.n_fatent-2u>=65525u) {
#endif
        (void)f_mount(0, "0:", 0);
        bound = 0;
        return -1;
    }
    selected = 1;
    return 0;
}
static int detach(void)
{
    if (!selected) return -1;
    if (f_mount(0, "0:", 0) != FR_OK) return -1;
    selected = 0;
    bound = 0;
    return 0;
}
/* The UEFI filesystem bridge supplies partition-relative BlockIo bytes. */
int ifs_fatfs_boot_attach(const IfsFatfsIo *io)
{
    if (attach(io) != 0) return -1;
    if (volume.volbase != 0u) {
        (void)detach();
        return -1;
    }
    return 0;
}
static int create_file(const char *path)
{
    FIL f;
    if (!selected || !path || f_open(&f, path, FA_WRITE | FA_CREATE_NEW) != FR_OK)
        return -1;
    return f_close(&f) == FR_OK ? 0 : -1;
}
static int file_read(const char *path, uint32_t offset, void *buffer, uint32_t n)
{
    FIL f;
    UINT got=0;
    int good;
    if (!selected || !path || (!buffer && n) ||
        f_open(&f, path, FA_READ) != FR_OK) return -1;
    good=(offset <= f_size(&f)) &&
         (n <= f_size(&f)-(FSIZE_t)offset) &&
         f_lseek(&f, (FSIZE_t)offset) == FR_OK &&
         f_read(&f,buffer,n,&got)==FR_OK && got==n;
    if (f_close(&f)!=FR_OK) good=0;
    return good?0:-1;
}
static int file_write(const char *path, uint32_t offset, const void *buffer, uint32_t n)
{
    FIL f;
    UINT written=0;
    int good;
    if (!selected || !path || (!buffer && n) || n>UINT32_MAX-offset ||
        f_open(&f,path,FA_WRITE|FA_OPEN_EXISTING)!=FR_OK) return -1;
    /* FatFs may return FR_OK after clipping a seek when the volume is full.
     * Never write at the clipped position: that would overwrite earlier data
     * while telling the caller that its requested offset was honoured. */
    good=f_lseek(&f,(FSIZE_t)offset)==FR_OK && f_tell(&f)==(FSIZE_t)offset &&
         f_write(&f,buffer,n,&written)==FR_OK && written==n &&
         f_sync(&f)==FR_OK;
    if (f_close(&f)!=FR_OK) good=0;
    return good?0:-1;
}
static int file_truncate(const char *path, uint32_t bytes)
{
    FIL f;
    int good;
    if (!selected || !path ||
        f_open(&f,path,FA_WRITE|FA_OPEN_EXISTING)!=FR_OK) return -1;
    /* As with writes, a clipped seek is not a successful resize. */
    good=f_lseek(&f,(FSIZE_t)bytes)==FR_OK && f_tell(&f)==(FSIZE_t)bytes &&
         f_truncate(&f)==FR_OK && f_sync(&f)==FR_OK;
    if (f_close(&f)!=FR_OK) good=0;
    return good?0:-1;
}
static int make_dir(const char *path) { return selected && path && f_mkdir(path)==FR_OK?0:-1; }
static int remove_path(const char *path) { return selected && path && f_unlink(path)==FR_OK?0:-1; }
/* FatFs does not reject moving a FAT directory under its own descendants.
 * Follow the destination parent's on-media '..' chain by cluster identity;
 * comparing path strings would miss short-name aliases and case differences.
 * Refuse malformed chains and excessive path lengths rather than mutating. */
static int rename_directory_safe(const char *from,const char *to)
{
    FILINFO source_info;
    DIR source_dir,parent_dir;
    DWORD source_cluster,parent_cluster;
    char parent[4096];
    uint8_t sector[4096];
    unsigned length=0,last_separator=0,has_separator=0;
    if (f_stat(from,&source_info)!=FR_OK) return 0;
    if (!(source_info.fattrib & AM_DIR)) return 1;
    if (f_opendir(&source_dir,from)!=FR_OK) return 0;
    source_cluster=source_dir.obj.sclust;
    if (f_closedir(&source_dir)!=FR_OK || source_cluster<2u) return 0;
    while (to[length]) {
        if (length+1u>=sizeof parent) return 0;
        if (to[length]=='/' || to[length]=='\\') {
            last_separator=length;
            has_separator=1;
        }
        ++length;
    }
    if (has_separator) {
        unsigned count=last_separator+1u;
        for(unsigned i=0;i<count;++i) parent[i]=to[i];
        parent[count]='\0';
    } else {
        /* A path without a separator is relative to volume zero's root. */
        parent[0]='/'; parent[1]='\0';
    }
    if (f_opendir(&parent_dir,parent)!=FR_OK) return 0;
    parent_cluster=parent_dir.obj.sclust;
    if (f_closedir(&parent_dir)!=FR_OK) return 0;
    for(DWORD steps=0;parent_cluster!=0u && steps<volume.n_fatent;++steps) {
        LBA_t first_sector;
        if(parent_cluster==source_cluster || parent_cluster<2u ||
           parent_cluster>=volume.n_fatent) return 0;
        first_sector=volume.database+(LBA_t)(parent_cluster-2u)*volume.csize;
        if(disk_read(0,sector,first_sector,1)!=RES_OK ||
           sector[0]!='.' || sector[1]!=' ' || !(sector[11]&AM_DIR) ||
           sector[32]!='.' || sector[33]!='.' || sector[34]!=' ' ||
           !(sector[43]&AM_DIR) || sector[20] || sector[21] ||
           sector[52] || sector[53] ||
           ((DWORD)sector[26] | (DWORD)sector[27]<<8)!=parent_cluster)
            return 0;
        parent_cluster=(DWORD)sector[58] | (DWORD)sector[59]<<8;
    }
    return parent_cluster==0u;
}
static int rename_path(const char *from,const char *to) {
    return selected && from && to && rename_directory_safe(from,to) &&
           f_rename(from,to)==FR_OK?0:-1;
}
static void copy_name(char out[768],const char *in)
{
    unsigned i=0;
    while (i<767u && in[i]) {out[i]=in[i];++i;}
    out[i]='\0';
}
static int stat_path(const char *path, IfsFatfsEntry *out)
{
    FILINFO i;
    if (!selected || !path || !out || f_stat(path,&i)!=FR_OK) return -1;
    copy_name(out->name,i.fname);
    out->bytes=(uint32_t)i.fsize;
    out->attributes=i.fattrib;
    return 0;
}
static int nth_dir_entry(const char *path,uint32_t nth,IfsFatfsEntry *out)
{
    DIR d;
    FILINFO i;
    int ok=0;
    if(!selected || !path || !out || f_opendir(&d,path)!=FR_OK) return -1;
    for(uint32_t n=0;n<=nth;++n) {
        if(f_readdir(&d,&i)!=FR_OK) goto done;
        if(i.fname[0]=='\0') {ok=1;goto done;}
    }
    copy_name(out->name,i.fname);
    out->bytes=(uint32_t)i.fsize;
    out->attributes=i.fattrib;
    ok=2;
done:
    if(f_closedir(&d)!=FR_OK) return -1;
    return ok==2?0:ok==1?1:-1;
}
__attribute__((used,section(".ifsmeta")))
static const char identity[]="IFS-MODULE:" FS_ID ":3";
static const IfsFatfsModuleV3 module={
    IFS_FATFS_MODULE_ABI,sizeof(IfsFatfsModuleV3),FS_ID,
    IFS_FATFS_CAP_READ | IFS_FATFS_CAP_WRITE_EXISTING |
    IFS_FATFS_CAP_CREATE | IFS_FATFS_CAP_DELETE | IFS_FATFS_CAP_RENAME |
    IFS_FATFS_CAP_DIRECTORY | IFS_FATFS_CAP_TRUNCATE,
    attach,detach,create_file,file_read,file_write,file_truncate,
    make_dir,remove_path,rename_path,stat_path,nth_dir_entry
};
__attribute__((visibility("default")))
const IfsFatfsModuleV3 *infiltrator_filesystem_module_v3(void) {return &module;}

static uint16_t boot16(unsigned offset)
{
    return (uint16_t)((uint16_t)preview_boot[offset] |
                      (uint16_t)preview_boot[offset+1u]<<8);
}
static uint32_t boot32(unsigned offset)
{
    return (uint32_t)boot16(offset) | (uint32_t)boot16(offset+2u)<<16;
}
static int preview_matches_format(void)
{
    uint64_t sectors=boot16(19u), root_sectors, overhead, clusters;
    unsigned bytes=boot16(11u), sectors_per_cluster=preview_boot[13u];
    unsigned reserved=boot16(14u), fats=preview_boot[16u];
    unsigned roots=boot16(17u), fat_sectors=boot16(22u);
    if (!preview_boot_seen || preview_boot[510]!=0x55u || preview_boot[511]!=0xaau ||
        bytes!=device.sector_bytes || !sectors_per_cluster ||
        (sectors_per_cluster & (sectors_per_cluster-1u)) ||
        !reserved || !fats || !roots || !fat_sectors) return 0;
    if (!sectors) sectors=boot32(32u);
    root_sectors=((uint64_t)roots*32u+bytes-1u)/bytes;
    overhead=(uint64_t)reserved+(uint64_t)fats*fat_sectors+root_sectors;
    if (sectors<=overhead || sectors>device.partition_bytes/bytes) return 0;
    clusters=(sectors-overhead)/sectors_per_cluster;
#if IFS_FATFS_ONLY == 12
    return clusters>0u && clusters<4085u;
#else
    /* FatFs classifies exactly 4085 clusters as FAT12, while Linux and
     * EDK2 classify it as FAT16. Reject that ambiguous boundary BEFORE any
     * format write instead of producing media our own driver cannot mount. */
    return clusters>4085u && clusters<65525u;
#endif
}
static int format_volume(const IfsFatfsIo *io,uint32_t cluster_bytes)
{
    uint8_t work[4096];
    MKFS_PARM options={FM_FAT|FM_SFD,2u,1u,512u,0u};
    int good=0;
    if (bound || selected || !io) return -1;
    device=*io;
    bound=1;
    if (!sector_valid()) goto done;
    if (cluster_bytes && (cluster_bytes<device.sector_bytes ||
        cluster_bytes>(uint64_t)device.sector_bytes*128u ||
        (cluster_bytes & (cluster_bytes-1u)))) goto done;
    options.au_size=cluster_bytes;
    preview_boot_seen=0;
    format_preview=1;
    if (f_mkfs("0:",&options,work,sizeof work)!=FR_OK || !preview_matches_format())
        goto done;
    format_preview=0;
    if (f_mkfs("0:",&options,work,sizeof work)!=FR_OK) goto done;
    /* Confirm that the completed on-media result is the named variant. */
    if (f_mount(&volume,"0:",1)!=FR_OK || volume.fs_type!=REQUIRED_FS ||
        volume.n_fatent<3u || !mounted_bounds_valid())
        goto done;
    good=1;
done:
    format_preview=0;
    (void)f_mount(0,"0:",0);
    bound=0;
    return good?0:-1;
}
static int set_attributes(const char *path,uint8_t attributes,uint8_t mask)
{
    const uint8_t valid=AM_RDO|AM_HID|AM_SYS|AM_ARC;
    return selected && path && mask && !(mask & (uint8_t)~valid) &&
           !(attributes & (uint8_t)~valid) &&
           f_chmod(path,attributes,mask)==FR_OK?0:-1;
}
static int set_modified_time(const char *path,uint16_t date,uint16_t time)
{
    FILINFO info;
    unsigned year=1980u+(date>>9),month=(date>>5)&15u,day=date&31u;
    unsigned hour=time>>11,minute=(time>>5)&63u,second=time&31u;
    static const uint8_t days[]={0u,31u,28u,31u,30u,31u,30u,31u,31u,30u,31u,30u,31u};
    unsigned max_day;
    if (!selected || !path || month<1u || month>12u || !day ||
        hour>23u || minute>59u || second>29u) return -1;
    max_day=days[month];
    if (month==2u && (year%4u==0u && (year%100u!=0u || year%400u==0u))) ++max_day;
    if (day>max_day) return -1;
    info.fdate=date; info.ftime=time;
    return f_utime(path,&info)==FR_OK?0:-1;
}
static int volume_space(uint64_t *total,uint64_t *free_bytes)
{
    FATFS *fs=0;
    DWORD free_clusters=0;
    if (!selected || !total || !free_bytes ||
        f_getfree("0:",&free_clusters,&fs)!=FR_OK || fs!=&volume ||
        volume.n_fatent<2u || free_clusters>volume.n_fatent-2u) return -1;
    *total=(uint64_t)(volume.n_fatent-2u)*volume.csize*device.sector_bytes;
    *free_bytes=(uint64_t)free_clusters*volume.csize*device.sector_bytes;
    return 0;
}
static int get_modified_time(const char *path,uint16_t *date,uint16_t *time)
{
    FILINFO info;
    if (!selected || !path || !date || !time || f_stat(path,&info)!=FR_OK)
        return -1;
    *date=info.fdate;
    *time=info.ftime;
    return 0;
}
__attribute__((used,section(".ifsmeta")))
static const char identity_v4[]="IFS-MODULE:" FS_ID ":4";
static const IfsFatfsModuleV4 module_v4={
    IFS_FATFS_MODULE_ABI_V4,sizeof(IfsFatfsModuleV4),FS_ID,
    IFS_FATFS_CAP_FORMAT | IFS_FATFS_CAP_ATTRIBUTES |
    IFS_FATFS_CAP_MODIFIED_TIME | IFS_FATFS_CAP_SPACE,
    &module,format_volume,set_attributes,set_modified_time,get_modified_time,volume_space
};
__attribute__((visibility("default")))
const IfsFatfsModuleV4 *infiltrator_filesystem_module_v4(void) {return &module_v4;}


/* Common/Libraries is the format-neutral ABI authority. This first FAT12
 * adapter supports read-only detection and EXCLUSIVE REFORMAT of a proven
 * existing FAT12 volume. Full mounted-file operations and checking remain
 * pending; do not advertise them merely because the legacy APIs exist.
 * No host libc or platform-specific filesystem code is imported. */
static int common_probe(const IfsFsHostIoV1 *host,
                        uint32_t *detected_variant)
{
    IfsClassicFatIo input;
    IfsClassicFatVolume geometry;
    if (!host || host->structure_bytes < sizeof(*host) ||
        !host->read_at || !detected_variant || host->volume_bytes < 512u ||
        (host->flags & ~(IFS_FS_IO_READ_ONLY | IFS_FS_IO_EXCLUSIVE)))
        return IFS_FS_INVALID;
    input.read_at = host->read_at;
    input.context = host->context;
    input.device_bytes = host->volume_bytes;
    if (infiltrator_filesystem_module_v1()->probe(&input, &geometry) != 0)
        return IFS_FS_NOT_FOUND;
    *detected_variant = 12u;
    return IFS_FS_OK;
}
static int common_read(void *context, uint64_t offset,
                       void *destination, uint32_t bytes)
{
    const IfsFsHostIoV1 *host = (const IfsFsHostIoV1 *)context;
    return host->read_at(host->context, offset, destination, bytes);
}
static int common_write(void *context, uint64_t offset,
                        const void *source, uint32_t bytes)
{
    const IfsFsHostIoV1 *host = (const IfsFsHostIoV1 *)context;
    return host->write_at(host->context, offset, source, bytes);
}
static int common_flush(void *context)
{
    const IfsFsHostIoV1 *host = (const IfsFsHostIoV1 *)context;
    return host->flush(host->context);
}
static uint32_t common_timestamp(void *context)
{
    (void)context;
    /* Common V1 currently has no timestamp provider; use valid FAT epoch. */
    return (1u << 21) | (1u << 16);
}
static int common_format(const IfsFsHostIoV1 *host, uint32_t variant,
                         const char *volume_label)
{
    IfsClassicFatIo input;
    IfsClassicFatVolume geometry;
    IfsFatfsIo io;
    if (!host || host->structure_bytes < sizeof(*host) ||
        !host->read_at || host->volume_bytes < 512u ||
        (host->flags & ~(IFS_FS_IO_READ_ONLY | IFS_FS_IO_EXCLUSIVE)))
        return IFS_FS_INVALID;
    if ((host->flags & IFS_FS_IO_READ_ONLY) ||
        !(host->flags & IFS_FS_IO_EXCLUSIVE) ||
        !host->write_at || !host->flush)
        return IFS_FS_ACCESS;
    if (variant != 12u || (volume_label && volume_label[0]))
        return IFS_FS_UNSUPPORTED;
    /* Common V1 does not provide physical/logical sector size for an empty
     * target. Reformat only a previously validated FAT12 volume: NEVER guess
     * blank-device geometry, and never access an actively mounted device. */
    input.read_at = host->read_at;
    input.context = host->context;
    input.device_bytes = host->volume_bytes;
    if (infiltrator_filesystem_module_v1()->probe(&input, &geometry) != 0)
        return IFS_FS_NOT_FOUND;
    io.user = (void *)host;
    io.partition_bytes = host->volume_bytes;
    io.sector_bytes = geometry.bytes_per_sector;
    io.read_at = common_read;
    io.write_at = common_write;
    io.flush = common_flush;
    io.fat_timestamp = common_timestamp;
    return infiltrator_filesystem_module_v4()->format(&io, 0u) == 0 ?
           IFS_FS_OK : IFS_FS_IO_ERROR;
}
__attribute__((used,section(".ifsmeta")))
static const char common_identity[] = "IFS-MODULE:fat12:common1";
static const IfsFsModuleApiV1 common_api = {
    IFS_FS_COMMON_ABI_V1, sizeof(IfsFsModuleApiV1), "fat12",
    IFS_FS_CAP_PROBE | IFS_FS_CAP_FORMAT,
    common_probe,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    common_format,
    0, 0
};
__attribute__((visibility("default")))
const IfsFsModuleApiV1 *infiltrator_filesystem_api_v1(void)
{
    return &common_api;
}
