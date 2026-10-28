# Canonical EXT2 firmware-facing reader

The portable `reader.c` uses the *same* EXT2 volume, inode, directory and
file-mapping implementation as Filesystems' Linux and Windows integrations.
It accepts a bounded, read-only block-device callback and supports ordinary
absolute path traversal, file reads and seeking. Slash and backslash are
accepted as path separators.

The reader strips all write and flush capabilities, checks block-device
bounds independently of the calling host, rejects an unclean EXT2 volume,
and fails closed on symlinks, dot segments and unsupported inode types.
The tests exercise its boundary alongside the canonical engine.

**This is NOT a loadable .efi file**, nor a qualified boot path. A UEFI
driver needs a separate EFI Block/Disk I/O and Simple File System protocol
adapter, UTF-16 path conversion, lifecycle management, PE/COFF build,
QEMU/OVMF media qualification and real-firmware testing.

The standalone EXT3, EXT4, OFS, FFS, SFS, SFS2 and PFS3 portable cores
do not yet supply equivalent complete firmware file readers. Their Linux
modules cannot simply be renamed to .efi. PFS3's Linux mutation path
remains unqualified. This component is deliberately not installed by the
desktop Filesystems package and does not change Setup eligibility.
