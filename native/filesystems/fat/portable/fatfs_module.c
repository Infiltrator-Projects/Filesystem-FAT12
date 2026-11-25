/* SPDX-License-Identifier: GPL-3.0-or-later
 * Adapter for source-owned ChaN FatFs R0.16 patch2. One active image per
 * standalone module: caller must ensure exclusive media ownership. Common V2
 * rejects overlapping operations with BUSY. A .so is not a Linux VFS mount or EFI driver.
 * Common exposes the one filesystem interface including formatting.
 */
#include "fatfs_module.h"
#include "ff.h"
#include "diskio.h"
#include "classic_fat_module.h"
#include "infiltratr/filesystem_module_v2.h"
#include <string.h>
#include <stdatomic.h>
#include "fat12_journal.h"

/* The native engine has one active image and shared scratch/IO state.
 * Enforce exclusion inside the V2 provider, not merely in its callers.
 * A reentrant host callback must receive BUSY rather than deadlocking
 * or replacing the device while a transfer is in progress. */
static atomic_flag v2_operation_gate=ATOMIC_FLAG_INIT;
static int v2_enter_operation(void)
{
    return !atomic_flag_test_and_set_explicit(&v2_operation_gate,memory_order_acquire);
}
static int v2_leave_operation(int rc)
{
    atomic_flag_clear_explicit(&v2_operation_gate,memory_order_release);
    return rc;
}


/* Source-owned FatFs internal allocation-chain accessor. */
extern DWORD ifs_fatfs_native_next_cluster(FIL *file, DWORD cluster);

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
/* Translate the native engine's result at the provider boundary. FR_DENIED
 * is ambiguous (permissions, nonempty directory, or a full directory);
 * only an observed short write or clipped seek is definitely NO_SPACE. */
static int fat_status(FRESULT result)
{
    switch(result) {
    case FR_OK: return IFS_FS_OK;
    case FR_NO_FILE: case FR_NO_PATH: return IFS_FS_NOT_FOUND;
    case FR_EXIST: return IFS_FS_EXISTS;
    case FR_INVALID_NAME: case FR_INVALID_PARAMETER:
        return IFS_FS_INVALID;
    case FR_DENIED: case FR_WRITE_PROTECTED: return IFS_FS_ACCESS;
    case FR_NOT_ENOUGH_CORE: return IFS_FS_NO_MEMORY;
    case FR_INT_ERR: return IFS_FS_CORRUPT;
    default: return IFS_FS_IO_ERROR;
    }
}
static int create_file(const char *path)
{
    FIL f;
    if (!selected || !path) return IFS_FS_INVALID;
    FRESULT result=f_open(&f,path,FA_WRITE|FA_CREATE_NEW);
    if (result!=FR_OK) return fat_status(result);
    return fat_status(f_close(&f));
}
static int file_read(const char *path,uint32_t offset,void *buffer,uint32_t n)
{
    FIL f;
    UINT got=0;
    if (!selected || !path || (!buffer && n)) return IFS_FS_INVALID;
    FRESULT result=f_open(&f,path,FA_READ);
    if (result!=FR_OK) return fat_status(result);
    int rc=IFS_FS_OK;
    if (offset>f_size(&f) || n>f_size(&f)-(FSIZE_t)offset)
        rc=IFS_FS_INVALID;
    else if ((result=f_lseek(&f,(FSIZE_t)offset))!=FR_OK)
        rc=fat_status(result);
    else if (n) {
        result=f_read(&f,buffer,n,&got);
        rc=result!=FR_OK?fat_status(result):
           (got==n?IFS_FS_OK:IFS_FS_CORRUPT);
    }
    result=f_close(&f);
    if (result!=FR_OK && rc==IFS_FS_OK) rc=fat_status(result);
    return rc;
}
static int file_write(const char *path,uint32_t offset,const void *buffer,uint32_t n)
{
    FIL f;
    UINT written=0;
    if (!selected || !path || (!buffer && n) || n>UINT32_MAX-offset)
        return IFS_FS_INVALID;
    FRESULT result=f_open(&f,path,FA_WRITE|FA_OPEN_EXISTING);
    if (result!=FR_OK) return fat_status(result);
    /* A full volume can clip a successful seek or short-write. Report
     * exhaustion distinctly and never write at a clipped file offset. */
    int rc=fat_status(f_lseek(&f,(FSIZE_t)offset));
    if (rc==IFS_FS_OK && f_tell(&f)!=(FSIZE_t)offset)
        rc=IFS_FS_NO_SPACE;
    if (rc==IFS_FS_OK && n) {
        result=f_write(&f,buffer,n,&written);
        rc=result!=FR_OK?fat_status(result):
           (written==n?IFS_FS_OK:IFS_FS_NO_SPACE);
    }
    if (rc==IFS_FS_OK) rc=fat_status(f_sync(&f));
    result=f_close(&f);
    /* A failed close may mean a partially committed write, even when
     * the original operation stopped for lack of space. */
    if (result!=FR_OK) rc=fat_status(result);
    return rc;
}
static int file_truncate(const char *path,uint32_t bytes)
{
    FIL f;
    if (!selected || !path) return IFS_FS_INVALID;
    FRESULT result=f_open(&f,path,FA_WRITE|FA_OPEN_EXISTING);
    if (result!=FR_OK) return fat_status(result);
    int rc=fat_status(f_lseek(&f,(FSIZE_t)bytes));
    if (rc==IFS_FS_OK && f_tell(&f)!=(FSIZE_t)bytes) rc=IFS_FS_NO_SPACE;
    if (rc==IFS_FS_OK) rc=fat_status(f_truncate(&f));
    if (rc==IFS_FS_OK) rc=fat_status(f_sync(&f));
    result=f_close(&f);
    if (result!=FR_OK) rc=fat_status(result);
    return rc;
}
static int make_dir(const char *path)
{ return selected && path?fat_status(f_mkdir(path)):IFS_FS_INVALID; }
static int remove_path(const char *path)
{ return selected && path?fat_status(f_unlink(path)):IFS_FS_INVALID; }
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
    /* Select the smallest valid allocation unit for the requested FAT
     * width. FatFs's unqualified default chooses FAT16 for large volumes
     * such as a 255 MiB FAT12 test partition. Each candidate is a pure
     * dry run: disk_read rejects all calls and disk_write records only the
     * proposed BPB. No media writes occur until width is independently
     * validated against FAT12's strict cluster-count boundary. */
    int preview_ok=0;
    uint32_t candidate=cluster_bytes;
    for (unsigned attempt=0;attempt<10u;++attempt) {
        options.au_size=candidate;
        preview_boot_seen=0;
        format_preview=1;
        if (f_mkfs("0:",&options,work,sizeof work)==FR_OK &&
            preview_matches_format()) {
            preview_ok=1;
            break;
        }
        if (cluster_bytes) break;
        if (!candidate) candidate=device.sector_bytes;
        else {
            if (candidate>(uint64_t)device.sector_bytes*64u) break;
            candidate*=2u;
        }
    }
    if (!preview_ok) goto done;
    format_preview=0;
    /* Invalidate any old commit records before publishing a new BPB. A
     * failed reformat must never replay transactions from the old volume. */
    memset(work,0,device.sector_bytes);
    if(disk_write(0,work,1u,1u)!=RES_OK ||
       disk_write(0,work,2u,1u)!=RES_OK ||
       disk_ioctl(0,CTRL_SYNC,NULL)!=RES_OK)goto done;
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
/* Common V2 is the only exported FAT12 filesystem interface.
 * The FAT12 integrity scanner validates geometry; one FatFs engine owns
 * all mounts, file operations and formatting. No legacy API tables. */
