# Filesystem-FAT12

**Final design:** [FAT12.so is the sole distributed filesystem driver](docs/ONE_MODULE_END_STATE.md). BOOT, the kernel and applications will consume the same module using generic adapters. Linux `.ko` and UEFI `.efi` source remains transitional build material, not alternative FAT12 implementations to retain for compatibility. Boot now consumes this same FAT12.so via its generic ELF/UEFI filesystem bridge; a generic kernel-side SO loader remains future work.

Independent InfiltratorOS FAT12 filesystem repository. One repository is authoritative for exactly one installed shared-object module: `/System/Filesystems/FAT12.so`.

## Actual status

The full source was copied from Infiltrator-Projects/Filesystems, pinned to commit `d7a572dcd0d11f7bc69cc05e09e3e944be747838`, including the FAT filesystem interface, its FAT-specific defragmentation and recovery engine, portable FatFs, the Linux VFS source, TianoCore EFI driver source, tests, and Common support source. Import alone is NOT a qualified standalone release.

`make` builds `build/FAT12.so` solely from this repository. `make check` verifies the sole Common filesystem export, its format identity, the in-memory integrity/recovery fixture and a disposable-image format/read/write test. `make release` produces the generic `.deb` and native `.run`; a published GitHub release tag supplies GitHub's automatic source ZIP (not a third uploaded asset). Releases are permitted only when the exact HEAD qualifies in CI.

## Ownership

- This repository: fat12 interpretation, read/write/file APIs, formatting, allocation, fat12 relocation, journal/recovery, format-specific kernel and EFI source and tests.
- Filesystems: discovery, selection and management; it must not silently overwrite or install a second copy of this module after package ownership transfers.
- Defragmenter: the worker and UI; it dynamically loads the installed `FAT12.so` and calls `infiltrator_filesystem_defrag_v1` via the existing versioned host ABI.
- Linux/UEFI transition: existing `fat12.ko` and `FAT12.efi` implementation and tests remain useful for compatibility and qualification. BOOT now uses the same `FAT12.so` through its validated, read-only generic ELF/UEFI bridge and boots a real Linux kernel/initramfs under OVMF. A generic kernel-side SO loader is not yet implemented.

## Filesystem qualification scope

FAT12 12-bit packed entries, fixed-root FAT directory and cluster count below 4,085. Reject foreign formats, malformed geometry and ambiguous boundaries before changes. The independent `.so` exposes the single canonical Common filesystem API plus separate Defragmenter and read-only Boot host contracts; supported operations, limitations and required test evidence are tracked in [Validation](docs/VALIDATION.md) and [API](docs/API.md). Destructive testing must use disposable images only.

## Documentation

[Architecture](docs/ARCHITECTURE.md) · [Design](docs/DESIGN.md) · [Validation](docs/VALIDATION.md) · [Packaging](docs/PACKAGING.md) · [Source provenance](docs/SOURCE.md) · [Security](docs/SECURITY.md) · [Roadmap](docs/ROADMAP.md) · [Documentation index](docs/README.md)

## Installer files

- `/System/Filesystems/FAT12.so`
- `/System/Resources/Documentation/Filesystem-FAT12.pdf`

The normal published Debian package installs only FAT12.so and its PDF. The native RUN can separately compile a transitional kernel adapter against a qualified, prepared kernel build; that adapter is not an additional published filesystem product. Use `--kernel-build DIRECTORY` when deliberately building one.

InfiltratorOS calls `--system-package-mode` and decides which outputs need updating. Its worker builds a missing or older SO, repairs the PDF independently, and stores the matching released kernel module when available. Use `--build-only --output DIRECTORY` to build a Debian package.
