# Filesystem-FAT12 design rationale

**End state:** [one FAT12.so, all consumers](ONE_MODULE_END_STATE.md). BOOT and kernel should adapt to the chosen freestanding/module profile, not force independent FAT12 implementations forever.

The format implementation, not the user interface or management installer, owns parsing, directory and cluster interpretation, mutation, verification and journal recovery. This prevents version-skew between Defragmenter and Filesystems and ensures one selected driver is responsible for its format.

## Why a standalone repository

Small independently releasable format packages can be developed, tested, installed, upgraded and removed without forcing unrelated filesystem engines to be rebuilt. Shared supporting code is initially copied intentionally. Independence and correctness take precedence over deduplicating code prematurely.

## Why transitional execution environments need different adapters

A conventional Linux `.so`, kernel `.ko` and UEFI `.efi` do not share an executable ABI *today*. That is an interface constraint to be handled by generic BOOT/kernel adapters, **not** a permanent requirement for three per-format distributed filesystem drivers or independent FAT12 engines. BOOT and kernel will eventually load/bridge functionality from the one `FAT12.so`. Until equivalent runtime testing passes, existing kernel and EFI test builds are retained as compatibility mechanisms. A green SO test cannot certify kernel mount or UEFI boot.

## Failure policy

Unknown features, ambiguous cluster counts, invalid identity, foreign FAT widths, mounted or substituted targets, unsafe source privileges, incomplete journal state and media I/O failures must fail closed. Preserve recoverable on-media state when interruption occurs; a successful process exit without post-operation verification does not establish durability.

## Dependency boundary

Use copied source and standard build tools. Do not fetch another repository merely to compile the `.so`. Libraries/Common headers remain accompanied by provenance and original licence. The host defragmentation ABI supplies generic functions, never a competing fat12 implementation.
