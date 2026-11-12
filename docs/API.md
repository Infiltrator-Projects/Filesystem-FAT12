# FAT12 single authoritative filesystem API

Applications and BOOT use `infiltrator_filesystem_api_v2`, defined by
Common's `infiltratr/filesystem_module_v2.h`. The module supplies probe,
mount, directory and file operations, formatting, integrity checking and
restricted FAT mirror recovery. BOOT supplies a read-only host. The same
file contains the dedicated Defragmenter interface for the qualified
relocation and journal recovery operations.

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

For FAT12 formatting, allocation-unit selection trials are *non-destructive* and
bounded, probing FAT12's strict cluster-count threshold before any actual media
write. The engine can choose 64 KiB clusters for a 255 MiB FAT12 partition
without silently formatting it as FAT16. The 255 MiB image is tested in CI.

## Native kernel integration boundary

The installed `/System/Filesystems/FAT12.so` is the canonical filesystem engine.
A kernel-owned generic loader must validate and load this exact ELF artifact,
execute the canonical `infiltrator_filesystem_api_v2` interface in kernel
context, provide bounded partition-relative block I/O and translate the API
into native VFS operations. No userspace filesystem transport or parallel
FAT12 reader is permitted. The legacy kernel-specific filesystem module is
transitional; the direct ELF loader and full kernel mount qualification remain
unfinished.

Formatting continues to be performed only by the canonical SO, under
exclusive media ownership and the existing physical-removable-media safety
checks. Normal mounting must never format any medium.