/* Format probes also issue short byte reads, so every engine request must
 * pass through the bounded sector adapter required by the Common V2 host. */
static int common_read(void *context, uint64_t offset,
                       void *destination, uint32_t bytes);
/* Track host failures separately from malformed or foreign media. */
typedef struct {
    const IfsFsHostRuntimeV2 *host;
    int error;
} IfsFatV2ProbeIo;
static int common_probe_read(void *context, uint64_t at,
                             void *destination, uint32_t bytes)
{
    IfsFatV2ProbeIo *io=(IfsFatV2ProbeIo *)context;
    const int rc=ifs_fs_host_read_bytes_v2(io->host,at,destination,bytes);
    if(rc!=IFS_FS_OK)
        io->error=rc==IFS_FS_INVALID?IFS_FS_CORRUPT:rc;
    return rc==IFS_FS_OK?0:-1;
}
static int common_probe(const IfsFsHostRuntimeV2 *host,
                        uint32_t *detected_variant)
{
    IfsClassicFatIo input;
    IfsClassicFatVolume geometry;
    IfsFatV2ProbeIo probe_io={host,IFS_FS_OK};
    if (!detected_variant)
        return IFS_FS_INVALID;
    *detected_variant = 0u;
    if (!ifs_fs_host_v2_valid(host) || host->volume_bytes < 512u)
        return IFS_FS_INVALID;
    input.read_at = common_probe_read;
    input.context = &probe_io;
    input.device_bytes = host->volume_bytes;
    if (ifs_classic_fat_probe(&input, &geometry) != 0)
        return probe_io.error==IFS_FS_OK?IFS_FS_NOT_FOUND:probe_io.error;
    *detected_variant = 12u;
    return IFS_FS_OK;
}
static int guarded_common_probe(const IfsFsHostRuntimeV2 *host,
                        uint32_t *detected_variant)
{
    if (detected_variant) *detected_variant=0u;
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_probe(host,detected_variant));
}

/* FatFs converts media failures into FRESULT; retain their originating V2
 * error so STAT never disguises a failed firmware read as a missing file. */
static int common_read_failure = IFS_FS_OK;
static int common_read(void *context, uint64_t offset,
                       void *destination, uint32_t bytes)
{
    const IfsFsHostRuntimeV2 *host = (const IfsFsHostRuntimeV2 *)context;
    const int result=ifs_fs_host_read_bytes_v2(host, offset, destination, bytes);
    if (result!=IFS_FS_OK && common_read_failure==IFS_FS_OK)
        common_read_failure=result==IFS_FS_INVALID?IFS_FS_CORRUPT:result;
    return result==IFS_FS_OK?0:-1;
}
static int common_write(void *context, uint64_t offset,
                        const void *source, uint32_t bytes)
{
    const IfsFsHostRuntimeV2 *host = (const IfsFsHostRuntimeV2 *)context;
    return ifs_fs_host_write_bytes_v2(host, offset, source, bytes)==IFS_FS_OK ? 0 : -1;
}
static int common_flush(void *context)
{
    const IfsFsHostRuntimeV2 *host = (const IfsFsHostRuntimeV2 *)context;
    return host->flush(host->context);
}
/* FAT stores civil time without a timezone. Common V2 provides Unix
 * epoch nanoseconds; encode UTC consistently in every runtime profile.
 * Clamp to FAT's representable years and floor to its two-second tick.
 * Older hosts without a clock retain the documented 1980 fallback. */
