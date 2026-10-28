# Filesystem-FAT12 packaging and installation

## Artifacts

For each version `vX.Y.Z`, publish exactly two GitHub release assets: `infiltrator-filesystem-fat12_X.Y.Z_amd64.deb` and `filesystem-fat12-X.Y.Z-native-installer.run`. GitHub's default `Source code (zip)` and `Source code (tar.gz)` links are generated from the same tag, not uploaded copies. `VERSION` is the only standalone release version authority.

## Package ownership

Debian package identity `infiltrator-filesystem-fat12`. The 0.1.2 candidate owns `/System/Filesystems/FAT12.so`, `/System/Kernel/Firmware/EFI/FAT12.efi`, the QEMU-versioned reference `/System/Kernel/Modules/6.12.107-qemu-tested-only/fat12.ko`, local kernel source and helper under `/System/Kernel/Sources/Filesystem-FAT12/`, and passive documentation. A native installation additionally owns a matching running-kernel module under Linux's required `/lib/modules/<kernel>/updates/infiltrator/` location via dpkg and registers it with `depmod`. It must NOT own the opposite FAT module, another filesystem, `/usr/lib/filesystems`, or the Defragmenter executable. Reinstall or removal targets only the selected module. A previous Filesystems selector may have installed an unowned module at the same path; confirm ownership and upgrade behaviour before retiring its installer.

## Generic and native builds

The generic package uses an x86-64 portable C11 compile, LTO where configured, and exact format and ABI checks. The source `.run` embeds the matching full repository source archive and its SHA-256, extracts to temporary private storage, recompiles on the local CPU with `-O3 -march=native -mtune=native -flto`, validates module identity and installs `X.Y.Z+native1` via dpkg. It accepts `--profile aggressive --system-package-mode`. Hardware-native PGO training is not asserted until an actual training/qualification implementation exists.

## InfiltratorOS

InfiltratorOS should launch two separately tracked and checksum-verified filesystem module installers, verify each package and native build metadata, and preserve the existing module loader path. It must not claim successful installation when either release, build or installed module is missing.

## Publication rules

Pull requests build and test without publishing. A protected main commit qualifies in CI; only a successful exact revision is tagged and published, never mutating an existing public release. GitHub's source ZIP must include the full local source and licence notices. The old Filesystems release remains untouched until the new module package has independently passed installation and Defragmenter integration.

## 0.1.2 three-driver gate

The release workflow first builds and boots the selected kernel module in pinned Linux 6.12.107 QEMU and the separate EFI driver in OVMF, with read/write/media consistency tests. The exact qualified `fat12.ko` and `FAT12.efi` are downloaded from those jobs and included alongside `FAT12.so` in the Debian package and checksum-verified source RUN. Failure to supply tested driver binaries blocks publication. The public source ZIP contains all local implementation source; binaries are normal qualified release payloads, not automatically present in GitHub's source-only ZIP. Linux `.ko` is kernel-ABI-specific; the RUN must preflight host headers and build an actual compatible module before modifying installed files. See [full acceptance contract](COMPLETE_FILESYSTEM_ACCEPTANCE.md).
