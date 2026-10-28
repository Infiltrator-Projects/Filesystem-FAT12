# FAT12 complete three-driver acceptance contract

The target is ONE independently maintained FAT12 filesystem with **fully functional read/write SO + KO + EFI**, not a shared-object-only placeholder. The published FAT12 0.1.0 and 0.1.1 packages were shared-object-only. The 0.1.2 branch is a **candidate**, not complete until its exact-source Linux/QEMU, EFI/OVMF and native-install tests all pass and the final packages are published.

## The three delivered driver forms

1. **Shared object**, `/System/Filesystems/FAT12.so`: filesystem V1/V3/V4, read/write/create/delete/rename/format, plus the exact versioned defragmentation and recovery API. Defragmenter dynamically loads it; it owns no duplicate FAT engine.
2. **Kernel driver**, `fat12.ko`: its own Linux VFS filesystem registration, read/write/create/delete/rename, mount/unmount and remount. The release includes the *actual tested binary* for Linux **6.12.107 QEMU** under `/System/Kernel/Modules/6.12.107-qemu-tested-only/` and the full locally copied Linux source and one-format build helper under `/System/Kernel/Sources/Filesystem-FAT12/`. The native RUN compiles a **second** module for its actual running kernel using its prepared kernel headers; the module is then installed as a dpkg-owned `/lib/modules/<running-version>/updates/infiltrator/fat12.ko` and registered by `depmod` without automatically loading it. If matching headers, exports or dependencies are unavailable, fail rather than pretend the reference KO is portable. This does **not** claim universal Linux-kernel compatibility.
3. **UEFI driver**, `FAT12.efi`: independent writable driver built from our imported source using pinned EDK2 and booted/tested in QEMU/OVMF. Shipped under `/System/Kernel/Firmware/EFI/FAT12.efi` and verified against the exact tested image. We do not silently edit the EFI System Partition, firmware boot entries or NVRAM. Actual physical UEFI hardware qualification and boot-from-volume testing remain separate evidence requirements.

## Mandatory per-repository tests

- SO read/write format identity and standalone fixture; exported filesystem V1/V3/V4 and Defragmenter V1 entry points.
- Kernel QEMU boot **using this repository's selected compiled KO**, wrong-format rejection without media mutation, mount/write/create/rename/delete/remount/reopen, FAT12 old DOS and large-cluster boundary where applicable, and external `fsck.fat`/`mtools` readback.
- EFI OVMF boots and binds this repository's driver, denies the other FAT variant, reads/writes/renames/deletes/flushes/rebinds, checks independent media integrity and tests FAT12 large-cluster variant as applicable.
- The generic Debian and source-embedded native RUN each ship **all three** driver forms with SHA-256 and format-identity checks; any missing or altered driver fails the release gate. The RUN must not substitute a Linux-6.12 reference kernel binary for the actual host kernel.
- Two public release assets: generic `.deb` and native `.run`. GitHub generates source ZIP and tarball automatically for the same immutable tag.
- InfiltratorOS and Filesystems coordinate package ownership; the installed correct kernel driver is discoverable via Linux `modprobe`, and the EFI binary remains available for an explicitly configured firmware loader. Defragmenter continues loading the `.so` ABI without a private FAT engine.

## Evidence boundaries

CI/QEMU/OVMF proving functionality is real automated integration evidence, **not** evidence that a driver has been tried on every Mint/Debian kernel, physical UEFI board or power-loss scenario. Do not declare the full filesystem finished merely because the three binaries exist or compile. Keep the release PR unmerged if any runtime or package gate fails.

The QEMU Linux core is the compatible upstream FAT core; the namespace drivers are independent but not independent of required exported kernel core functions. The EFI driver likewise uses EDK2 libraries. Both are normal platform dependencies and must be documented accurately. Licensing and upstream notices remain intact.
