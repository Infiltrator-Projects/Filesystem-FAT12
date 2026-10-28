# APFS editable original Linux driver source

## Current source status

The original, actual out-of-tree Linux APFS implementation from
[linux-apfs/linux-apfs-rw](https://github.com/linux-apfs/linux-apfs-rw)
is preserved in `native/filesystems/apfs/linux/` for direct development.
The imported source is the exact Git tree
`03187127cd2e62cf78321cdb942eb3a7b17981f9`; all 38 source,
build and licence files were verified against the original blob hashes.

This is working implementation code, **not** a discarded reference. The
upstream version supplied its own `obj-m = apfs.o` Kbuild and DKMS support.
Its existing copyright and licence notices are retained verbatim.

## Deployment and qualification

The APFS source already defines a Linux VFS driver with build-time
read/write capability. Importing it is not evidence that its write path,
feature variants, kernel ABI, crash handling, or installed-driver selection
are qualified for Filesystems. The project currently delegates APFS support
to the externally managed catalogue provider.

Do not add `apfs` to `data/native-filesystems.tsv` or enable **Install
native** until the exact Debian target kernel has a symbol-resolved module
build, read-only mount and corruption tests, independent interoperability
tests, and separately qualified writes. If the host has another APFS
provider, fail safely instead of silently loading an unintended driver.

EFI boot-source and Linux root qualification are **separate**, incomplete
milestones. Original source ownership is preserved until actual code units
have been independently replaced; moving files does not imply new authorship.
