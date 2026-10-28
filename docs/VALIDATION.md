# Filesystem-FAT12 validation and release criteria

## Evidence separation

1. Source copied: import hash, local source files and preserved notices, but NO functional qualification.
2. Build: native compiler produces an ELF DYN `FAT12.so` with no unexpected dynamic dependencies.
3. ABI: correct width metadata, V1/V3/V4 and Defragmenter entry points, no substitution by another format.
4. Fixture: current `tests/fat12_module_test.c` runs against the exact standalone shared object on an in-memory volume.
5. Integration: Defragmenter's versioned child-worker maps, relocates and recovers disposable independently formatted FAT images, including interrupted-journal stages and independent post-verification.
6. Packaging: inspect exact owned paths in the Debian archive; install the native `.run`, verify native package version, BUILD-INFO and module SHA-256, and ensure the other format remains unchanged.
7. Hardware: physical removable media, supported distributions/kernels and actual UEFI firmware testing. CI and QEMU are not substitutes for hardware evidence.

## CI contract

Run `make check`, `make release`, inspect the resulting package, invoke `.run --profile aggressive --system-package-mode` in a disposable Ubuntu runner, and independently verify installed ABI and package ownership. Publishing is permitted only after the successful release-job gate on the exact main commit. GitHub release source ZIP is automatic with the version tag and is not a third uploaded asset.

## Outstanding confidence limits

Versions 0.1.0/0.1.1 covered only the shared object. The 0.1.2 candidate now requires the QEMU-booted Linux kernel driver and OVMF-booted EFI driver in addition to the shared object. Candidate status is NOT release qualification until its exact CI evidence and packaged bytes pass. Broad fuzzing of malformed BPBs, interrupted media writes, durability under physical power loss, alignment/geometry variations, cross-architecture support and exact Defragmenter release pairing remain separate gates. Do not label these fully qualified merely from a green compilation workflow.

## 0.1.2 candidate — mandatory full filesystem proof

The source-pinned kernel runtime workflow boots the FAT namespace driver with a compatible Linux FAT core, mounts independently formatted images, writes and renames files, removes files/directories, syncs, unmounts/remounts, rejects foreign FAT widths without modifying the image and validates the final result with external FAT tools. The UEFI workflow launches the selected EFI driver under OVMF with firmware default FAT bindings disconnected, checks read/write/rename/delete/flush/rebind and wrong-format rejection, then independently validates images. The `.deb` and `.run` are checked for **exact binary equality** to the separately tested `.ko` and `.efi`, plus checksum records and normal SO ABI. Native installs must build an extra matching `.ko` for `uname -r` before installing, and verify Linux module registration. See [acceptance](COMPLETE_FILESYSTEM_ACCEPTANCE.md).
