/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Formatter and metadata integration on regular disposable image files only. */
#define _POSIX_C_SOURCE 200809L
#include "fatfs_module.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {FILE *file; unsigned writes;} Disk;
static int read_at(void *u,uint64_t at,void *dst,uint32_t bytes) {
    Disk *d=u;
    return at<=INT64_MAX && fseeko(d->file,(off_t)at,SEEK_SET)==0 &&
        fread(dst,1,bytes,d->file)==bytes ? 0:-1;
}
static int write_at(void *u,uint64_t at,const void *src,uint32_t bytes) {
    Disk *d=u;
    ++d->writes;
    return at<=INT64_MAX && fseeko(d->file,(off_t)at,SEEK_SET)==0 &&
        fwrite(src,1,bytes,d->file)==bytes ? 0:-1;
}
static int flush_at(void *u) {
    Disk *d=u;
    return fflush(d->file)==0 && fsync(fileno(d->file))==0 ? 0:-1;
}
static uint32_t time_at(void *u) {
    (void)u;
    return ((uint32_t)(2026-1980)<<25)|(10u<<21)|(28u<<16)|(12u<<11);
}
static int fail(const char *s) {fprintf(stderr,"FAT-V4-FAIL: %s\n",s);return 1;}
int main(int argc,char **argv) {
    const IfsFatfsModuleV4 *api;
    const IfsFatfsModuleV4 *(*resolve)(void)=NULL;
    IfsFatfsIo io;
    IfsFatfsEntry entry;
    struct stat st;
    Disk disk={0};
    uint64_t total,free_before,free_after;
    uint32_t cluster_bytes;
    void *handle;
    int fd;
    const char payload[]="FAT formatter and metadata verified\n";
    uint16_t date=(uint16_t)(((2024u-1980u)<<9)|(2u<<5)|29u);
    uint16_t time=(uint16_t)((14u<<11)|(23u<<5)|28u);
    uint16_t actual_date=0,actual_time=0;
    if(argc!=4 && argc!=5) return fail("arguments: module image sector-bytes [reject]");
    fd=open(argv[2],O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0 || fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0)
        return fail("image must be an existing regular file, never a device or symlink");
    disk.file=fdopen(fd,"r+b");
    if(!disk.file) return fail("open image");
    io.user=&disk;io.partition_bytes=(uint64_t)st.st_size;
    io.sector_bytes=(uint32_t)strtoul(argv[3],NULL,10);
    cluster_bytes=io.sector_bytes;
    if(argc==5 && strcmp(argv[4],"reject")!=0)
        cluster_bytes=(uint32_t)strtoul(argv[4],NULL,10);
    io.read_at=read_at;io.write_at=write_at;io.flush=flush_at;io.fat_timestamp=time_at;
    handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!handle) return fail(dlerror());
    *(void **)(&resolve)=dlsym(handle,"infiltrator_filesystem_module_v4");
    api=resolve?resolve():NULL;
    if(!api || api->abi_version!=4u || api->structure_bytes!=sizeof(*api) ||
       !api->v3 || !api->format || !api->set_attributes || !api->set_modified_time ||
       !api->get_modified_time || !api->space)
        return fail("missing V4 entry points");
    if(argc==5 && strcmp(argv[4],"reject")==0) {
        if(api->format(&io,io.sector_bytes)==0 || disk.writes!=0)
            return fail("wrong-width format did not fail before writing");
        if(fclose(disk.file)!=0 || dlclose(handle)!=0) return fail("close reject fixture");
        puts("FAT-V4-FORMAT-REJECTION-PASS");return 0;
    }
    if(api->format(&io,cluster_bytes)!=0 || disk.writes==0)
        return fail("format image");
    if(api->v3->mount(&io)!=0 || api->space(&total,&free_before)!=0 ||
       !total || total!=free_before ||
       api->v3->mkdir("0:/FORMAT")!=0 || api->v3->create("0:/FORMAT/META.TXT")!=0 ||
       api->v3->write("0:/FORMAT/META.TXT",0,payload,(uint32_t)(sizeof payload-1u))!=0 ||
       api->space(&total,&free_after)!=0 || free_after>=free_before)
        return fail("create and free-space accounting");
    if(api->set_attributes("0:/FORMAT/META.TXT",1u,1u)!=0 ||
       api->v3->stat("0:/FORMAT/META.TXT",&entry)!=0 || !(entry.attributes&1u) ||
       api->v3->write("0:/FORMAT/META.TXT",0,"X",1u)==0 ||
       api->set_attributes("0:/FORMAT/META.TXT",0u,1u)!=0 ||
       api->set_attributes("0:/FORMAT/META.TXT",0u,16u)==0 ||
       api->set_modified_time("0:/FORMAT/META.TXT",(uint16_t)(((2023u-1980u)<<9)|(2u<<5)|29u),time)==0 ||
       api->set_modified_time("0:/FORMAT/META.TXT",date,time)!=0 ||
       api->get_modified_time("0:/FORMAT/META.TXT",&actual_date,&actual_time)!=0 ||
       actual_date!=date || actual_time!=time)
        return fail("attributes or validated modification timestamp");
    if(api->v3->unmount()!=0 || flush_at(&disk)!=0 || fclose(disk.file)!=0 || dlclose(handle)!=0)
        return fail("close formatted image");
    puts("FAT-V4-FORMAT-METADATA-SPACE-PASS");
    return 0;
}