static uint32_t common_timestamp(void *context)
{
    const IfsFsHostRuntimeV2 *host=context;
    const uint32_t epoch=(1u<<21)|(1u<<16);
    if (!host || !(host->capabilities&IFS_FS_HOST_CLOCK) || !host->realtime_ns)
        return epoch;
    const uint64_t seconds=host->realtime_ns(host->context)/UINT64_C(1000000000);
    if (seconds<UINT64_C(315532800)) return epoch;
    if (seconds>=UINT64_C(4354819200))
        return (127u<<25)|(12u<<21)|(31u<<16)|(23u<<11)|(59u<<5)|29u;
    uint32_t days=(uint32_t)(seconds/86400u);
    uint32_t year=1970u, month=1u, day_seconds=(uint32_t)(seconds%86400u);
    for (;;) {
        const uint32_t leap=(year%4u==0u && (year%100u!=0u || year%400u==0u));
        const uint32_t length=365u+leap;
        if (days<length) break;
        days-=length; ++year;
    }
    static const uint8_t month_days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    for (;;) {
        const uint32_t leap=(year%4u==0u && (year%100u!=0u || year%400u==0u));
        const uint32_t length=month_days[month-1u]+(month==2u?leap:0u);
        if (days<length) break;
        days-=length; ++month;
    }
    return ((year-1980u)<<25)|(month<<21)|((days+1u)<<16)|
           ((day_seconds/3600u)<<11)|(((day_seconds/60u)%60u)<<5)|
           ((day_seconds%60u)/2u);
}
static int common_format(const IfsFsHostRuntimeV2 *host, uint32_t variant,
                         const char *volume_label, uint32_t flags)
{
    IfsFatfsIo io;
    if (!ifs_fs_host_v2_valid(host) || host->volume_bytes < 512u)
        return IFS_FS_INVALID;
    if ((host->capabilities & IFS_FS_HOST_READ_ONLY) ||
        !(host->capabilities & IFS_FS_HOST_EXCLUSIVE) ||
        !host->write_at || !host->flush)
        return IFS_FS_ACCESS;
    if (selected || bound) return IFS_FS_BUSY;
    if (flags) return IFS_FS_UNSUPPORTED;
    if (variant != 12u || (volume_label && volume_label[0]))
        return IFS_FS_UNSUPPORTED;
    /* Verified logical geometry is mandatory; never guess blank-media size. */
    const uint32_t sector_bytes=host->logical_sector_bytes;
    if (sector_bytes!=512u && sector_bytes!=1024u &&
        sector_bytes!=2048u && sector_bytes!=4096u)
        return IFS_FS_INVALID;
    io.user = (void *)host;
    io.partition_bytes = host->volume_bytes;
    io.sector_bytes = sector_bytes;
    io.read_at = common_read;
    io.write_at = common_write;
    io.flush = common_flush;
    io.fat_timestamp = common_timestamp;
    if(format_volume(&io,0u)!=0)return IFS_FS_IO_ERROR;
    IfsClassicFatVolume geometry;
    IfsFatV2ProbeIo probe_io={host,IFS_FS_OK};
    IfsClassicFatIo input={common_probe_read,&probe_io,host->volume_bytes};
    if(ifs_classic_fat_probe(&input,&geometry)!=0)return IFS_FS_CORRUPT;
    return fj_initialize(host,&geometry);
}
static int guarded_common_format(const IfsFsHostRuntimeV2 *host, uint32_t variant,
                         const char *volume_label, uint32_t flags)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_format(host,variant,volume_label,flags));
}

/* A Common session uses the one FatFs engine directly.
 * Its media context is exclusive; refuse concurrent mounts rather than
 * risk redirecting callers between partitions. Host I/O is copied. */
static struct {
    IfsFsHostRuntimeV2 host;
    uintptr_t serial;       /* Never recycle opaque Common mount handles. */
    uintptr_t active_handle;
    int mounted;
    int journal_present,mutation_fault;
    uint64_t mount_recovered;
    void *reloc_plan;
    uintptr_t reloc_token;
} common_session;
static void fat_reloc_discard(void);

static int common_writable(void)
{
    return common_session.mounted && !common_session.mutation_fault &&
           !common_session.reloc_plan &&
           !(common_session.host.capabilities & IFS_FS_HOST_READ_ONLY) &&
           (common_session.host.capabilities & IFS_FS_HOST_EXCLUSIVE) &&
           common_session.host.write_at && common_session.host.flush;
}
static int common_guarded_write(void *context, uint64_t at,
                                const void *data, uint32_t length)
{
    const IfsFsHostRuntimeV2 *host = (const IfsFsHostRuntimeV2 *)context;
    if ((host->capabilities & IFS_FS_HOST_READ_ONLY) ||
        !(host->capabilities & IFS_FS_HOST_EXCLUSIVE) || !host->write_at)
        return -1;
    return common_write(context, at, data, length);
}
static int common_guarded_flush(void *context)
{
    const IfsFsHostRuntimeV2 *host = (const IfsFsHostRuntimeV2 *)context;
    if ((host->capabilities & IFS_FS_HOST_READ_ONLY) ||
        !(host->capabilities & IFS_FS_HOST_EXCLUSIVE))
        return 0;
    return host->flush ? host->flush(host->context) : -1;
}
static int common_session_valid(void *context)
{
    return common_session.mounted && !common_session.mutation_fault &&
           common_session.active_handle != 0u &&
           (uintptr_t)context == common_session.active_handle;
}
/* Enforce volume-relative absolute paths, without traversal or FatFs drive
 * prefixes. Reject malformed and overflowing names before filesystem I/O. */
