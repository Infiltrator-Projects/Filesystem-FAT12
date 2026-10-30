/* SPDX-License-Identifier: GPL-3.0-or-later */
/* FAT12 read-only Boot adapter: reuse the same native FatFs engine as V3/V4. */
#include "fatfs_module.h"
#include "ff.h"
#include "infiltratr/fs/boot_module.h"

#if IFS_FATFS_ONLY == 16
#define BOOT_FORMAT "fat16"
#define BOOT_NAME "FAT16"
#elif IFS_FATFS_ONLY == 12
#define BOOT_FORMAT "fat12"
#define BOOT_NAME "FAT12"
#else
#error "Select one filesystem implementation"
#endif

__attribute__((used, section(".ifsboot")))
static const IfsBootManifest manifest = {
    {'I','F','S','B','O','O','T','1'}, sizeof(IfsBootManifest), IFS_BOOT_ABI,
    62u, IFS_BOOT_PROFILE_X64, BOOT_FORMAT, BOOT_NAME
};

typedef struct {
    FIL file;
    DIR directory;
    uint32_t directory_index;
    int valid, is_directory;
} BootFile;
static IfsBootIo boot_io;
static int boot_mounted;
static uint32_t open_files;

static int read_at(void *unused, uint64_t offset, void *buffer, uint32_t bytes) {
    (void)unused;
    if (offset > boot_io.media_bytes || bytes > boot_io.media_bytes-offset)
        return -1;
    return boot_io.read_at(boot_io.context, offset, buffer, bytes);
}
static int deny_write(void *u, uint64_t p, const void *b, uint32_t n) {
    (void)u; (void)p; (void)b; (void)n; return -1;
}
static int no_flush(void *u) { (void)u; return 0; }
static uint32_t no_time(void *u) { (void)u; return 0; }
static int result(FRESULT status) {
    if (status == FR_OK) return IFS_BOOT_OK;
    if (status == FR_NO_FILE || status == FR_NO_PATH) return IFS_BOOT_NOT_FOUND;
    if (status == FR_DISK_ERR || status == FR_NOT_READY) return IFS_BOOT_IO_ERROR;
    return IFS_BOOT_INVALID;
}
static int IFS_BOOT_CALL boot_mount(const IfsBootIo *io) {
    uint8_t bpb[512];
    if (!io || io->structure_bytes != sizeof(*io) || !io->read_at || boot_mounted ||
        io->media_bytes < sizeof(bpb))
        return IFS_BOOT_INVALID;
    if (io->read_at(io->context, 0, bpb, sizeof(bpb)) != 0)
        return IFS_BOOT_IO_ERROR;
    /* BlockIo describes device blocks; FAT's BPB describes filesystem
     * sectors. The generic host already supplies arbitrary bounded bytes. */
    uint32_t sector_bytes = (uint32_t)bpb[11] | (uint32_t)bpb[12] << 8;
    if (sector_bytes < 512u || sector_bytes > 4096u ||
        (sector_bytes & (sector_bytes - 1u))) return IFS_BOOT_INVALID;
    boot_io = *io;
    IfsFatfsIo device = {0, io->media_bytes, sector_bytes,
                         read_at, deny_write, no_flush, no_time};
    if (ifs_fatfs_boot_attach(&device) != 0)
        return IFS_BOOT_INVALID;
    boot_mounted = 1;
    return IFS_BOOT_OK;
}
static int IFS_BOOT_CALL boot_unmount(void) {
    if (!boot_mounted || open_files) return IFS_BOOT_INVALID;
    if (infiltrator_filesystem_module_v3()->unmount() != 0)
        return IFS_BOOT_IO_ERROR;
    boot_mounted = 0;
    return IFS_BOOT_OK;
}
static void info_copy(IfsBootInfo *out, const FILINFO *in) {
    out->bytes = in->fsize;
    out->attributes = in->fattrib;
    out->reserved = 0;
    unsigned i = 0;
    while (i+1u < sizeof(out->name) && in->fname[i]) {
        out->name[i] = in->fname[i]; ++i;
    }
    out->name[i] = 0;
}
static int IFS_BOOT_CALL boot_open(const char *path, void *context, IfsBootInfo *info) {
    BootFile *file = context;
    FILINFO metadata = {0};
    if (!boot_mounted || !path || !file || !info || file->valid || open_files >= 256u)
        return IFS_BOOT_INVALID;
    *file = (BootFile){0};
    /* An absolute root has no directory entry for f_stat to inspect. */
    if (path[0] == '/' && path[1] == 0) {
        metadata.fattrib = AM_DIR;
        metadata.fname[0] = '/';
    } else {
        int status = result(f_stat(path, &metadata));
        if (status != IFS_BOOT_OK) return status;
    }
    file->is_directory = (metadata.fattrib & AM_DIR) != 0;
    FRESULT status = file->is_directory ? f_opendir(&file->directory, path) :
                                        f_open(&file->file, path, FA_READ);
    if (status != FR_OK) return result(status);
    info_copy(info, &metadata);
    file->valid = 1;
    ++open_files;
    return IFS_BOOT_OK;
}
static int IFS_BOOT_CALL boot_close(void *context) {
    BootFile *file = context;
    if (!file || !file->valid || !boot_mounted) return IFS_BOOT_INVALID;
    FRESULT status = file->is_directory ? f_closedir(&file->directory) :
                                        f_close(&file->file);
    file->valid = 0;
    --open_files;
    return result(status);
}
static int IFS_BOOT_CALL boot_read(void *context, uint64_t offset, void *buffer,
                                  uint32_t bytes, uint32_t *got) {
    BootFile *file = context;
    UINT count = 0;
    if (got) *got = 0;
    if (!boot_mounted || !file || !file->valid || file->is_directory || !got ||
        (!buffer && bytes) || offset > UINT32_MAX || offset > f_size(&file->file))
        return IFS_BOOT_INVALID;
    FRESULT status = FR_OK;
    if (f_tell(&file->file) != (FSIZE_t)offset)
        status = f_lseek(&file->file, (FSIZE_t)offset);
    if (status != FR_OK || f_tell(&file->file) != (FSIZE_t)offset)
        return IFS_BOOT_IO_ERROR;
    status = f_read(&file->file, buffer, bytes, &count);
    *got = count;
    return result(status);
}
static int IFS_BOOT_CALL boot_directory(void *context, uint32_t index,
                                       IfsBootInfo *info) {
    BootFile *file = context;
    FILINFO metadata;
    if (!boot_mounted || !file || !file->valid || !file->is_directory || !info)
        return IFS_BOOT_INVALID;
    if (index < file->directory_index) {
        if (f_readdir(&file->directory, 0) != FR_OK) return IFS_BOOT_IO_ERROR;
        file->directory_index = 0;
    }
    /* A corrupt index cannot create unbounded firmware work. FAT12 has a
     * finite number of directory entries; f_readdir also bounds its chain. */
    while (file->directory_index <= index) {
        FRESULT status = f_readdir(&file->directory, &metadata);
        if (status != FR_OK) return result(status);
        if (!metadata.fname[0]) return IFS_BOOT_END;
        ++file->directory_index;
    }
    info_copy(info, &metadata);
    return IFS_BOOT_OK;
}
static int IFS_BOOT_CALL boot_label(char *out, uint32_t bytes) {
    char label[48];
    if (!boot_mounted || !out || !bytes) return IFS_BOOT_INVALID;
    FRESULT status = f_getlabel("0:", label, 0);
    if (status != FR_OK) return result(status);
    uint32_t i=0;
    while (label[i]) {
        if (i+1u>=bytes || i+1u>=sizeof(label)) return IFS_BOOT_INVALID;
        out[i]=label[i]; ++i;
    }
    out[i]=0;
    return IFS_BOOT_OK;
}
static const IfsBootModuleV1 api = {
    IFS_BOOT_ABI, sizeof(IfsBootModuleV1), BOOT_FORMAT, sizeof(BootFile), 0,
    boot_mount, boot_unmount, boot_open, boot_close, boot_read, boot_directory,
    boot_label
};
__attribute__((visibility("default")))
const IfsBootModuleV1 *IFS_BOOT_CALL infiltrator_filesystem_boot_v1(void) {
    return &api;
}
