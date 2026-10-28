/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent DOS-formatted FAT12/FAT16 image interoperability (no devices).
 * Both media files are produced by dosfstools, not the reader under test.
 */
#define _POSIX_C_SOURCE 200809L
#include "fatfs_module.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int read_at(void *u,uint64_t at,void *dst,uint32_t bytes) {
    FILE *f=(FILE *)u;
    if(at>INT64_MAX || fseeko(f,(off_t)at,SEEK_SET)!=0) return -1;
    return fread(dst,1,bytes,f)==bytes ? 0 : -1;
}
static int write_at(void *u,uint64_t at,const void *src,uint32_t bytes) {
    FILE *f=(FILE *)u;
    if(at>INT64_MAX || fseeko(f,(off_t)at,SEEK_SET)!=0) return -1;
    return fwrite(src,1,bytes,f)==bytes ? 0 : -1;
}
static int flush_at(void *u) {
    FILE *f=(FILE *)u;
    return fflush(f)==0 && fsync(fileno(f))==0 ? 0 : -1;
}
static uint32_t gettime_at(void *u) {
    (void)u;
    /* FAT 2026-10-28 12:30:00 local. */
    return ((uint32_t)(2026-1980)<<25) | (10u<<21) | (28u<<16) |
           (12u<<11) | (30u<<5);
}
static int die(const char *msg) {fprintf(stderr,"FAT-IMAGE-FAIL: %s\n",msg);return 1;}
static int setup(const char *path,FILE **f,IfsFatfsIo *io)
{
    struct stat st;
    unsigned char bpb[512];
    int fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0) return -1;
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0) {
        close(fd);
        return -1;
    }
    *f=fdopen(fd,"r+b");
    if(!*f) {close(fd);return -1;}
    if(fread(bpb,1,sizeof bpb,*f)!=sizeof bpb) {fclose(*f);return -1;}
    io->user=*f;
    io->partition_bytes=(uint64_t)st.st_size;
    io->sector_bytes=(uint32_t)bpb[11] | (uint32_t)bpb[12]<<8;
    io->read_at=read_at;
    io->write_at=write_at;
    io->flush=flush_at;
    io->fat_timestamp=gettime_at;
    return 0;
}
int main(int argc,char **argv) {
    const IfsFatfsModuleV3 *api;
    const char *name="0:/SUBDIR/Français Über Naming Example.txt";
    const char *renamed="0:/SUBDIR/Verified Café Ünîcode Data.txt";
    const char *dir="0:/SUBDIR";
    FILE *f=0;
    IfsFatfsIo io;
    IfsFatfsEntry metadata,entry;
    uint8_t data[3072],out[3072];
    char longname[400]="0:/SUBDIR/", longleaf[300];
    void *handle;
    int found=0;
    const IfsFatfsModuleV3 *(*resolve)(void)=0;
    if(argc!=4 && argc!=5) return die("arguments: module own-image other-variant-image [cleanup]");
    handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!handle) {fprintf(stderr,"%s\n",dlerror());return die("dlopen");}
    *(void **)(&resolve)=dlsym(handle,"infiltrator_filesystem_module_v3");
    api=resolve?resolve():0;
    if(!api || api->abi_version!=3u || api->structure_bytes!=sizeof(*api) ||
       !api->mount || !api->unmount || !api->create || !api->read ||
       !api->write || !api->truncate || !api->mkdir ||
       !api->unlink || !api->rename || !api->stat || !api->directory_entry)
        return die("V3 functions not present");
    if(setup(argv[3],&f,&io)!=0) return die("wrong image setup");
    if(api->mount(&io)==0) return die("mounted incorrect FAT variant");
    if(fclose(f)!=0) return die("close wrong media");
    if(setup(argv[2],&f,&io)!=0) return die("own image setup");
    if(api->mount(&io)!=0) return die("cannot mount correctly formatted image");
    if(argc==5 && strcmp(argv[4],"reject")==0) {
        if(api->unmount()!=0 || fclose(f)!=0 || dlclose(handle)!=0)
            return die("close after rejecting foreign format");
        puts("FAT-IMAGE-FOREIGN-REJECTION-PASS");
        return 0;
    }
    if(argc==5 && strcmp(argv[4],"full")==0) {
        /* A FatFs seek on full media may succeed at a clipped position.
         * Both operations must fail, and must not overwrite the original. */
        const char *full="0:/FULL.DAT";
        const uint8_t marker[4]={0x12,0x34,0x56,0x78};
        uint8_t saved[sizeof marker];
        if(io.partition_bytes>=UINT32_MAX || api->create(full)!=0 ||
           api->write(full,0,marker,sizeof marker)!=0 ||
           api->write(full,(uint32_t)io.partition_bytes,marker,1)==0 ||
           api->read(full,0,saved,sizeof saved)!=0 ||
           memcmp(marker,saved,sizeof marker)!=0 || api->unlink(full)!=0 ||
           api->create(full)!=0 || api->write(full,0,marker,sizeof marker)!=0 ||
           api->truncate(full,(uint32_t)io.partition_bytes)==0 ||
           api->read(full,0,saved,sizeof saved)!=0 ||
           memcmp(marker,saved,sizeof marker)!=0 || api->unlink(full)!=0 ||
           api->unmount()!=0 || flush_at(f)!=0 || fclose(f)!=0)
            return die("full-media seek was accepted or changed existing data");
        if(dlclose(handle)!=0) return die("close module after full-media test");
        puts("FAT-IMAGE-FULL-MEDIA-SEEK-PASS");
        return 0;
    }
    /* Ninety three-byte UTF-8 characters exceed 255 bytes while remaining
     * within the FAT limit of 255 UTF-16 code units. */
    for(unsigned i=0;i<90u;i++) memcpy(longleaf+i*3u,"\xe2\x82\xac",3u);
    memcpy(longleaf+270u,".txt",5u);
    strcat(longname,longleaf);
    if(argc==5) {
        if(strcmp(argv[4],"cleanup")!=0 ||
           api->stat(renamed,&metadata)!=0 || metadata.bytes!=1000u ||
           api->stat(longname,&metadata)!=0 || api->unlink(longname)!=0 ||
           api->unlink(renamed)!=0 || api->unlink(dir)!=0 ||
           api->unmount()!=0 || flush_at(f)!=0 || fclose(f)!=0)
            return die("cleanup persisted media");
        if(dlclose(handle)!=0) return die("close module after cleanup");
        return 0;
    }
    if(api->mkdir(dir)!=0) return die("create nested directory");
    if(api->mkdir("0:/SUBDIR/CHILD")!=0 ||
       api->mkdir("0:/SUBDIR/CHILD/DEEP")!=0 ||
       api->rename("0:/SUBDIR","0:/subdir/child/deep/LOST")==0 ||
       api->stat("0:/SUBDIR/CHILD/DEEP",&metadata)!=0 ||
       api->rename("0:/SUBDIR/CHILD","0:/MOVED")!=0 ||
       api->rename("0:/MOVED","0:/SUBDIR/CHILD")!=0 ||
       api->unlink("0:/SUBDIR/CHILD/DEEP")!=0 ||
       api->unlink("0:/SUBDIR/CHILD")!=0)
        return die("directory ancestry protection or valid reparent");
    if(api->mkdir("0:/SUBDIR/Long Directory Name")!=0 ||
       api->mkdir("0:/SUBDIR/Long Directory Name/CHILD")!=0 ||
       api->stat("0:/SUBDIR/LONGDI~1/CHILD",&metadata)!=0 ||
       api->rename("0:/SUBDIR/Long Directory Name",
                   "0:/subdir/longdi~1/child/LOST")==0 ||
       api->stat("0:/SUBDIR/Long Directory Name/CHILD",&metadata)!=0 ||
       api->unlink("0:/SUBDIR/Long Directory Name/CHILD")!=0 ||
       api->unlink("0:/SUBDIR/Long Directory Name")!=0)
        return die("directory ancestry through short-name alias");
    if(api->create(longname)!=0 || api->stat(longname,&metadata)!=0 ||
       strcmp(metadata.name,longleaf)!=0) return die("UTF-8 long filename stat");
    /* Extending a file, including a gap created by a seek, must expose
     * zeroes instead of residual bytes from previously free clusters. */
    if(api->create("0:/SUBDIR/GAP.DAT")!=0 ||
       api->write("0:/SUBDIR/GAP.DAT",1300u,"Z",1u)!=0 ||
       api->read("0:/SUBDIR/GAP.DAT",0u,out,1301u)!=0)
        return die("create sparse-style gap");
    for(unsigned i=0;i<1300u;i++) if(out[i]!=0) return die("gap is not zero-filled");
    if(out[1300]!='Z' || api->truncate("0:/SUBDIR/GAP.DAT",5u)!=0 ||
       api->truncate("0:/SUBDIR/GAP.DAT",2049u)!=0 ||
       api->read("0:/SUBDIR/GAP.DAT",0u,out,2049u)!=0)
        return die("truncate shrink and extend");
    for(unsigned i=0;i<2049u;i++) if(out[i]!=0) return die("extension is not zero-filled");
    if(api->unlink("0:/SUBDIR/GAP.DAT")!=0) return die("remove gap fixture");
    if(api->create(name)!=0) return die("create long-name file");
    for(unsigned i=0;i<sizeof data;i++) data[i]=(uint8_t)((i*17u+23u)&255u);
    if(api->write(name,0u,data,2048u)!=0 ||
       api->write(name,2048u,data+2048u,1024u)!=0)
        return die("write/allocate/append");
    memset(out,0,sizeof out);
    if(api->read(name,0,out,sizeof out)!=0 ||
       memcmp(out,data,sizeof data)!=0) return die("immediate readback");
    if(api->rename(name,renamed)!=0 || api->stat(renamed,&metadata)!=0 ||
       metadata.bytes!=3072u) return die("rename and stat");
    for(uint32_t i=0;i<64u;++i) {
        const int r=api->directory_entry(dir,i,&entry);
        if(r==1) break;
        if(r!=0) return die("directory enumeration failed");
        if(strcmp(entry.name,"Verified Café Ünîcode Data.txt")==0) found|=1;
        if(strcmp(entry.name,longleaf)==0) found|=2;
    }
    if(found!=3) return die("long filename not independently enumerated");
    if(api->unmount()!=0 || flush_at(f)!=0) return die("unmount/flush");
    if(api->mount(&io)!=0) return die("remount same image");
    memset(out,0,sizeof out);
    if(api->read(renamed,0u,out,sizeof out)!=0 ||
       memcmp(out,data,sizeof data)!=0) return die("remount persistence");
    if(api->truncate(renamed,1000u)!=0 ||
       api->stat(renamed,&metadata)!=0 || metadata.bytes!=1000u)
        return die("truncate");
    if(api->unmount()!=0 || api->mount(&io)!=0 ||
       api->stat(renamed,&metadata)!=0 || metadata.bytes!=1000u)
        return die("truncate persistence");
    /* Preserve test data for independent mtools/fsck validation; cleanup
     * is a separate process, not a self-verifier hiding on-disk errors. */
    if(api->unmount()!=0 || flush_at(f)!=0 || fclose(f)!=0)
        return die("final flush");
    if(dlclose(handle)!=0) return die("close module");
    printf("FAT12-FAT16-CROSS-FORMAT-IMAGE-READ-WRITE-PASS: %s\n",argv[1]);
    return 0;
}