static int common_path(const char *path, char translated[4096])
{
    unsigned i=0;
    if (!path || path[0]!='/') return IFS_FS_INVALID;
    translated[0]='0'; translated[1]=':';
    while (i<4093u && path[i]) {
        const unsigned char c=(unsigned char)path[i];
        if (c<32u || c==127u || c==':' || c=='\\')
            return IFS_FS_INVALID;
        if (c=='/' && i>0u && path[i-1u]=='/')
            return IFS_FS_INVALID;
        if (c=='.' && (i==1u || path[i-1u]=='/') &&
            (path[i+1u]=='/' || path[i+1u]=='\0' ||
             (path[i+1u]=='.' && (path[i+2u]=='/' ||
                                   path[i+2u]=='\0'))))
            return IFS_FS_INVALID;
        translated[i+2u]=(char)c;
        ++i;
    }
    if (path[i]) return IFS_FS_INVALID;
    translated[i+2u]='\0';
    return IFS_FS_OK;
}
static int common_mount(const IfsFsHostRuntimeV2 *host, void **context,
                        IfsFsVolumeInfoV2 *info)
{
    IfsClassicFatIo input;
    IfsClassicFatVolume geometry;
    IfsFatV2ProbeIo probe_io={host,IFS_FS_OK};
    IfsFatfsIo io;
    uint64_t total=0, available=0;
    if (!ifs_fs_host_v2_valid(host) || !context || !info ||
        info->structure_bytes<sizeof(*info) || host->volume_bytes<512u)
        return IFS_FS_INVALID;
    *context=0;
    if (common_session.mounted || selected || bound ||
        common_session.serial == UINTPTR_MAX) return IFS_FS_BUSY;
    input.read_at=common_probe_read;
    input.context=&probe_io;
    input.device_bytes=host->volume_bytes;
    if (ifs_classic_fat_probe(&input,&geometry)!=0)
        return probe_io.error==IFS_FS_OK?IFS_FS_NOT_FOUND:probe_io.error;
    int journal_present=0;uint64_t recovered=0u;
    int journal_rc=fj_mount(host,&geometry,&journal_present,&recovered);
    if(journal_rc!=IFS_FS_OK)return journal_rc;
    /* Common V2 hosts can omit later optional fields. Never read beyond
     * the caller's declared prefix; retain only bytes this session owns. */
    size_t host_bytes=host->structure_bytes<sizeof(common_session.host) ?
        host->structure_bytes : sizeof(common_session.host);
    memset(&common_session.host,0,sizeof(common_session.host));
    memcpy(&common_session.host,host,host_bytes);
    common_session.host.structure_bytes=(uint32_t)host_bytes;
    io.user=&common_session.host;
    io.partition_bytes=host->volume_bytes;
    io.sector_bytes=geometry.bytes_per_sector;
    io.read_at=common_read;
    io.write_at=common_guarded_write;
    io.flush=common_guarded_flush;
    io.fat_timestamp=common_timestamp;
    common_read_failure=IFS_FS_OK;
    if (attach(&io)!=0)
        return common_read_failure!=IFS_FS_OK ?
            common_read_failure:IFS_FS_CORRUPT;
    if (volume_space(&total,&available)!=0) {
        (void)detach();
        return common_read_failure!=IFS_FS_OK ?
            common_read_failure:IFS_FS_IO_ERROR;
    }
    common_session.active_handle=++common_session.serial;
    common_session.mounted=1;
    common_session.journal_present=journal_present;
    common_session.mount_recovered=recovered;
    common_session.mutation_fault=0;
    info->variant=12u;
    info->max_name_bytes=765u; /* FAT LFN: 255 UTF-16 units, max 3 UTF-8 bytes each. */
    info->block_bytes=geometry.cluster_bytes;
    info->total_bytes=total;
    info->free_bytes=available;
    /* Opaque non-dereferenceable generation token. A stale caller must
     * never gain access to a subsequent volume, even if it remounts in the
     * same shared object. The single-session API remains non-concurrent. */
    *context=(void *)common_session.active_handle;
    return IFS_FS_OK;
}
static int guarded_common_mount(const IfsFsHostRuntimeV2 *host, void **context,
                        IfsFsVolumeInfoV2 *info)
{
    if (context) *context=NULL;
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_mount(host,context,info));
}

static int common_unmount(void *context)
{
    if(!common_session.mounted || !context ||
       (uintptr_t)context!=common_session.active_handle)return IFS_FS_INVALID;
    const int fault=common_session.mutation_fault;
    fat_reloc_discard();
    if(selected && detach()!=0)return IFS_FS_IO_ERROR;
    common_session.mounted=0;common_session.active_handle=0u;
    common_session.mutation_fault=0;common_session.journal_present=0;
    common_session.mount_recovered=0u;
    return fault?IFS_FS_IO_ERROR:IFS_FS_OK;
}
static int guarded_common_unmount(void *context)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_unmount(context));
}

static int common_entry(IfsFsEntryV2 *out, const IfsFatfsEntry *file_info,
                        int root)
{
    const char *source=root?"/":file_info->name;
    uint32_t length=0;
    if (!out || out->structure_bytes<sizeof(*out) || !out->name ||
        !out->name_capacity) return IFS_FS_INVALID;
    out->name_bytes=0u;
    while (source[length]) {
        if (length+1u>=out->name_capacity) return IFS_FS_NO_SPACE;
        ++length;
    }
    for (uint32_t i=0;i<=length;++i) out->name[i]=source[i];
    out->name_bytes=length;
    out->attributes=root?0u:file_info->attributes;
    out->size_bytes=root?0u:file_info->bytes;
    out->type=(root || (file_info->attributes & AM_DIR)) ?
              IFS_FS_ENTRY_DIRECTORY : IFS_FS_ENTRY_FILE;
    return IFS_FS_OK;
}
static int common_stat(void *context, const char *path, IfsFsEntryV2 *out)
{
    char p[4096];
    IfsFatfsEntry entry;
    int rc;
    if (!common_session_valid(context)) return IFS_FS_INVALID;
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    if (p[2]=='/' && !p[3]) return common_entry(out, &entry, 1);
    common_read_failure=IFS_FS_OK;
    if (stat_path(p,&entry)!=0)
        return common_read_failure!=IFS_FS_OK ?
            common_read_failure:IFS_FS_NOT_FOUND;
    return common_entry(out,&entry,0);
}
static int guarded_common_stat(void *context, const char *path, IfsFsEntryV2 *out)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_stat(context,path,out));
}

