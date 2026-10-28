# Independent FAT12 forensic audit

## Examined baseline

Original Filesystems source commit `d7a572dcd0d11f7bc69cc05e09e3e944be747838` and native GitHub qualification for the standalone FAT12 v0.1.0. Filesystem sources were copied in full before any removal of unrelated formats; provenance and original licence notices remain present. The independently published generic Debian package and native RUN were built from the same repository and validated on an ephemeral Ubuntu 24.04 GitHub runner.

## Evidence

- Standalone C11 build produces `FAT12.so` with the exact FAT12 embedded metadata for ABI V1, V3 and V4, plus the Defragmenter relocation/recovery export.
- The existing format-specific in-memory FAT fixture executed successfully under the exact standalone build.
- The generated Debian package contains its own module, documentation and no opposite-format module, unrelated executables, or second filesystem implementation installation.
- The aggressive `.run` rebuilt from a checksum-verified embedded source archive, installed native Debian `+native1`, and verified its installed module SHA against BUILD-INFO.
- Release-workflow checks passed on the migration head and merge; public v0.1.0 published with exactly the generic `.deb` and native `.run`. GitHub supplies source ZIP and tarball automatically for the tag.

## Claims explicitly not made

A complete source copy is not the same as an independent audited implementation for every copied filesystem. The .ko source requires compatible kernel module dependencies and version-specific actual mount qualification. The EFI driver requires EDK2 and actual firmware/boot testing. The bundled defragmentation ABI and journaling relocation code have targeted CI tests in Filesystems/Defragmenter, but this independent qualification does not constitute a physical power-failure or destructive-media durability audit. The first standalone release focuses on the shared-object runtime and installed-package ownership.

## Remaining verification

- Independent disposable-image read/write/create/rename/delete/format/unmount/reopen/defrag/recover matrix with corrupt media and interrupt injection.
- Physical removable media, kernel ABI and UEFI hardware qualification of the separate driver forms.
- Filesystems manager selector handover and installed-module upgrade/removal coexistence with earlier combined packages.
- Periodic code provenance and upstream licence review when duplicated sources are changed or pruned.

The status of these gates must be updated with evidence, not changed to green by modifying this document alone.
