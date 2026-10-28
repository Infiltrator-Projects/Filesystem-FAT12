# Filesystem-FAT12 ABI and consumer contract

## Shipped shared object

Filename: `FAT12.so` under `/System/Filesystems`. Architecture: x86-64 ELF shared object. The independent module must export:

- `infiltrator_filesystem_module_v1`: format-specific read and fragment analysis.
- `infiltrator_filesystem_module_v3`: portable writable filesystem and file/directory operations.
- `infiltrator_filesystem_module_v4`: formatting, attributes, timestamps and free-space reporting.
- `infiltrator_filesystem_defrag_v1`: relocation, journal and crash recovery, invoked in a dedicated Defragmenter child-process host.

The `.ifsmeta` identity must contain `IFS-MODULE:fat12:1`, `IFS-MODULE:fat12:3` and `IFS-MODULE:fat12:4`. A caller must check ABI versions, structure lengths, the actual detected FAT width and function pointers, not merely the filename. The defrag ABI host must supply all function pointers defined in `native/filesystems/fat/defrag/fat_defrag_abi.h`. The child-process lifetime must keep the module loaded for cleanup handlers.

## Constraints

The portable API supports one active volume per module, requires serialized access and exclusive device ownership. Formatting is destructive and not atomic on write failure. On full media or interrupted allocation, partial extension may occur. Format-identity checks and independent readback are required. The module's own source and documented on-disk contracts are authoritative; a GUI geometry guess does not authorise mutation.

## Linux and EFI driver interfaces

`fat12.ko` registers a distinct Linux VFS FAT12 filesystem with the normal FAT core dependency. Its `vermagic` and exported symbol requirements are specific to the actual kernel. The 0.1.2 candidate ships the exact QEMU-tested 6.12.107 reference module, locally copied kernel source and `standalone/kernel.py` for host builds; the native installer must compile and register a module for the running kernel rather than loading the reference artifact on another ABI.

`FAT12.efi` is a PE32+ x64 EFI boot-services driver exposing UEFI Simple File System via the EDK2 driver binding. Its separate FAT12 width identity is validated under OVMF; other formats must not be mistaken for FAT12. The binary is shipped in the candidate package, not silently installed onto an ESP. The SO, KO and EFI interfaces are separate, and all are required for completion.
