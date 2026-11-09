/* SPDX-License-Identifier: GPL-3.0-or-later
 * Generic FUSE3/VFS adapter for the single canonical filesystem .so API.
 * Zero FAT12 format interpretation. Each process hosts one independent
 * volume; many mounts use many processes with the same authoritative .so.
 * Physical block-device access is read-only; writes require an explicitly
 * selected regular-file image, never an implicit format or fallback.
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <infiltratr/filesystem_module.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    int fd,mod_fd,writable;
    uint64_t bytes;
    void *lib,*volume;
    const IfsFsModuleApiV1 *api;
    IfsFsHostIoV1 io;
    IfsFsVolumeInfoV1 info;
    uid_t uid;
    gid_t gid;
} Volume;
static Volume v={.fd=-1,.mod_fd=-1};

static int errcode(int rc) {
    switch(rc) {
        case IFS_FS_OK:return 0;
        case IFS_FS_END: case IFS_FS_NOT_FOUND:return -ENOENT;
        case IFS_FS_INVALID:return -EINVAL;
        case IFS_FS_CORRUPT:return -EUCLEAN;
        case IFS_FS_NO_SPACE:return -ENOSPC;
        case IFS_FS_EXISTS:return -EEXIST;
        case IFS_FS_ACCESS:return -EACCES;
        case IFS_FS_BUSY:return -EBUSY;
        case IFS_FS_NO_MEMORY:return -ENOMEM;
        case IFS_FS_UNSUPPORTED:return -EOPNOTSUPP;
        default:return -EIO;
    }
}
static int within(uint64_t offset,uint32_t size) {
    return offset<=v.bytes && size<=v.bytes-offset &&
           offset<=INT64_MAX && size<=INT64_MAX-offset;
}
static int io_read(void *unused,uint64_t offset,void *p,uint32_t n) {
    (void)unused;
    return p && within(offset,n) &&
           pread(v.fd,p,n,(off_t)offset)==(ssize_t)n?0:-1;
}
static int io_write(void *unused,uint64_t offset,const void *p,uint32_t n) {
    (void)unused;
    return v.writable && p && within(offset,n) &&
           pwrite(v.fd,p,n,(off_t)offset)==(ssize_t)n?0:-1;
}
static int io_flush(void *unused) {
    (void)unused;
    return fsync(v.fd)==0?0:-1;
}
static void shutdown_volume(void) {
    if(v.volume && v.api) (void)v.api->unmount(v.volume);
    v.volume=NULL;
    if(v.lib)dlclose(v.lib);
    v.lib=NULL;v.api=NULL;
    if(v.mod_fd>=0)close(v.mod_fd);
    v.mod_fd=-1;
    if(v.fd>=0)close(v.fd);
    v.fd=-1;
}
static int init_volume(const char *module,const char *media,int writable,int block) {
    struct stat st={0},mod={0};
    unsigned long long count=0;
    void *symbol=NULL;
    IfsFsModuleApiEntryV1 entry=NULL;
    if(v.fd>=0)return -EBUSY;
    /* No physical writes, even for root. Only a trusted installed FAT12.so
     * may be used for block-device reads. */
    if(block && (writable || geteuid()!=0 ||
        strcmp(module,"/System/Filesystems/FAT12.so")))return -EACCES;
    int flags=(writable?O_RDWR:O_RDONLY)|O_CLOEXEC|O_NOFOLLOW;
    if(block)flags|=O_EXCL;
    v.fd=open(media,flags);
    if(v.fd<0)return -errno;
    if(fstat(v.fd,&st)!=0 ||
       (block?!S_ISBLK(st.st_mode):!S_ISREG(st.st_mode))) {
        shutdown_volume();return -EINVAL;
    }
    if(block) {
        if(ioctl(v.fd,BLKGETSIZE64,&count)!=0 || !count) {
            shutdown_volume();return -EINVAL;
        }
        v.bytes=count;
    } else {
        if(st.st_size<0) {shutdown_volume();return -EINVAL;}
        v.bytes=(uint64_t)st.st_size;
        if(flock(v.fd,(writable?LOCK_EX:LOCK_SH)|LOCK_NB)!=0) {
            int e=-errno;shutdown_volume();return e;
        }
    }
    if(v.bytes<1024u || v.bytes>INT64_MAX) {
        shutdown_volume();return -EINVAL;
    }
    v.writable=writable;
    v.uid=getuid();v.gid=getgid();
    v.mod_fd=open(module,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(v.mod_fd<0 || fstat(v.mod_fd,&mod)!=0 || !S_ISREG(mod.st_mode) ||
       (block && (mod.st_uid!=0 || (mod.st_mode&0022u)))) {
        shutdown_volume();return -EACCES;
    }
    char path[64];
    if(snprintf(path,sizeof path,"/proc/self/fd/%d",v.mod_fd)>=(int)sizeof path) {
        shutdown_volume();return -EINVAL;
    }
    v.lib=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!v.lib) {shutdown_volume();return -ELIBBAD;}
    symbol=dlsym(v.lib,IFS_FS_COMMON_EXPORT_V1);
    if(!symbol || sizeof(symbol)!=sizeof(entry)) {
        shutdown_volume();return -ELIBBAD;
    }
    memcpy(&entry,&symbol,sizeof entry);
    v.api=entry?entry():NULL;
    const uint64_t needs=IFS_FS_CAP_PROBE|IFS_FS_CAP_MOUNT|
        IFS_FS_CAP_LIST|IFS_FS_CAP_READ|IFS_FS_CAP_WRITE|
        IFS_FS_CAP_CREATE|IFS_FS_CAP_REMOVE|IFS_FS_CAP_RENAME|
        IFS_FS_CAP_TRUNCATE|IFS_FS_CAP_CHECK;
    if(!ifs_fs_common_api_valid(v.api,"fat12") ||
       (v.api->capabilities&needs)!=needs) {
        shutdown_volume();return -ELIBBAD;
    }
    v.io=(IfsFsHostIoV1){0};
    v.io.structure_bytes=sizeof v.io;
    v.io.flags=writable?IFS_FS_IO_EXCLUSIVE:IFS_FS_IO_READ_ONLY;
    v.io.context=&v;
    v.io.volume_bytes=v.bytes;
    v.io.read_at=io_read;
    v.io.write_at=writable?io_write:NULL;
    v.io.flush=writable?io_flush:NULL;
    return 0;
}
static int mount_volume(void) {
    uint32_t width=0;
    if(v.api->probe(&v.io,&width)!=IFS_FS_OK || width!=12u)return -ENODEV;
    v.info=(IfsFsVolumeInfoV1){0};
    v.info.structure_bytes=sizeof v.info;
    int rc=v.api->mount(&v.io,&v.volume,&v.info);
    if(rc!=IFS_FS_OK || !v.volume || v.info.variant!=12u)
        return rc==IFS_FS_OK?-EIO:errcode(rc);
    return 0;
}
static int lookup(const char *path,IfsFsEntryV1 *e,char name[768]) {
    *e=(IfsFsEntryV1){0};
    e->structure_bytes=sizeof *e;
    e->name=name;e->name_capacity=768u;
    return v.api->stat(v.volume,path,e);
}
static void as_stat(const IfsFsEntryV1 *e,struct stat *s) {
    memset(s,0,sizeof *s);
    const int dir=e->type==IFS_FS_ENTRY_DIRECTORY;
    s->st_mode=(dir?S_IFDIR:S_IFREG)|
        (dir?(v.writable?0755:0555):(v.writable?0644:0444));
    s->st_uid=v.uid;s->st_gid=v.gid;
    s->st_nlink=dir?2:1;
    s->st_size=(off_t)e->size_bytes;
    s->st_blksize=(blksize_t)(v.info.block_bytes?v.info.block_bytes:512u);
    s->st_blocks=(blkcnt_t)((e->size_bytes+511u)/512u);
}
static int f_getattr(const char *path,struct stat *s,struct fuse_file_info *fi) {
    (void)fi;IfsFsEntryV1 e;char name[768];
    int rc=lookup(path,&e,name);
    if(rc!=IFS_FS_OK)return errcode(rc);
    as_stat(&e,s);return 0;
}
static int f_readdir(const char *path,void *buf,fuse_fill_dir_t fill,
                      off_t off,struct fuse_file_info *fi,enum fuse_readdir_flags flags) {
    (void)fi;(void)flags;
    IfsFsEntryV1 root;char root_name[768];
    int rc=lookup(path,&root,root_name);
    if(rc!=IFS_FS_OK)return errcode(rc);
    if(root.type!=IFS_FS_ENTRY_DIRECTORY)return -ENOTDIR;
    if(off<0 || (uint64_t)off>UINT32_MAX)return -EINVAL;
    if(off==0) {
        if(fill(buf,".",NULL,1,0))return 0;
        off=1;
    }
    if(off==1) {
        if(fill(buf,"..",NULL,2,0))return 0;
        off=2;
    }
    uint32_t index=(uint32_t)off-2u;
    for(;index<UINT32_MAX;++index) {
        IfsFsEntryV1 e;char name[768]={0};
        e=(IfsFsEntryV1){0};e.structure_bytes=sizeof e;
        e.name=name;e.name_capacity=sizeof name;
        rc=v.api->directory_entry(v.volume,path,index,&e);
        if(rc==IFS_FS_END)return 0;
        if(rc!=IFS_FS_OK)return errcode(rc);
        if(!name[0] || strchr(name,'/') || !strcmp(name,".") ||
           !strcmp(name,".."))return -EUCLEAN;
        struct stat s;as_stat(&e,&s);
        if(fill(buf,name,&s,(off_t)index+3,0))return 0;
    }
    return -EOVERFLOW;
}
static int f_open(const char *path,struct fuse_file_info *fi) {
    IfsFsEntryV1 e;char name[768];
    int rc=lookup(path,&e,name);
    if(rc!=IFS_FS_OK)return errcode(rc);
    if(e.type!=IFS_FS_ENTRY_FILE)return -EISDIR;
    if(!v.writable && (fi->flags&O_ACCMODE)!=O_RDONLY)return -EROFS;
    fi->direct_io=1;return 0;
}
static int f_read(const char *path,char *buf,size_t size,off_t off,struct fuse_file_info *fi) {
    (void)fi;
    if(off<0 || size>UINT32_MAX || size>INT_MAX)return -EINVAL;
    IfsFsEntryV1 e;char name[768];
    int rc=lookup(path,&e,name);
    if(rc!=IFS_FS_OK)return errcode(rc);
    if(e.type!=IFS_FS_ENTRY_FILE)return -EISDIR;
    if((uint64_t)off>=e.size_bytes)return 0;
    if(size>e.size_bytes-(uint64_t)off)size=(size_t)(e.size_bytes-(uint64_t)off);
    rc=v.api->read(v.volume,path,(uint64_t)off,buf,(uint32_t)size);
    return rc==IFS_FS_OK?(int)size:errcode(rc);
}
static int f_write(const char *path,const char *buf,size_t size,off_t off,
                   struct fuse_file_info *fi) {
    (void)fi;
    if(!v.writable)return -EROFS;
    if(off<0 || size>UINT32_MAX || size>INT_MAX)return -EFBIG;
    int rc=v.api->write(v.volume,path,(uint64_t)off,buf,(uint32_t)size);
    return rc==IFS_FS_OK?(int)size:errcode(rc);
}
static int f_create(const char *path,mode_t mode,struct fuse_file_info *fi) {
    (void)mode;
    if(!v.writable)return -EROFS;
    int rc=v.api->create(v.volume,path,IFS_FS_ENTRY_FILE);
    if(rc==IFS_FS_OK)fi->direct_io=1;
    return errcode(rc);
}
static int f_mkdir(const char *path,mode_t mode) {
    (void)mode;
    return v.writable?errcode(v.api->create(v.volume,path,IFS_FS_ENTRY_DIRECTORY)):-EROFS;
}
static int f_unlink(const char *path) {
    return v.writable?errcode(v.api->remove(v.volume,path)):-EROFS;
}
static int f_rename(const char *old,const char *newpath,unsigned flags) {
    if(!v.writable)return -EROFS;
    if(flags)return -EOPNOTSUPP;
    return errcode(v.api->rename(v.volume,old,newpath));
}
static int f_truncate(const char *path,off_t n,struct fuse_file_info *fi) {
    (void)fi;
    if(!v.writable)return -EROFS;
    if(n<0)return -EINVAL;
    return errcode(v.api->truncate(v.volume,path,(uint64_t)n));
}
static int f_fsync(const char *path,int data,struct fuse_file_info *fi) {
    (void)path;(void)data;(void)fi;
    return io_flush(NULL)==0?0:-EIO;
}
static const struct fuse_operations ops={
    .getattr=f_getattr,.readdir=f_readdir,.open=f_open,
    .read=f_read,.write=f_write,.create=f_create,
    .mkdir=f_mkdir,.unlink=f_unlink,.rmdir=f_unlink,
    .rename=f_rename,.truncate=f_truncate,.fsync=f_fsync
};
static int find_file(void *data,const char *name,const struct stat *s,
                     off_t off,enum fuse_fill_dir_flags flags) {
    (void)s;(void)off;(void)flags;
    if(!strcmp(name,"FUSEREN.TXT"))*(int *)data=1;
    return 0;
}
static int find_parent(void *data,const char *name,const struct stat *s,
                       off_t off,enum fuse_fill_dir_flags flags) {
    (void)s;(void)off;(void)flags;
    if(!strcmp(name,".."))*(int *)data=1;
    return 0;
}
/* The destructive diagnostic accepts only a wholly empty regular-file image.
 * Scan every byte, not merely the BPB/signatures: a sparse or hidden payload
 * must never be destroyed by mistaking a populated image for disposable media.
 * The acquired descriptor is exclusively locked by init_volume. */