static int common_directory_entry(void *context, const char *path,
                                  uint32_t index, IfsFsEntryV2 *out)
{
    char p[4096];
    IfsFatfsEntry entry;
    int rc;
    if (!common_session_valid(context)) return IFS_FS_INVALID;
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    common_read_failure=IFS_FS_OK;
    rc=nth_dir_entry(p,index,&entry);
    if (rc==1) return IFS_FS_END;
    if (rc!=0) return common_read_failure!=IFS_FS_OK ?
        common_read_failure:IFS_FS_IO_ERROR;
    return common_entry(out,&entry,0);
}
static int guarded_common_directory_entry(void *context, const char *path,
                                  uint32_t index, IfsFsEntryV2 *out)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_directory_entry(context,path,index,out));
}

static int common_file_read(void *context, const char *path, uint64_t offset,
                            void *destination, uint32_t length)
{
    char p[4096];
    int rc;
    if (!common_session_valid(context) || (!destination && length))
        return IFS_FS_INVALID;
    if (offset>UINT32_MAX || length>UINT32_MAX-offset)
        return IFS_FS_UNSUPPORTED; /* FAT12 on-media file offsets are 32-bit. */
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    common_read_failure=IFS_FS_OK;
    rc=file_read(p,(uint32_t)offset,destination,length);
    return rc!=IFS_FS_OK && common_read_failure!=IFS_FS_OK?
        common_read_failure:rc;
}
static int guarded_common_file_read(void *context, const char *path, uint64_t offset,
                            void *destination, uint32_t length)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_file_read(context,path,offset,destination,length));
}

static int common_file_write(void *context, const char *path, uint64_t offset,
                             const void *source, uint32_t length)
{
    char p[4096];
    int rc;
    if (!common_session_valid(context) || (!source && length))
        return IFS_FS_INVALID;
    if (!common_writable()) return IFS_FS_ACCESS;
    if (offset>UINT32_MAX || length>UINT32_MAX-offset)
        return IFS_FS_UNSUPPORTED;
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    common_read_failure=IFS_FS_OK;
    rc=file_write(p,(uint32_t)offset,source,length);
    return rc!=IFS_FS_OK && common_read_failure!=IFS_FS_OK?
        common_read_failure:rc;
}
static int guarded_common_file_write(void *context, const char *path, uint64_t offset,
                             const void *source, uint32_t length)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_file_write(context,path,offset,source,length));
}

static int common_create(void *context, const char *path, uint32_t type)
{
    char p[4096];
    int rc;
    if (!common_session_valid(context)) return IFS_FS_INVALID;
    if (!common_writable()) return IFS_FS_ACCESS;
    if (type!=IFS_FS_ENTRY_FILE && type!=IFS_FS_ENTRY_DIRECTORY)
        return IFS_FS_UNSUPPORTED;
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    if (!p[3]) return IFS_FS_INVALID; /* Never create the volume root. */
    common_read_failure=IFS_FS_OK;
    rc=type==IFS_FS_ENTRY_FILE?create_file(p):make_dir(p);
    return rc!=IFS_FS_OK && common_read_failure!=IFS_FS_OK?
        common_read_failure:rc;
}
static int guarded_common_create(void *context, const char *path, uint32_t type)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_create(context,path,type));
}

static int common_remove(void *context, const char *path)
{
    char p[4096];
    int rc;
    if (!common_session_valid(context)) return IFS_FS_INVALID;
    if (!common_writable()) return IFS_FS_ACCESS;
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    if (!p[3]) return IFS_FS_INVALID;
    common_read_failure=IFS_FS_OK;
    rc=remove_path(p);
    return rc!=IFS_FS_OK && common_read_failure!=IFS_FS_OK?
        common_read_failure:rc;
}
static int guarded_common_remove(void *context, const char *path)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_remove(context,path));
}

static int common_rename(void *context, const char *old_path,
                         const char *new_path)
{
    char old_p[4096],new_p[4096];
    int rc;
    if (!common_session_valid(context)) return IFS_FS_INVALID;
    if (!common_writable()) return IFS_FS_ACCESS;
    if ((rc=common_path(old_path,old_p))!=IFS_FS_OK) return rc;
    if ((rc=common_path(new_path,new_p))!=IFS_FS_OK) return rc;
    if (!old_p[3] || !new_p[3]) return IFS_FS_INVALID;
    return rename_path(old_p,new_p)==0 ?
           IFS_FS_OK : IFS_FS_IO_ERROR;
}
static int guarded_common_rename(void *context, const char *old_path,
                         const char *new_path)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_rename(context,old_path,new_path));
}

static int common_truncate(void *context, const char *path, uint64_t length)
{
    char p[4096];
    int rc;
    if (!common_session_valid(context)) return IFS_FS_INVALID;
    if (!common_writable()) return IFS_FS_ACCESS;
    if (length>UINT32_MAX) return IFS_FS_UNSUPPORTED;
    if ((rc=common_path(path,p))!=IFS_FS_OK) return rc;
    common_read_failure=IFS_FS_OK;
    rc=file_truncate(p,(uint32_t)length);
    return rc!=IFS_FS_OK && common_read_failure!=IFS_FS_OK?
        common_read_failure:rc;
}
static int guarded_common_truncate(void *context, const char *path, uint64_t length)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_truncate(context,path,length));
}



static int common_check(void *context,uint32_t *errors)
{
    IfsClassicFatIo io;
    IfsClassicFatVolume geometry;
    if(!common_session_valid(context) || !errors) return IFS_FS_INVALID;
    io.read_at=common_read;
    io.context=&common_session.host;
    io.device_bytes=common_session.host.volume_bytes;
    if(ifs_classic_fat_probe(&io,&geometry)!=0)
        return IFS_FS_CORRUPT;
    return ifs_classic_fat_check(&io,&geometry,errors)==0 ?
           IFS_FS_OK:IFS_FS_IO_ERROR;
}


