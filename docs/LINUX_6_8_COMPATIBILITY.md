# Linux 6.8 compatibility source provenance

This repository includes a literal copy of Linux 6.8 `fs/fat/fat.h` and `fs/fat/namei_vfat.c` from `torvalds/linux`, tag `v6.8`, with original copyright and GPL-2.0 notices retained.

The variant-specific changes in `namei_vfat.c` introduce the independent FAT12 namespace name, a read-only BPB width check before upstream FAT code can write a dirty flag, a verified DOS 1.x FAT12 exception, and a post-mount width assertion. Other directory/file mutation operations remain the full upstream VFS implementation.

The native builder selects this source **only** for a kernel reporting a 6.8.x release. It retains the copied 6.12 implementation for 6.12.x. Other kernel versions fail closed, requiring a separate supported ABI port and actual read/write runtime qualification. Do not attempt to resolve undefined kernel fat_* symbols by bypassing modpost, or assume successful compilation proves physical media durability.
