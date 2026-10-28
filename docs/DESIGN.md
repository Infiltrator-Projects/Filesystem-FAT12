# Filesystem-FAT12 design rationale

The format implementation, not the user interface or management installer, owns parsing, directory and cluster interpretation, mutation, verification and journal recovery. This prevents version-skew between Defragmenter and Filesystems and ensures one selected driver is responsible for its format.

## Why a standalone repository

Small independently releasable format packages can be developed, tested, installed, upgraded and removed without forcing unrelated filesystem engines to be rebuilt. Shared supporting code is initially copied intentionally. Independence and correctness take precedence over deduplicating code prematurely.

## Why three driver forms are distinct

A Linux `.so`, kernel `.ko` and UEFI `.efi` do not share an executable ABI. A successful shared-object test does not prove the kernel module mounts on every kernel or that a firmware driver boots an operating system. Each driver form needs its own isolated compile, runtime qualification, release and installation evidence. The initial native package installs only the `.so`.

## Failure policy

Unknown features, ambiguous cluster counts, invalid identity, foreign FAT widths, mounted or substituted targets, unsafe source privileges, incomplete journal state and media I/O failures must fail closed. Preserve recoverable on-media state when interruption occurs; a successful process exit without post-operation verification does not establish durability.

## Dependency boundary

Use copied source and standard build tools. Do not fetch another repository merely to compile the `.so`. Libraries/Common headers remain accompanied by provenance and original licence. The host defragmentation ABI supplies generic functions, never a competing fat12 implementation.