static int common_recover(void *context,uint32_t *repaired)
{
    IfsClassicFatIo io;
    IfsClassicFatVolume geometry;
    int result;
    if(!common_session_valid(context) || !repaired) return IFS_FS_INVALID;
    if(!common_writable()) return IFS_FS_ACCESS;
    io.read_at=common_read;
    io.context=&common_session.host;
    io.device_bytes=common_session.host.volume_bytes;
    if(ifs_classic_fat_probe(&io,&geometry)!=0)
        return IFS_FS_CORRUPT;
    result=ifs_classic_fat_repair_mirrors(
        &io,&geometry,common_write,
        common_flush,repaired);
    return result==0 ? IFS_FS_OK :
           result==1 ? IFS_FS_CORRUPT :
           result==2 ? IFS_FS_UNSUPPORTED : IFS_FS_IO_ERROR;
}

/* Every public filesystem operation is now a direct Common V2 callback.
 * All original FatFs operations and allocation checks are retained. */
/* Expose only independently qualified mounted-volume operations. */
/* Reuse the mounted FatFs engine's authoritative allocation accounting.
 * Do not infer free space from the partition size or parse the FAT again. */
static int v2_query_volume(void *mounted, IfsFsVolumeInfoV2 *info)
{
    uint64_t total = 0u, free_bytes = 0u;
    if (!common_session_valid(mounted) || !info ||
        info->structure_bytes < sizeof(*info)) return IFS_FS_INVALID;
    if (volume_space(&total, &free_bytes) != 0) return IFS_FS_IO_ERROR;
    info->variant = 12u;
    info->max_name_bytes = 765u;
    info->block_bytes = (uint32_t)volume.csize * device.sector_bytes;
    info->total_bytes = total;
    info->free_bytes = free_bytes;
    return IFS_FS_OK;
}
static int guarded_v2_query_volume(void *mounted, IfsFsVolumeInfoV2 *info)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(v2_query_volume(mounted,info));
}

static int common_v2_sync(void *volume)
{
    if (!common_session_valid(volume)) return IFS_FS_INVALID;
    if (!common_session.host.flush) return IFS_FS_OK; /* read-only */
    return common_session.host.flush(common_session.host.context) == 0 ?
           IFS_FS_OK : IFS_FS_IO_ERROR;
}
static int guarded_common_v2_sync(void *volume)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_v2_sync(volume));
}

static int common_v2_check(void *volume, uint32_t flags, uint64_t *errors)
{
    uint32_t count = 0;
    int status;
    if (!errors) return IFS_FS_INVALID;
    if (flags) return IFS_FS_UNSUPPORTED;
    status = common_check(volume, &count);
    if (status == IFS_FS_OK) *errors = count;
    return status;
}
static int guarded_common_v2_check(void *volume, uint32_t flags, uint64_t *errors)
{
    if (errors) *errors=0u;
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_v2_check(volume,flags,errors));
}

static int common_v2_repair(void *volume, uint32_t flags, uint64_t *repaired)
{
    uint32_t count = 0;
    int status;
    if (!repaired) return IFS_FS_INVALID;
    if (flags) return IFS_FS_UNSUPPORTED;
    status = common_recover(volume, &count);
    if (status == IFS_FS_OK) *repaired = count;
    return status;
}
static int guarded_common_v2_repair(void *volume, uint32_t flags, uint64_t *repaired)
{
    if (repaired) *repaired=0u;
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_v2_repair(volume,flags,repaired));
}

static int v2_label(void *mounted, char *out, uint32_t capacity, uint32_t *bytes)
{
    if (!common_session_valid(mounted) || !out || !capacity || !bytes)
        return IFS_FS_INVALID;

    /* FatFs converts the FAT label to UTF-8. Eleven characters fit even
     * when every character requires the maximum UTF-8 width. */
    char label[64] = {0};
    if (f_getlabel("0:", label, NULL) != FR_OK)
        return IFS_FS_IO_ERROR;
    const size_t length = strlen(label);
    if (length >= capacity)
        return IFS_FS_NO_SPACE;
    memcpy(out, label, length + 1);
    *bytes = (uint32_t)length;
    return IFS_FS_OK;
}
static int guarded_v2_label(void *mounted, char *out, uint32_t capacity, uint32_t *bytes)
{
    if (bytes) *bytes=0u;
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(v2_label(mounted,out,capacity,bytes));
}

/* FAT volume labels occupy an OEM-encoded, 11-byte root-directory entry.
 * Let the existing native engine perform conversion, case folding and
 * allocation. Reject controls and drive prefixes before it can parse a
 * truncated name or switch logical drives. No raw boot-sector edits. */
static int v2_set_label(void *mounted,const char *label)
{
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    if (!common_session_valid(mounted) || !label)
        return v2_leave_operation(IFS_FS_INVALID);
    if (!common_writable()) return v2_leave_operation(IFS_FS_ACCESS);
    uint32_t n=0u;
    while (n<=44u && label[n]) {
        unsigned char c=(unsigned char)label[n++];
        if (c<32u || c==127u || c==':')
            return v2_leave_operation(IFS_FS_INVALID);
    }
    if (n>44u) return v2_leave_operation(IFS_FS_INVALID);
    FRESULT rc=f_setlabel(label);
    if (rc==FR_OK) return v2_leave_operation(IFS_FS_OK);
    if (rc==FR_INVALID_NAME || rc==FR_INVALID_PARAMETER)
        return v2_leave_operation(IFS_FS_INVALID);
    if (rc==FR_DENIED) return v2_leave_operation(IFS_FS_NO_SPACE);
    if (rc==FR_WRITE_PROTECTED) return v2_leave_operation(IFS_FS_ACCESS);
    if (rc==FR_INT_ERR) return v2_leave_operation(IFS_FS_CORRUPT);
    return v2_leave_operation(IFS_FS_IO_ERROR);
}

