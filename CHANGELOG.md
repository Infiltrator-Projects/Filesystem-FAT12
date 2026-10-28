# Filesystem FAT12 changelog

## 0.1.4 — 2026-10-29 (single-driver architecture documentation)

- Define `FAT12.so` as the only final filesystem-specific distributed driver for BOOT, kernel and applications; one canonical FAT12 engine.
- Clarify that currently built `fat12.ko`/`FAT12.efi` components are transitional compatibility and test outputs, not separate permanent end-user filesystem products.
- Document missing universal BOOT/kernel loader work and independent acceptance gates. No runtime code or functional completion claims changed.


## 0.1.1 — Standalone documentation and release provenance

- Record the independently qualified FAT12.so release, its limitations and immutable build/release evidence.
- Preserve the imported Filesystems historical changelog separately rather than treating unrelated filesystem development as standalone FAT12 work.
- Clarify intentional source duplication, licensing, independent package ownership and the deferred .ko/.efi milestone.

## 0.1.0 — First standalone shared-object release

- Extract complete Filesystems source from pinned commit `d7a572dcd0d11f7bc69cc05e09e3e944be747838`, including local Common snapshot, FAT filesystem/defragmentation, Linux and UEFI source.
- Build independently from this repository with exact FAT12 identity and V1/V3/V4 plus defragmentation ABIs.
- Qualify the generic Debian package and CPU-native LTO source installer, install as `infiltrator-filesystem-fat12` in `/System/Filesystems/FAT12.so`, and publish with GitHub source archives.
- Kernel .ko and EFI driver source retained but NOT installed or represented as fully qualified by this .so package.

## Pre-extraction historical source

The previous combined project's release history is retained in [FILESYSTEMS-SOURCE-CHANGELOG.md](FILESYSTEMS-SOURCE-CHANGELOG.md), and is not the release history of this standalone filesystem.