static int image_is_empty(int fd,uint64_t bytes) {
    unsigned char data[65536];
    for(uint64_t at=0;at<bytes;) {
        const uint32_t n=(uint32_t)((bytes-at)<sizeof data?
            (bytes-at):sizeof data);
        if(pread(fd,data,n,(off_t)at)!=(ssize_t)n)return 0;
        for(uint32_t i=0;i<n;++i)if(data[i]!=0u)return 0;
        at+=n;
    }
    return 1;
}
static int selftest(const char *module,const char *image) {
    struct stat st;
    if(lstat(image,&st)!=0 || !S_ISREG(st.st_mode) ||
       st.st_size<1024*1024)return 2;
    int rc=init_volume(module,image,1,0);
    if(rc)goto failure;
    if(!image_is_empty(v.fd,v.bytes)) {
        fputs("Refusing destructive self-test: image contains data or cannot be fully verified empty\n",stderr);
        shutdown_volume();return 2;
    }
    /* Only an exclusively opened, byte-for-byte empty regular image
     * may be formatted by the explicit diagnostic. Normal mounts never format. */
    IfsFsHostFormatIoV1 ext={0};
    ext.host=v.io;ext.host.structure_bytes=sizeof ext;
    ext.logical_sector_bytes=512u;
    if(!v.api->format ||
       v.api->format(&ext.host,12u,NULL)!=IFS_FS_OK ||
       io_flush(NULL)!=0)goto failure;
    if((rc=mount_volume())!=0)goto failure;
    struct stat attr;
    struct fuse_file_info fi={.flags=O_RDWR};
    const char *a="/FUSETEST.TXT",*b="/FUSEREN.TXT";
    const char msg[]="canonical filesystem FUSE roundtrip";
    char got[sizeof msg]={0};
    int seen=0,parent_seen=0;
    uint32_t errors=UINT32_MAX;
    if(f_getattr("/",&attr,NULL)!=0 || !S_ISDIR(attr.st_mode) ||
       f_create(a,0644,&fi)!=0 ||
       f_write(a,msg,sizeof msg-1u,0,&fi)!=(int)sizeof msg-1 ||
       f_read(a,got,sizeof msg-1u,0,&fi)!=(int)sizeof msg-1 ||
       memcmp(got,msg,sizeof msg-1u) ||
       f_rename(a,b,0u)!=0 ||
       f_readdir("/",&seen,find_file,0,NULL,0)!=0 || !seen ||
       f_readdir("/",&parent_seen,find_parent,1,NULL,0)!=0 || !parent_seen ||
       f_truncate(b,9,NULL)!=0 ||
       f_getattr(b,&attr,NULL)!=0 || attr.st_size!=9 ||
       f_unlink(b)!=0 ||
       v.api->check(v.volume,&errors)!=IFS_FS_OK || errors!=0u)goto failure;
    shutdown_volume();
    puts("IFS-FUSE-CANONICAL-MOUNT-OPERATIONS-PASS");
    return 0;
failure:
    fprintf(stderr,"IFS-FUSE-HOST-FAIL: %d\n",rc);
    shutdown_volume();
    return 1;
}
static void help(void) {
    puts("ifs-fuse-host --mount-ro-image MODULE IMAGE MOUNTPOINT");
    puts("ifs-fuse-host --mount-rw-image MODULE IMAGE MOUNTPOINT");
    puts("ifs-fuse-host --mount-ro-device /System/Filesystems/FAT12.so PARTITION MOUNTPOINT");
    puts("ifs-fuse-host --self-test MODULE DISPOSABLE-IMAGE");
    puts("One foreground process per mount; no physical-device writes.");
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--help")) {help();return 0;}
    if(argc==4 && !strcmp(argv[1],"--self-test"))
        return selftest(argv[2],argv[3]);
    if(argc!=5) {help();return 2;}
    int block=!strcmp(argv[1],"--mount-ro-device");
    int writable=!strcmp(argv[1],"--mount-rw-image");
    if(!block && !writable && strcmp(argv[1],"--mount-ro-image")) {
        help();return 2;
    }
    struct stat st;
    if(lstat(argv[4],&st)!=0 || !S_ISDIR(st.st_mode)) {
        fputs("Mountpoint must be an existing real directory\n",stderr);
        return 2;
    }
    int rc=init_volume(argv[2],argv[3],writable,block);
    if(rc==0)rc=mount_volume();
    if(rc) {
        fprintf(stderr,"FAT12.so mount admission rejected: %d\n",rc);
        shutdown_volume();return 1;
    }
    /* Single thread: the canonical FAT12 engine has one global session.
     * A second volume is served by a separate daemon, not an extra ABI. */
    char *av[]={(char *)"ifs-fuse-host",(char *)"-f",(char *)"-s",
                (char *)"-o",(char *)"default_permissions,fsname=infiltrator-fat12",
                argv[4],NULL};
    rc=fuse_main(6,av,&ops,NULL);
    shutdown_volume();
    return rc;
}
