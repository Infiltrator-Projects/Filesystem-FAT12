# Fosfat / Smaky Filesystem Design and Ownership

## Original source baseline

The original Fosfat / Smaky userspace filesystem implementation is now
preserved for direct modification in `native/filesystems/fosfat/userspace/`.
All 35 relevant source, build and licence files were copied without changing
implementation content from Skywalker13/Fosfat at
`289c7eb838dd5e6305c98f09aa375746a6043d86`.
These include disk decoders in `libfosfat/`, the FUSE adapter in `fosmount/`,
the related libraries, original tools and the upstream `COPYING` file.

## Native Linux status

This is the original working userspace implementation, not a Linux kernel
driver. A qualified `fosfat.ko` does not yet exist, and this format is
deliberately absent from `data/native-filesystems.tsv`.
Debian's existing read-only userspace provider remains the current access path.

The next step is to make the original bounded read-only on-disk decoding
kernel-safe, provide its real VFS mount/inode/directory/file operations, and
qualify independently using original Smaky-format media and malformed inputs.
Original copyright and attribution must remain accurate for inherited code.
Write support requires its own format/allocation/recovery proof.

Firmware UEFI reading is a separate future adapter qualification.
