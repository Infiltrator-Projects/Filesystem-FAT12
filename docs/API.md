# FAT12 single authoritative filesystem API

The only public **filesystem** entry point in `/System/Filesystems/FAT12.so` is
`infiltrator_filesystem_api_v1`, defined by the Common/Libraries header
`infiltratr/filesystem_module.h`. The “v1” names the **current canonical ABI
revision**, not support for previous FAT12 implementations. All FAT12
operations—probe, mount, stat/list, create, read, write, rename, truncate,
remove, format, check and verified FAT mirror recovery—are exposed through
this single table. **The V1/V3/V4 FAT12 legacy tables and binary exports
are removed**, including their public headers.

`infiltrator_filesystem_boot_v1` is a separate purpose-specific read-only
Boot host contract. `infiltrator_filesystem_defrag_v1` is a separately
audited Defragmenter host contract. Neither is a historical FAT12
compatibility interface; both reside within this same shared-object file.

The production read/write/format engine is the single FatFs implementation.
The Common adapter calls its internal operations directly. The bounded
FAT12 geometry and FAT mirror scanner remains solely for independent
consistency validation and restricted secondary FAT repair; the previous
8.3 directory/file reader and its export have been deleted.

The module allows only **one active mount per process** and operations must
be serialised. Mutations require host-verified exclusive media ownership.
Blank-device formatting requires an explicitly verified logical sector size.
Formatting is not atomic on failure; mirror repair requires a valid
primary FAT and never repairs arbitrary orphaned clusters or cross-links.

Generic kernel VFS execution, simultaneous volume sessions and complete
power-failure recovery are independent **unfinished** engineering goals.

Consumers must load the Common filesystem export; no old V1/V3/V4
filesystem lookup or compatibility fallback is permitted.

For FAT12 formatting, allocation-unit selection trials are *non-destructive* and
bounded, probing FAT12's strict cluster-count threshold before any actual media
write. The engine can choose 64 KiB clusters for a 255 MiB FAT12 partition
without silently formatting it as FAT16. The 255 MiB image is tested in CI.

## Native kernel integration boundary

The installed `/System/Filesystems/FAT12.so` is the canonical filesystem engine.
A kernel-owned generic loader must validate and load this exact ELF artifact,
execute the canonical `infiltrator_filesystem_api_v1` interface in kernel
context, provide bounded partition-relative block I/O and translate the API
into native VFS operations. No userspace filesystem transport or parallel
FAT12 reader is permitted. The legacy kernel-specific filesystem module is
transitional; the direct ELF loader and full kernel mount qualification remain
unfinished.

Formatting continues to be performed only by the canonical SO, under
exclusive media ownership and the existing physical-removable-media safety
checks. Normal mounting must never format any medium.
