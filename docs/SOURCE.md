# Filesystem-FAT12 source record

## Original repository and revision

Imported from https://github.com/Infiltrator-Projects/Filesystems at `d7a572dcd0d11f7bc69cc05e09e3e944be747838` (28 October 2026). Its own Common source snapshot was copied from its pinned submodule; the original source tree, complete supporting files, test fixtures, and upstream notices remain in the migration commit.

## External implementation provenance

The portable `native/filesystems/fat/portable/` contains ChaN FatFs code with upstream notices. The UEFI `native/filesystems/fat/efi/FatPkg/` contains TianoCore EnhancedFatDxe code from `447c415c2678a2bab107d5d529b879e3d7713830`. The Linux `native/filesystems/fat/linux/` is based on Linux FAT/VFAT sources, qualified against Linux 6.12.107 reference tests, subject to upstream GPL-2.0 notices. The defragmentation engine was migrated from https://github.com/Infiltrator-Projects/Defragmenter and retains its GPL-3.0-or-later notices. The copied Common sources retain their original licensing and Git provenance. Copied implementation is not represented as independently invented code.

## Source duplication policy

Both FAT12 and FAT16 repositories deliberately contain overlapping supporting source during migration. Each compiles and releases exactly its selected format. Any future pruning or Common linkage must retain reproducible native builds, accurate declarations, licensing, tests and existing loader ABI. The imported full Filesystems README is retained as `FILESYSTEMS-SOURCE-README.md` for historical context, NOT as the new package specification.
