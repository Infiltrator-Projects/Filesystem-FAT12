/* SPDX-License-Identifier: GPL-3.0-or-later
 * Black-box Common ABI integration on an existing disposable file image.
 * Tests the production filesystem shared object, never a test-only engine.
 */
#define _POSIX_C_SOURCE 200809L
#include "infiltratr/filesystem_module.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { int fd; uint64_t bytes; unsigned writes; } Disk;
static int bounds(const Disk *d,uint64_t at,uint32_t n)
{ return at<=d->bytes && n<=d->bytes-at; }
static int rd(void *u,uint64_t at,void *p,uint32_t n)
{
    Disk *d=u;
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

    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library)return fail(dlerror());
    const IfsFsModuleApiV1 *(*resolve)(void)=
        (const IfsFsModuleApiV1 *(*)(void))dlsym(library,IFS_FS_COMMON_EXPORT_V1);
    const IfsFsModuleApiV1 *fs=resolve?resolve():NULL;
    if(!ifs_fs_common_api_valid(fs,"fat12") ||
       !fs->format || !fs->probe || !fs->mount || !fs->unmount ||
       !fs->check || !fs->read || !fs->write || !fs->create ||
       !fs->remove || !fs->rename || !fs->truncate ||
       !fs->directory_entry || !fs->stat)
        return fail("canonical filesystem API incomplete");

    IfsFsHostFormatIoV1 io={0};
    io.host.structure_bytes=sizeof io;
    io.host.flags=IFS_FS_IO_EXCLUSIVE;
    io.host.context=&disk;
    io.host.volume_bytes=disk.bytes;
    io.host.read_at=rd;
    io.host.write_at=wr;
    io.host.flush=flush;
    io.logical_sector_bytes=(uint32_t)bps;
    uint32_t variant=0;

    if(fs->format(&io.host,16u,NULL)!=IFS_FS_UNSUPPORTED || disk.writes)
        return fail("foreign format must not write");
    io.logical_sector_bytes=256u;
    if(fs->format(&io.host,12u,NULL)!=IFS_FS_INVALID || disk.writes)
        return fail("invalid sector size must not write");
    io.logical_sector_bytes=(uint32_t)bps;
    io.host.flags=IFS_FS_IO_READ_ONLY;
    if(fs->format(&io.host,12u,NULL)!=IFS_FS_ACCESS || disk.writes)
        return fail("read-only format must not write");
    io.host.flags=IFS_FS_IO_EXCLUSIVE;

    if(fs->format(&io.host,12u,NULL)!=IFS_FS_OK || !disk.writes ||
       fs->probe(&io.host,&variant)!=IFS_FS_OK || variant!=12u)
        return fail("blank FAT12 format and probe");

    IfsFsVolumeInfoV1 info={0};
    info.structure_bytes=sizeof info;
    void *session=NULL;
    if(fs->mount(&io.host,&session,&info)!=IFS_FS_OK || !session ||
       info.variant!=12u || info.free_bytes>info.total_bytes)
        return fail("mount formatted FAT12 image");
    const char *dir="/SUBDIR";
    const char *name="/SUBDIR/Verified Café Unicode Data.txt";
    const char *renamed="/SUBDIR/Renamed File.txt";
    unsigned char payload[3072],readback[3072];
    for(unsigned i=0;i<sizeof payload;++i)
        payload[i]=(unsigned char)((i*17u+23u)&255u);
    char outname[768]={0};
    IfsFsEntryV1 entry={0};
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

    uint32_t errors=UINT32_MAX;
    if(fs->check(session,&errors)!=IFS_FS_OK || errors!=0u ||
       fs->unmount(session)!=IFS_FS_OK)
        return fail("new-media FAT integrity or unmount");

    info.structure_bytes=sizeof info;
    if(fs->mount(&io.host,&session,&info)!=IFS_FS_OK ||
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
       fs->check(session,&errors)!=IFS_FS_OK || errors!=0u ||
       fs->unmount(session)!=IFS_FS_OK || flush(&disk)!=0)
        return fail("enumerate/delete/final integrity");

    if(close(disk.fd)!=0 || dlclose(library)!=0)return fail("close");
    puts("FAT12-COMMON-FORMAT-READ-WRITE-RENAME-PERSISTENCE-PASS");
    return 0;
}
