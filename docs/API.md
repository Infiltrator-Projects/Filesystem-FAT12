# Filesystem-FAT12 ABI and consumer contract

**One-driver endpoint:** [FAT12.so is the filesystem driver for applications, BOOT and the future kernel](ONE_MODULE_END_STATE.md). The exported Linux userspace ABI is current; a trusted freestanding/kernel-compatible shared execution ABI still requires design and runtime proof. The `.ko`/`.efi` code described below is transitional, not separate final filesystem products.

## Shipped shared object

Filename: `FAT12.so` under `/System/Filesystems`. Architecture: x86-64 ELF shared object. The independent module must export:

- `infiltrator_filesystem_module_v1`: format-specific read and fragment analysis.
- `infiltrator_filesystem_module_v3`: portable writable filesystem and file/directory operations.
- `infiltrator_filesystem_module_v4`: formatting, attributes, timestamps and free-space reporting.
- `infiltrator_filesystem_defrag_v1`: relocation, journal and crash recovery, invoked in a dedicated Defragmenter child-process host.

The `.ifsmeta` identity must contain `IFS-MODULE:fat12:1`, `IFS-MODULE:fat12:3` and `IFS-MODULE:fat12:4`. A caller must check ABI versions, structure lengths, the actual detected FAT width and function pointers, not merely the filename. The defrag ABI host must supply all function pointers defined in `native/filesystems/fat/defrag/fat_defrag_abi.h`. The child-process lifetime must keep the module loaded for cleanup handlers.

## Constraints

The portable API supports one active volume per module, requires serialized access and exclusive device ownership. Formatting is destructive and not atomic on write failure. On full media or interrupted allocation, partial extension may occur. Format-identity checks and independent readback are required. The module's own source and documented on-disk contracts are authoritative; a GUI geometry guess does not authorise mutation.

## Transitional platform adapters (not the end state)

`fat12.ko` is kernel-version dependent and relies on a compatible Linux FAT core. `FAT12.efi` requires EDK2 and firmware qualification. Neither is treated as the same ABI as this shared object and neither is installed by the independent initial Debian package.

## Common V1 pilot (9 November 2026)

The source now additionally exports `infiltrator_filesystem_api_v1()` from the **existing** Common/Libraries `include/infiltratr/filesystem_module.h` contract. FAT12 carries an exact source snapshot of that freestanding header (Common `main` commit `a99ef676b0e43c7e645c81756e3fd38be0a54406`, header blob `20682324e93982b672ab1c53b04a95d8b04836ff`) until the Common dependency can be pinned. This is interface metadata/host adaptation, not a copy of a filesystem engine.

Common V1 advertises probe, mount, directory list, stat, read, guarded write/create/remove/rename/truncate and format. The **existing FAT12 V1/V3/V4 implementation** owns all filesystem operations; the Common interface only adapts the host callbacks, volumes, paths and status values. The Common mount supports one live volume per module instance, with read-only callbacks on read-only media and exclusive access required for any mutation. Generation-stamped opaque handles reject stale callers after a remount, avoiding accidental access to a different partition. Host callbacks remain valid until unmount.

Common formatting invokes the existing V4 formatter only when the caller declares verified exclusive access, provides read/write/flush callbacks, selects FAT12 (variant 12), leaves the label empty and supplies a pre-existing, independently recognised FAT12 volume. Its BPB supplies the logical sector size. **Blank-media formatting through Common V1 is not supported** because this ABI lacks a logical-sector-size field; V4 supports explicit sector sizes and is the current native formatting route. Formatting remains destructive and non-atomic.

Common V1 advertises a non-destructive FAT12 allocation and directory integrity check. It reuses the existing validated V1 geometry and packed FAT-entry reader, scans reachable files/directories without recursion or allocation and reports cross-links, bad chains, orphaned clusters and mismatched FAT copies. A successfully completed scan returns `IFS_FS_OK` and an error count; I/O failures return `IFS_FS_IO_ERROR`. It currently requires a successful mount, so severely damaged media may need an offline checker. **Common V1 still does not advertise automated repair or defragmentation through this table.** Full FAT12 V3/V4, Defragmenter and Boot interfaces remain available through their established entry points. Files right-click volume formatting, generic kernel SO loading, kernel VFS support, simultaneous volume sessions and physical media qualification are still pending. Do not present this as a completed all-consumer migration.

Defragmentation has its separately versioned host ABI; there is no generic defrag interface in Common V1 yet. Files right-click actions, generic kernel SO loading, kernel VFS support and physical media qualification are still pending. Do not present this as a completed all-consumer migration.