__attribute__((used,section(".ifsmeta")))
static const char common_v2_identity[]="IFS-MODULE:fat12:common2";
static const IfsFsVolumeOpsV2 common_v2_volume={
    .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsVolumeOpsV2),
             IFS_FS_GROUP_VOLUME,0u,
             IFS_FS_VOLUME_QUERY|IFS_FS_VOLUME_LABEL_GET|IFS_FS_VOLUME_LABEL_SET|IFS_FS_VOLUME_SYNC|IFS_FS_VOLUME_CHECK|
             IFS_FS_VOLUME_REPAIR|IFS_FS_VOLUME_FORMAT},
    .label=guarded_v2_label,.set_label=v2_set_label,
    .query=guarded_v2_query_volume,
    .sync=guarded_common_v2_sync,
    .check=guarded_common_v2_check,
    .repair=guarded_common_v2_repair,
    .format=guarded_common_format
};
/* One native FAT12 CORE engine implements both the on-media file namespace
 * and mounted volume lifetime. There is no historic ABI pointer or fallback. */
static const IfsFsCoreOpsV2 common_v2_core = {
    .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsCoreOpsV2),
             IFS_FS_GROUP_CORE,0u,
             IFS_FS_CORE_PROBE|IFS_FS_CORE_MOUNT|IFS_FS_CORE_STAT|
             IFS_FS_CORE_LIST|IFS_FS_CORE_READ|IFS_FS_CORE_WRITE|
             IFS_FS_CORE_CREATE|IFS_FS_CORE_REMOVE|IFS_FS_CORE_RENAME|
             IFS_FS_CORE_TRUNCATE},
    .probe=guarded_common_probe,
    .mount=guarded_common_mount,
    .unmount=guarded_common_unmount,
    .stat=guarded_common_stat,
    .directory_entry=guarded_common_directory_entry,
    .read=guarded_common_file_read,
    .write=guarded_common_file_write,
    .create=guarded_common_create,
    .remove=guarded_common_remove,
    .rename=guarded_common_rename,
    .truncate=guarded_common_truncate
};
/* Common V2 read-only DEFRAG MAP. A native FatFs FIL determines the
 * authoritative first cluster; its own get_fat() follows allocated blocks.
 * Validate the complete chain before accepting a map, including clusters
 * outside a requested page, and never return success on a broken FAT chain.
 */
/* Directory sizes are not stored in FAT directory entries. Open the
 * directory with FatFs and walk the native cluster chain to EOC; do not
 * guess its allocation from its entry count. "/" retains the existing
 * whole-volume allocation-map contract. Every non-root directory map is
 * validated through the terminal FAT link even when the caller's page
 * fills early, so loops and truncated chains cannot yield a short pass. */
static int common_v2_directory_map(const char *path,uint64_t first,
                                    IfsFsExtentV2 *out,uint32_t capacity,
                                    uint32_t *written)
{
    DIR dir;
    FRESULT opened=f_opendir(&dir,path);
    if(opened!=FR_OK)return fat_status(opened);
    FIL table={0};
    table.obj.fs=&volume;
    DWORD cluster=dir.obj.sclust,previous=0u;
    const uint64_t unit=(uint64_t)volume.csize*device.sector_bytes;
    uint64_t seen=0u;
    uint32_t count=0u;
    int result=IFS_FS_CORRUPT;
    if(!unit || volume.n_fatent<3u)goto done;
    for(uint32_t i=0u;i<volume.n_fatent-2u;++i) {
        if(common_session.host.cancelled &&
           common_session.host.cancelled(common_session.host.context)){
            result=IFS_FS_BUSY;goto done;
        }
        if(cluster<2u || cluster>=volume.n_fatent || cluster>=0xff0u)
            goto done;
        const uint64_t physical=((uint64_t)volume.database+
            (uint64_t)(cluster-2u)*volume.csize)*device.sector_bytes;
        if(physical>device.partition_bytes ||
           unit>device.partition_bytes-physical)goto done;
        if(!i || cluster!=previous+1u){
            if(seen>=first && count<capacity){
                out[count++]=(IfsFsExtentV2){
                    .logical_offset=(uint64_t)i*unit,
                    .physical_offset=physical,.length=unit};
            }
            ++seen;
        }else if(seen>first && seen-first<=count)out[count-1u].length+=unit;
        previous=cluster;
        DWORD next=ifs_fatfs_native_next_cluster(&table,cluster);
        if(next==UINT32_MAX){result=IFS_FS_IO_ERROR;goto done;}
        if(next>=0xff8u && next<=0xfffu){
            result=count?IFS_FS_OK:IFS_FS_END;goto done;
        }
        if(next<2u || next>=volume.n_fatent || next>=0xff0u)goto done;
        cluster=next;
    }
done:
    if(f_closedir(&dir)!=FR_OK)result=IFS_FS_IO_ERROR;
    *written=result==IFS_FS_OK?count:0u;
    return result;
}

