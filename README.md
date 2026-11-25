# Filesystem-FAT12

## 1. Purpose and format scope

Microsoft FAT12, with packed 12-bit allocation entries and fixed root directory. This repository owns the format-specific implementation of `FAT12.so` for InfiltratorOS.

## 2. On-disk architecture and invariants

Classify by data-cluster count below 4,085, not by the text label in the boot sector. Derive the data region from BPB geometry, FAT sizes and the fixed-root region. Packed entries can straddle bytes and sectors; updating one must preserve neighbouring bits. Directory chains, file lengths, cluster ownership and redundant tables must be consistent.

## 3. Common V2 implementation and capabilities

Common V2 covers filesystem read/write, volume query, sync, integrity checks, exclusive mirror repair and explicitly sized formatting through the selected FAT engine. Native namespace and capacity errors must not be collapsed into format rejection. BOOT uses the same SO through a bounded read-only host; dedicated relocation and recovery remain separately qualified. The canonical entry point is `infiltrator_filesystem_api_v2`. Consumers must negotiate the actual profile and operation masks and use bounded, partition-relative host I/O. A source-level callback does not by itself establish that every format variant or host profile is qualified.

## 4. Integrity, mutation, allocation and recovery

FAT has no general native journal. Relocation and application-managed recovery must preserve cluster chains, directory start references, FAT copies and payload identity. Do not infer arbitrary power-loss durability or generic kernel-side SO mounting from a successful fixture. For destructive operations, require the applicable exclusive writable host, durability contract and format-specific validation. Never silently substitute a kernel or FUSE driver or treat a rejected or unavailable operation as success. Preserve original disk-format semantics; relocation and recovery are separate from ordinary file I/O.

## 5. Build, installation and release

The target installation is `/System/Filesystems/FAT12.so` and `/System/Resources/Documentation/Filesystem-FAT12.pdf`. The project release policy is a portable amd64 DEB and a CPU-native source-first RUN, with GitHub's automatic source archive; do not treat this policy statement as verification of the current installer payload. The release workflow is the authoritative executable build and package specification. Historical adapters and test artifacts are not additional production filesystem authorities.

## 6. Qualification and limitations

[Recorded qualification workflow](https://github.com/Infiltrator-Projects/Filesystem-FAT12/actions/runs/45519927956) · [Current workflows](https://github.com/Infiltrator-Projects/Filesystem-FAT12/actions?query=branch%3Amain). A successful workflow establishes the tested scope only, not complete original-format parity, physical-media crash safety, or production BOOT/kernel integration. Use disposable images for destructive qualification.

## 7. Technical references and provenance

- [Provider implementation](native/filesystems/fat/portable/fatfs_module.c)
- [Native design or source provenance](native/filesystems/fat/DESIGN.md)
- [Migration and source history](MIGRATION-PROVENANCE.md)
- [Original-format reference source](docs/FAT12.pages) and [PDF](docs/FAT12.pdf)
- [Field-level original-format specification supplement](docs/FORMAT_SPECIFICATION.md)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)
- [API](docs/API.md)
- [Validation](docs/VALIDATION.md)
- [One-module architecture](docs/ONE_MODULE_END_STATE.md)

The original-format reference and historical migration notes describe different evidence from the current production provider. When descriptions disagree, the current exported descriptor, implementation, executable tests and exact-revision release evidence take precedence.
