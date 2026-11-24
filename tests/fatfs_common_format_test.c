/* SPDX-License-Identifier: GPL-3.0-or-later
 * Black-box Common ABI integration on an existing disposable file image.
 * Tests the production filesystem shared object, never a test-only engine.
 */
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/filesystem_module_v2.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { int fd; uint64_t bytes; uint32_t sector; unsigned writes; int fail_reads; unsigned failed_reads; } Disk;
static int bounds(const Disk *d,uint64_t at,uint32_t n)
{ return d->sector && !(at%d->sector) && !(n%d->sector) &&
         at<=d->bytes && n<=d->bytes-at; }
static const IfsFsCoreOpsV2 *unmount_during_read;
static void *active_volume;
static int observed_unmount;
static int rd(void *u,uint64_t at,void *p,uint32_t n)
{
    if (unmount_during_read) {
        const IfsFsCoreOpsV2 *ops=unmount_during_read;
        unmount_during_read=NULL;
        observed_unmount=ops->unmount(active_volume);
        if (observed_unmount!=IFS_FS_BUSY) return -1;
    }
    Disk *d=u;
    if (d->fail_reads) { ++d->failed_reads; return -1; }
    return bounds(d,at,n) &&
        pread(d->fd,p,n,(off_t)at)==(ssize_t)n?0:-1;
}
static int wr(void *u,uint64_t at,const void *p,uint32_t n)
{
    Disk *d=u;
    if(!bounds(d,at,n) ||
       pwrite(d->fd,p,n,(off_t)at)!=(ssize_t)n)return -1;
    ++d->writes;
    return 0;
}
static int flush(void *u) { return fsync(((Disk *)u)->fd)==0?0:-1; }
static int fail(const char *message)
{ fprintf(stderr,"FAT12-COMMON-FAIL: %s\n",message);return 1; }
static void *allocate_at(void *ctx,size_t n) {(void)ctx;return malloc(n);}
static void release_at(void *ctx,void *p) {(void)ctx;free(p);}

/* Inspect the short directory entry directly, independently of FatFs. */
static uint64_t fixture_clock(void *ctx)
{ (void)ctx; return UINT64_C(1709212456000000000); }
static uint32_t read16(const unsigned char *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8); }
static uint32_t read32(const unsigned char *p)
{ return read16(p)|(read16(p+2)<<16); }
static int check_clock_entry(Disk *disk)
{
    unsigned char boot[512], root[4096];
    if (pread(disk->fd,boot,sizeof boot,0)!=(ssize_t)sizeof boot) return -1;
    uint32_t sector=read16(boot+11), reserved=read16(boot+14), fats=boot[16];
    uint32_t fat_sectors=read16(boot+22);
    if (!fat_sectors) fat_sectors=read32(boot+36);
    if (sector<512u || sector>sizeof root || !fat_sectors) return -1;
    uint64_t root_sector=(uint64_t)reserved+(uint64_t)fats*fat_sectors;
    if (!read16(boot+17)) {
        uint32_t cluster=read32(boot+44);
        if (cluster<2u || !boot[13]) return -1;
        root_sector+=(uint64_t)(cluster-2u)*boot[13];
    }
    if (pread(disk->fd,root,sector,(off_t)(root_sector*sector))!=(ssize_t)sector) return -1;
    for (uint32_t i=0;i+32u<=sector;i+=32u) {
        if (!root[i]) break;
        if (!memcmp(root+i,"CLOCK   TXT",11u)) {
            const uint32_t date=((2024u-1980u)<<9)|(2u<<5)|29u;
            const uint32_t time=(13u<<11)|(14u<<5)|8u;
            return read16(root+i+24u)==date && read16(root+i+22u)==time?0:-1;
        }
    }
    return -1;
}