static int common_v2_extent_map(void *context,const char *path,
                                uint64_t first_extent,IfsFsExtentV2 *out,
                                uint32_t capacity,uint32_t *written)
{
    char native_path[4096];
    FIL file;
    uint64_t file_size,unit,clusters,extents_seen=0u;
    uint32_t count=0u;
    DWORD current,next,previous=0u;
    int result;
    FRESULT opened;
    if(!written || !out || !capacity || !common_session_valid(context))
        return IFS_FS_INVALID;
    *written=0u;
    result=common_path(path,native_path);
    if(result!=IFS_FS_OK)return result;
    if(native_path[2]=='/' && !native_path[3]){
        /* "/" requests the volume's allocated-data run map, not a file
         * chain. The native FAT walker supplies authoritative allocations. */
        FIL table={0};
        table.obj.fs=&volume;
        uint64_t seen=0u, begin=0u, bytes=0u;
        uint32_t mapped=0u;
        const uint64_t allocation_unit=
            (uint64_t)volume.csize*device.sector_bytes;
        if(!allocation_unit || volume.n_fatent<3u)return IFS_FS_CORRUPT;
        for(uint32_t cluster=2u;cluster<volume.n_fatent;++cluster){
            const DWORD value=ifs_fatfs_native_next_cluster(&table,cluster);
            if(value==UINT32_MAX || value==1u){
                *written=0u;
                return value==UINT32_MAX?IFS_FS_IO_ERROR:IFS_FS_CORRUPT;
            }
            if(value && !bytes)
                begin=(uint64_t)(cluster-2u)*allocation_unit;
            if(value)
                bytes+=allocation_unit;
            if((!value || cluster+1u==volume.n_fatent) && bytes){
                if(seen>=first_extent && mapped<capacity){
                    const uint64_t physical=
                        (uint64_t)volume.database*device.sector_bytes+begin;
                    if(physical>device.partition_bytes ||
                       bytes>device.partition_bytes-physical){
                        *written=0u;
                        return IFS_FS_CORRUPT;
                    }
                    out[mapped++]=(IfsFsExtentV2){
                        .logical_offset=begin,.physical_offset=physical,
                        .length=bytes,.flags=0u,.reserved=0u};
                }
                ++seen;
                bytes=0u;
                if(mapped==capacity)break;
            }
        }
        *written=mapped;
        return mapped?IFS_FS_OK:IFS_FS_END;
    }
    opened=f_open(&file,native_path,FA_READ);
    if(opened==FR_NO_FILE || opened==FR_NO_PATH || opened==FR_DENIED)
        return common_v2_directory_map(native_path,first_extent,out,capacity,written);
    if(opened==FR_INVALID_NAME)return IFS_FS_INVALID;
    if(opened!=FR_OK)return IFS_FS_IO_ERROR;
    result=IFS_FS_OK;
    unit=(uint64_t)volume.csize*device.sector_bytes;
    file_size=(uint64_t)f_size(&file);
    if(!unit || !volume.csize || file_size>UINT32_MAX ||
       volume.n_fatent<3u){result=IFS_FS_CORRUPT;goto done;}
    if(!file_size){
        result=file.obj.sclust?IFS_FS_CORRUPT:IFS_FS_END;
        goto done;
    }
    clusters=(file_size+unit-1u)/unit;
    current=file.obj.sclust;
    if(clusters>volume.n_fatent-2u || current<2u ||
       current>=volume.n_fatent){result=IFS_FS_CORRUPT;goto done;}
    for(uint64_t i=0u;i<clusters;++i){
        const uint64_t logical=i*unit;
        const uint64_t length=file_size-logical<unit?
            file_size-logical:unit;
        const uint64_t sector=(uint64_t)volume.database+
            (uint64_t)(current-2u)*volume.csize;
        const uint64_t physical=sector*device.sector_bytes;
        if(sector>=device.partition_bytes/device.sector_bytes ||
           physical>device.partition_bytes ||
           length>device.partition_bytes-physical){
            result=IFS_FS_CORRUPT;goto done;
        }
        if(!i || current!=previous+1u){
            /* Extent indices, not allocation-cluster indices. */
            if(extents_seen>=first_extent && count<capacity){
                out[count]=(IfsFsExtentV2){
                    .logical_offset=logical,.physical_offset=physical,
                    .length=length,.flags=0u,.reserved=0u
                };
                ++count;
            }
            ++extents_seen;
        }else if(extents_seen>first_extent &&
                  extents_seen-first_extent<=count){
            out[count-1u].length+=length;
        }
        previous=current;
        next=ifs_fatfs_native_next_cluster(&file,current);
        if(next==UINT32_MAX){result=IFS_FS_IO_ERROR;goto done;}
        if(i+1u<clusters){
            if(next<2u || next>=volume.n_fatent || next>=0xff8u){
                result=IFS_FS_CORRUPT;goto done;
            }
            current=next;
        }else if(next<0xff8u || next>0xfffu){
            result=IFS_FS_CORRUPT;goto done;
        }
    }
    result=count?IFS_FS_OK:IFS_FS_END;
done:
    if(f_close(&file)!=FR_OK)result=IFS_FS_IO_ERROR;
    if(result!=IFS_FS_OK)count=0u;
    *written=count;
    return result;
}
static int guarded_common_v2_extent_map(void *context,const char *path,
                                uint64_t first_extent,IfsFsExtentV2 *out,
                                uint32_t capacity,uint32_t *written)
{
    if (written) *written=0u;
    if (!v2_enter_operation()) return IFS_FS_BUSY;
    return v2_leave_operation(common_v2_extent_map(context,path,first_extent,out,capacity,written));
}

#include "fat12_relocation.h"
static const IfsFsDefragOpsV2 common_v2_defrag={
    .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsDefragOpsV2),
             IFS_FS_GROUP_DEFRAG,0u,IFS_FS_DEFRAG_ALL},
    .map=guarded_common_v2_extent_map,
    .plan=guarded_fr_plan,.execute=guarded_fr_execute,.verify=guarded_fr_verify,
    .recover=guarded_fr_recover,.release_plan=guarded_fr_release
};

static const IfsFsGroupHeaderV2 *common_v2_query(
    uint32_t id, uint32_t minimum, uint32_t maximum)
{
    if (minimum>IFS_FS_GROUP_ABI_V2 || maximum<IFS_FS_GROUP_ABI_V2)
        return NULL;
    if (id==IFS_FS_GROUP_CORE) return &common_v2_core.header;
    if (id==IFS_FS_GROUP_VOLUME) return &common_v2_volume.header;
    if (id==IFS_FS_GROUP_DEFRAG) return &common_v2_defrag.header;
    return NULL;
}
static const IfsFsModuleApiV2 common_v2_api={
    IFS_FS_COMMON_ABI_V2,sizeof(IfsFsModuleApiV2),"fat12",
    IFS_FS_PROFILE_HOSTED|IFS_FS_PROFILE_FREESTANDING|IFS_FS_PROFILE_FIRMWARE,
    IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME)|
    IFS_FS_GROUP_BIT(IFS_FS_GROUP_DEFRAG),
    common_v2_query
};
__attribute__((visibility("default")))
const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void)
{
    return &common_v2_api;
}
