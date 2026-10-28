# exFAT Filesystem Implementation

## Editable original source

The full original Samsung-origin Linux exFAT VFS implementation is retained
under `native/filesystems/exfat/linux/` as the directly editable source
baseline, with its original SPDX, copyright and authorship intact.
Moving and compiling copied original source does not change its provenance.

## Independently managed Linux module

The original source now compiles as the self-contained project-managed
`exfat.ko` with explicit `obj-m` registration. The single exFAT format
identity maps to this Linux VFS module in `data/native-filesystems.tsv`.
The native module helper owns kernel-version source builds, signing,
selected module verification, clean removal and rollback.

The independent Linux 6.12 QEMU runtime test
(https://github.com/Infiltrator-Projects/Filesystems/actions/runs/39931682513)
passed. It booted Linux with exFAT not compiled into vmlinux, loaded
the project's copied original module, mounted an exFAT volume formatted
using the independent exfatprogs formatter, created and renamed files,
wrote a 128KiB multi-cluster file, unmounted/remounted, validated contents
and size, and completed an independent offline `fsck.exfat` check.

This qualifies the exercised native runtime path, not every exFAT edge
case, corruption recovery or removable-media failure. Unsupported variants
must fail closed and be tested with independently generated images.

## Remaining capabilities

Native filesystem-formatting/repair support, Linux runtime portability to
other kernel interfaces, malformed media and sustained mutation/recovery
qualification require separate evidence. A matching read-only EFI reader
and root/setup boot tests have not been qualified.
