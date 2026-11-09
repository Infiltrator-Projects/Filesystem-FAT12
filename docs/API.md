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

## Canonical Linux VFS host

The `ifs-fuse-host` executable under `/System/Commands` loads the one
`FAT12.so` Common API and translates Linux FUSE3 file operations. This
is a format-neutral bridge, not a second driver. Each mount runs its own
foreground process and its own single-volume SO session, allowing multiple
volumes to be independently hosted. Read-only regular-file images and block
devices are supported; explicit write mode is **regular-file images only**.

`--self-test` formats only a named disposable image and qualifies the
canonical API against FUSE callback mapping. Normal mounts do not format.
Actual kernel /dev/fuse runtime operation, multi-user permissions,
dynamic statfs, writable physical media and the eventual direct kernel
SO loader remain unqualified.