int main(int argc,char **argv)
{
    if(argc!=4)return fail("usage: module regular-file-image sector-bytes");
    Disk disk={0};
    disk.fd=open(argv[2],O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    struct stat st={0};
    if(disk.fd<0 || fstat(disk.fd,&st)!=0 || !S_ISREG(st.st_mode) ||
       st.st_size<=0)return fail("only an existing regular-file image is permitted");
    disk.bytes=(uint64_t)st.st_size;
    char *end=NULL;
    errno=0;
    unsigned long bps=strtoul(argv[3],&end,10);
    if(errno || !end || *end || bps!=512u)
        return fail("fixture needs 512-byte logical sectors");
    disk.sector=(uint32_t)bps;

    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library)return fail(dlerror());
    void *symbol=dlsym(library,IFS_FS_COMMON_EXPORT_V2);
    IfsFsModuleApiEntryV2 resolve=NULL;
    if (sizeof(symbol)!=sizeof(resolve))return fail("function pointer size");
    memcpy(&resolve,&symbol,sizeof(resolve));
    const IfsFsModuleApiV2 *api=resolve?resolve():NULL;
    if(!ifs_fs_api_v2_valid(api,"fat12"))
        return fail("canonical Common V2 descriptor invalid");
    const IfsFsCoreOpsV2 *fs =
        (const IfsFsCoreOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    const IfsFsVolumeOpsV2 *vol =
        (const IfsFsVolumeOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_VOLUME);
    if(!fs || !vol || !fs->probe || !fs->mount || !fs->unmount ||
       !fs->read || !fs->write || !fs->create || !fs->remove ||
       !fs->rename || !fs->truncate || !fs->directory_entry ||
       !fs->stat || !vol->format || !vol->check || !vol->query)
        return fail("canonical V2 operations missing");

    IfsFsHostRuntimeV2 io={0};
    io.abi_version=IFS_FS_HOST_ABI_V2;
    io.structure_bytes=sizeof io;
    io.capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE;
    io.context=&disk;
    io.volume_bytes=disk.bytes;
    io.logical_sector_bytes=(uint32_t)bps;
    io.physical_sector_bytes=(uint32_t)bps;
    io.maximum_transfer_bytes=65536u;
    io.read_at=rd;
    io.write_at=wr;
    io.flush=flush;
    io.allocate=allocate_at;
    io.release=release_at;
    uint32_t variant=UINT32_MAX;
    IfsFsHostRuntimeV2 invalid_host=io;
    invalid_host.abi_version=1u;
    if(fs->probe(&invalid_host,&variant)!=IFS_FS_INVALID || variant!=0u)
        return fail("Common V2 probe retained a stale variant for invalid host");

    if(vol->format(&io,16u,NULL,0u)!=IFS_FS_UNSUPPORTED || disk.writes)
        return fail("foreign format must not write");
    io.logical_sector_bytes=256u;
    if(vol->format(&io,12u,NULL,0u)!=IFS_FS_INVALID || disk.writes)
        return fail("invalid sector size must not write");
    io.logical_sector_bytes=(uint32_t)bps;
    io.capabilities=IFS_FS_HOST_READ_ONLY;
    io.write_at=NULL;io.flush=NULL;
    if(vol->format(&io,12u,NULL,0u)!=IFS_FS_ACCESS || disk.writes)
        return fail("read-only format must not write");
    io.capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE;
    io.write_at=wr;io.flush=flush;

    if(vol->format(&io,12u,NULL,0u)!=IFS_FS_OK || !disk.writes ||
       fs->probe(&io,&variant)!=IFS_FS_OK || variant!=12u)
        return fail("blank FAT12 format and probe");

    IfsFsVolumeInfoV2 info={0};
    info.structure_bytes=sizeof info;
    void *session=NULL;
    IfsFsHostRuntimeV2 clock_io=io;
    clock_io.capabilities|=IFS_FS_HOST_CLOCK;
    clock_io.monotonic_ns=fixture_clock;
    clock_io.realtime_ns=fixture_clock;
    if(fs->mount(&clock_io,&session,&info)!=IFS_FS_OK || !session ||
       info.variant!=12u || info.free_bytes>info.total_bytes)
        return fail("mount formatted FAT12 image");
    if (fs->create(session,"/CLOCK.TXT",IFS_FS_ENTRY_FILE)!=IFS_FS_OK ||
        fs->write(session,"/CLOCK.TXT",0u,"x",1u)!=IFS_FS_OK ||
        vol->sync(session)!=IFS_FS_OK || check_clock_entry(&disk)!=0 ||
        fs->remove(session,"/CLOCK.TXT")!=IFS_FS_OK)
        return fail("native FAT timestamp must match host leap-day clock");
    {
        IfsFsVolumeInfoV2 current={0};
        current.structure_bytes=sizeof current;
        if (!(vol->header.capabilities & IFS_FS_VOLUME_QUERY) ||
            vol->query(session,&current)!=IFS_FS_OK ||
            current.variant!=info.variant ||
            current.total_bytes!=info.total_bytes ||
            current.free_bytes!=info.free_bytes ||
            current.block_bytes!=info.block_bytes)
            return fail("Common V2 volume inquiry differs from mounted engine");
    }
    if(disk.bytes==255u*1024u*1024u && info.block_bytes!=65536u)
        return fail("255 MiB FAT12 allocation unit must be 64 KiB");
    const char *dir="/SUBDIR";
    const char *name="/SUBDIR/Verified Café Unicode Data.txt";
    const char *renamed="/SUBDIR/Renamed File.txt";
    unsigned char payload[3072],readback[3072];
    for(unsigned i=0;i<sizeof payload;++i)
        payload[i]=(unsigned char)((i*17u+23u)&255u);
    char outname[768]={0};
    IfsFsEntryV2 entry={0};
    entry.structure_bytes=sizeof entry;
    entry.name=outname;
    entry.name_capacity=sizeof outname;
    if(fs->create(session,dir,IFS_FS_ENTRY_DIRECTORY)!=IFS_FS_OK ||
       fs->create(session,name,IFS_FS_ENTRY_FILE)!=IFS_FS_OK ||
       fs->write(session,name,0u,payload,sizeof payload)!=IFS_FS_OK ||
       fs->read(session,name,0u,readback,sizeof readback)!=IFS_FS_OK ||
       memcmp(payload,readback,sizeof payload)!=0 ||
       fs->rename(session,name,renamed)!=IFS_FS_OK ||
       fs->stat(session,renamed,&entry)!=IFS_FS_OK ||
       entry.size_bytes!=sizeof payload ||
       fs->truncate(session,renamed,1000u)!=IFS_FS_OK ||
       fs->stat(session,renamed,&entry)!=IFS_FS_OK ||
       entry.size_bytes!=1000u)
        return fail("create/write/read/Unicode/rename/truncate");
    /* Disk callback attempts to unmount during a real data transfer. */
    active_volume=session;
    unmount_during_read=fs;
    observed_unmount=IFS_FS_INVALID;
    if (fs->read(session,renamed,0u,readback,1000u)!=IFS_FS_OK ||
        unmount_during_read || observed_unmount!=IFS_FS_BUSY)
        return fail("reentrant unmount must report BUSY");
    /* Verify a failed host read cannot be reported as a missing file.
     * Directory entries may be cached; assert STAT only when I/O occurs. */
    disk.fail_reads=1;
    disk.failed_reads=0u;
    int fault_stat=fs->stat(session,renamed,&entry);
    if (disk.failed_reads && fault_stat!=IFS_FS_IO_ERROR)
        return fail("STAT disguised media I/O failure as missing path");
    disk.failed_reads=0u;
    if (fs->read(session,renamed,0u,readback,1000u)!=IFS_FS_IO_ERROR ||
        !disk.failed_reads)
        return fail("read failed to propagate the host I/O error");
    disk.fail_reads=0;
    if (fs->stat(session,"/SUBDIR/ABSENT.TXT",&entry)!=IFS_FS_NOT_FOUND ||
        fs->read(session,renamed,0u,readback,1000u)!=IFS_FS_OK)
        return fail("fault recovery confused a missing FAT12 path");

    {
        const IfsFsDefragOpsV2 *defrag=
            (const IfsFsDefragOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_DEFRAG);
        IfsFsExtentV2 spans[8]={{0}};
        uint32_t count=UINT32_MAX;
        if (!defrag || defrag->header.capabilities!=IFS_FS_DEFRAG_MAP ||
            !defrag->map || defrag->plan || defrag->execute ||
            defrag->verify || defrag->recover || defrag->release_plan ||
            defrag->map(session,renamed,0u,spans,8u,&count)!=IFS_FS_OK ||
            count==0u || spans[0].logical_offset!=0u ||
            spans[0].physical_offset>=disk.bytes ||
            spans[0].length<1000u)
            return fail("FAT12 Common V2 extent map contract");
        count=UINT32_MAX;
        if (defrag->map(session,"/",0u,spans,8u,&count)!=IFS_FS_OK ||
            count==0u || spans[0].physical_offset>=disk.bytes ||
            spans[0].length==0u)
            return fail("FAT12 Common V2 root allocation map");
    }

    uint64_t errors=UINT64_MAX;
    if(vol->check(session,0u,&errors)!=IFS_FS_OK || errors!=0u ||
       fs->unmount(session)!=IFS_FS_OK)
        return fail("new-media FAT integrity or unmount");

    info.structure_bytes=sizeof info;
    if(fs->mount(&io,&session,&info)!=IFS_FS_OK ||
       fs->read(session,renamed,0,readback,1000u)!=IFS_FS_OK ||
       memcmp(payload,readback,1000u)!=0 ||
       fs->read(session,"/../SUBDIR/Renamed File.txt",0,readback,1)!=IFS_FS_INVALID)
        return fail("remount persistence or path traversal check");

    int found=0;
    for(uint32_t index=0;index<32u;++index) {
        int rc=fs->directory_entry(session,dir,index,&entry);
        if(rc==IFS_FS_END)break;
        if(rc!=IFS_FS_OK)return fail("directory enumeration error");
        if(strcmp(entry.name,"Renamed File.txt")==0)found=1;
    }
    if(!found || fs->remove(session,renamed)!=IFS_FS_OK ||
       fs->remove(session,dir)!=IFS_FS_OK ||
       vol->check(session,0u,&errors)!=IFS_FS_OK || errors!=0u ||
       fs->unmount(session)!=IFS_FS_OK || flush(&disk)!=0)
        return fail("enumerate/delete/final integrity");

    /* The existing 512-byte FAT filesystem also runs over a strict 4 KiB
     * host lease; sub-sector file updates must preserve neighbouring bytes. */
    if (disk.bytes % 4096u == 0u) {
        disk.sector=4096u;
        io.logical_sector_bytes=io.physical_sector_bytes=4096u;
        info.structure_bytes=sizeof info;
        session=NULL;
        if (fs->probe(&io,&variant)!=IFS_FS_OK ||
            fs->mount(&io,&session,&info)!=IFS_FS_OK || !session ||
            fs->create(session,"/FOURK.TXT",IFS_FS_ENTRY_FILE)!=IFS_FS_OK ||
            fs->write(session,"/FOURK.TXT",0u,payload,sizeof payload)!=IFS_FS_OK ||
            fs->read(session,"/FOURK.TXT",0u,readback,sizeof readback)!=IFS_FS_OK ||
            memcmp(payload,readback,sizeof payload)!=0 ||
            fs->remove(session,"/FOURK.TXT")!=IFS_FS_OK)
            return fail("4 KiB host-sector V2 round trip");
        /* Damage only the second FAT in this disposable file, then exercise
         * the V2 repair's 128-byte requests through the 4 KiB host lease. */
        unsigned char boot[512], original, damaged, verified;
        if (pread(disk.fd,boot,sizeof boot,0)!=(ssize_t)sizeof boot || boot[16]<2u)
            return fail("FAT mirror fixture geometry");
        uint32_t reserved=(uint32_t)boot[14] | ((uint32_t)boot[15]<<8);
        uint32_t fat_sectors=(uint32_t)boot[22] | ((uint32_t)boot[23]<<8);
        uint32_t fs_sector=(uint32_t)boot[11] | ((uint32_t)boot[12]<<8);
        uint64_t mirror=((uint64_t)reserved+fat_sectors)*fs_sector+128u;
        if (mirror>=disk.bytes || pread(disk.fd,&original,1,(off_t)mirror)!=1)
            return fail("FAT mirror fixture read");
        damaged=(unsigned char)(original ^ 0x40u);
        uint64_t repairs=0;
        if (pwrite(disk.fd,&damaged,1,(off_t)mirror)!=1 ||
            vol->repair(session,0u,&repairs)!=IFS_FS_OK || repairs==0 ||
            pread(disk.fd,&verified,1,(off_t)mirror)!=1 || verified!=original ||
            fs->unmount(session)!=IFS_FS_OK)
            return fail("4 KiB host-sector FAT mirror repair");
    }

    if(close(disk.fd)!=0 || dlclose(library)!=0)return fail("close");
    puts("FAT12-COMMON-FORMAT-READ-WRITE-RENAME-PERSISTENCE-PASS");
    return 0;
}
