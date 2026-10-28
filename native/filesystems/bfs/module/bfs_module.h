/* SPDX-License-Identifier: GPL-2.0-only */
/* SCO UnixWare BFS. Project-owned, single-file, freestanding BFS.so.
 * Disk layout follows the original BFS Linux VFS source in ../linux/.
 * Not the BeOS/Haiku BeFS format.
 */
#ifndef INFILTRATR_BFS_DROPIN_H
#define INFILTRATR_BFS_DROPIN_H
#include <stdint.h>

#define IFS_BFS_ABI_VERSION 1u
#define IFS_BFS_CAP_PROBE UINT64_C(1)
#define IFS_BFS_CAP_ROOT_DIRECTORY UINT64_C(2)
#define IFS_BFS_CAP_FILE_READ UINT64_C(4)
#define IFS_BFS_CAP_EXISTING_WRITE UINT64_C(8)
#define IFS_BFS_BLOCK_BYTES 512u
#define IFS_BFS_MAX_NAME 14u
#define IFS_BFS_ROOT_INODE 2u
#define IFS_BFS_TYPE_FILE 1u
#define IFS_BFS_TYPE_DIRECTORY 2u

typedef int (*IfsBfsReadAt)(void *user, uint64_t offset,
                             void *buffer, uint32_t bytes);
typedef int (*IfsBfsWriteAt)(void *user, uint64_t offset,
                              const void *buffer, uint32_t bytes);
typedef int (*IfsBfsFlush)(void *user);

typedef struct IfsBfsIo {
    IfsBfsReadAt read_at;
    IfsBfsWriteAt write_at; /* optional, must be paired with flush */
    IfsBfsFlush flush;
    void *user;
    uint64_t device_bytes; /* relative to beginning of partition */
} IfsBfsIo;

typedef struct IfsBfsVolume {
    IfsBfsIo io;
    uint64_t volume_bytes;
    uint32_t data_start_block;
    uint32_t total_blocks;
    uint32_t max_inode;
    int clean;
} IfsBfsVolume;

typedef struct IfsBfsFile {
    uint32_t inode_number;
    uint32_t type;
    uint32_t first_block;
    uint32_t last_block;
    uint32_t size_bytes;
    char name[IFS_BFS_MAX_NAME + 1u];
} IfsBfsFile;

typedef struct IfsBfsModule {
    uint32_t abi_version;
    uint32_t structure_bytes;
    const char *format_id;
    const char *format_name;
    uint64_t capabilities;
    /* 0=success, -1=invalid/unsupported/corrupt media. */
    int (*mount)(IfsBfsVolume *, const IfsBfsIo *);
    /* nth visible entry: 0=success, 1=end, -1=error */
    int (*root_file)(const IfsBfsVolume *, uint32_t nth, IfsBfsFile *);
    /* Case-sensitive; exact file name. 0=found, 1=not found, -1=error */
    int (*lookup)(const IfsBfsVolume *, const char *, uint32_t,
                  IfsBfsFile *);
    int (*read)(const IfsBfsVolume *, const IfsBfsFile *, uint64_t,
                void *, uint32_t);
    /* In-place overwrite only; requires clean volume, writer and flush. */
    int (*overwrite_existing)(const IfsBfsVolume *, const IfsBfsFile *,
                              uint64_t, const void *, uint32_t);
} IfsBfsModule;

const IfsBfsModule *infiltrator_filesystem_module_v1(void);
#endif
