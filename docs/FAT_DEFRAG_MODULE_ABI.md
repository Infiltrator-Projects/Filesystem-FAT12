# FAT defragmentation module interface

The FAT12 and FAT16 shared objects export `infiltrator_filesystem_defrag_v1`
in addition to their filesystem APIs. The same entry point is included in the
existing FAT32 shared object to preserve Defragmenter's FAT32 support. The
filesystem-specific analysis, relocation, directory updates and durable
journal implementation are compiled into each selected FAT module. The source
was migrated from Defragmenter's native FAT engine and remains GPL-3.0-or-later.

The versioned interface in `native/filesystems/fat/defrag/fat_defrag_abi.h`
exposes the module's FAT width, a read-only geometry probe and a worker entry
point. The caller supplies operating-system functions and generic device,
memory, timing, Stop and Common services in `IfsFatDefragHostV1`. This keeps
the `.so` self-contained: production builds have no undefined symbols or
`DT_NEEDED` dependencies. The host and module must agree on the ABI version and
structure size. The ABI is a Linux process interface; it is not the kernel or
UEFI driver interface.

The worker owns process-global state, registers cleanup handlers and may call
`exit`. Run it once in a dedicated child process and keep its host table and
module loaded until process exit. Defragmenter's loader opens the installed
module without following symlinks, verifies root ownership and permissions,
probes the target, and checks the selected module's identity. The worker checks
the format again from the actual opened and locked target before any write.
Its existing mounted-device, confirmation, exclusive-lock, target-identity and
durable-journal checks still apply. An installed module is required for the
corresponding FAT width; an earlier module without the defrag ABI must be
updated.

FAT12 and FAT16 use a fixed root directory and store zero root-cluster values in
their relocation journals. Recovery accepts those values only for fixed-root
volumes; FAT32 recovery verifies both root values against the opened volume.
The integration tests interrupt the worker after each durable journal stage,
reject recovery against a different image, then recover and verify the data.
They use disposable image files and do not qualify physical power-loss or
hardware behaviour.
