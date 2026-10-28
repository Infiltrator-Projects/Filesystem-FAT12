# Filesystem-FAT12 architecture

## Purpose

This repository owns the fat12 engine and its versioned C ABI. The shipped Linux shared object is `/System/Filesystems/FAT12.so`. Neither a sibling filesystem checkout nor the original Filesystems source tree is a build-time dependency. The copied Common headers are locally pinned, not dynamically retrieved during compilation.

## Production call paths

```text
Filesystems manager -> install selected filesystem package -> /System/Filesystems/FAT12.so
Defragmenter -> defragmenter-fat-worker -> dlopen(FAT12.so) -> versioned defrag engine
Caller device I/O -> FAT filesystem ABI -> FatFs + local format adapter
```

The filesystem engine must not ask Defragmenter to implement fat12 placement, allocation semantics, durable recovery stages or on-disk identity. Defragmenter supplies device and generic safety services through the host table; operations on unsafe or mounted devices are refused.

## Source layout

- `native/filesystems/fat/module/`: selected FAT ABI adapter and format-specific identity.
- `native/filesystems/fat/portable/`: copied portable FatFs read/write and format code.
- `native/filesystems/fat/defrag/`: allocation analysis, relocation, journal, crash recovery, and versioned defrag API.
- `native/filesystems/fat/linux/`: experimental Linux VFAT namespace module with external host FAT core dependency.
- `native/filesystems/fat/efi/FatPkg/`: copied TianoCore EnhancedFatDxe source and per-format binding definition.
- `src/infiltratr-common/`: copied Common source snapshot; the standalone shared-object build uses required local headers.
- `standalone/`: authoritative one-format build, release packaging and checks.
- Other copied Filesystems source remains an untrimmed migration snapshot. It is not silently built or installed by this module's authoritative Makefile.

## Format boundary

FAT12 12-bit packed entries, fixed-root FAT directory and cluster count below 4,085. Preserve the 4,085-cluster rejection for ambiguous interpretation. The module identity appears both in its filename and embedded metadata and is rechecked before trusted operations. A copied file renamed from the other FAT width is not a substitute.

## Independence

No network operation occurs in the regular shared-object build. The 0.1.2 candidate additionally packages its selected, independently runtime-qualified kernel and UEFI drivers, as documented in [the triple-driver acceptance contract](COMPLETE_FILESYSTEM_ACCEPTANCE.md). Two repositories can legitimately contain the same copied portable, defrag, Linux, EFI and Common source until consolidation; changes must be ported explicitly and independently qualified. Copying shared code does not erase upstream copyright or licence obligations.
