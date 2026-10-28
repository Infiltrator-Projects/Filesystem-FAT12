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
