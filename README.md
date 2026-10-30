# Filesystem-FAT12

**Final design:** [FAT12.so is the sole distributed filesystem driver](docs/ONE_MODULE_END_STATE.md). BOOT, the kernel and applications will consume the same module using generic adapters. Linux `.ko` and UEFI `.efi` code in this repository is transitional, not three permanent FAT12 products. Boot now consumes this same FAT12.so via its generic ELF/UEFI filesystem bridge; a generic kernel-side SO loader remains future work.

Independent InfiltratorOS FAT12 filesystem repository. One repository is authoritative for exactly one installed shared-object module: `/System/Filesystems/FAT12.so`.

## Actual status

The full source was copied from Infiltrator-Projects/Filesystems, pinned to commit `d7a572dcd0d11f7bc69cc05e09e3e944be747838`, including the FAT filesystem interface, its FAT-specific defragmentation and recovery engine, portable FatFs, the Linux VFS source, TianoCore EFI driver source, tests, and Common support source. Import alone is NOT a qualified standalone release.

`make` builds `build/FAT12.so` solely from this repository. `make check` checks module exports, format identity, and the existing standalone FAT fixture. `make release` produces the generic `.deb` and native `.run`; a published GitHub release tag supplies GitHub's automatic source ZIP (not a third uploaded asset). Releases are permitted only when the exact HEAD qualifies in CI.

## Ownership

- This repository: fat12 interpretation, read/write/file APIs, formatting, allocation, fat12 relocation, journal/recovery, format-specific kernel and EFI source and tests.
- Filesystems: discovery, selection and management; it must not silently overwrite or install a second copy of this module after package ownership transfers.
- Defragmenter: the worker and UI; it dynamically loads the installed `FAT12.so` and calls `infiltrator_filesystem_defrag_v1` via the existing versioned host ABI.
- Linux/UEFI transition: existing `fat12.ko` and `FAT12.efi` implementation and tests remain useful for compatibility and qualification. BOOT now uses the same `FAT12.so` through its validated, read-only generic ELF/UEFI bridge and boots a real Linux kernel/initramfs under OVMF. A generic kernel-side SO loader is not yet implemented.

## Filesystem qualification scope

FAT12 12-bit packed entries, fixed-root FAT directory and cluster count below 4,085. Reject foreign formats, malformed geometry and ambiguous boundaries before changes. The independent `.so` exposes V1, V3 and V4 filesystem interfaces, Defragmenter recovery, and the generic read-only Boot V1 ABI; supported operations, limitations and required test evidence are tracked in [Validation](docs/VALIDATION.md) and [API](docs/API.md). Destructive testing must use disposable images only.

## Documentation

[Architecture](docs/ARCHITECTURE.md) · [Design](docs/DESIGN.md) · [Validation](docs/VALIDATION.md) · [Packaging](docs/PACKAGING.md) · [Source provenance](docs/SOURCE.md) · [Security](docs/SECURITY.md) · [Roadmap](docs/ROADMAP.md) · [Documentation index](docs/README.md)
