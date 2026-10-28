# Filesystem-FAT12 roadmap

## Immediate standalone milestone

- [x] Preserve full pinned source in an isolated migration branch, including duplication where required.
- [ ] Build and qualify this repository's own `FAT12.so` with its V1/V3/V4 and defrag ABIs.
- [ ] Release only the matching standalone `.deb` and native `.run` after successful CI, plus GitHub's default source ZIP.
- [ ] Transfer installer ownership from Filesystems without colliding with old selector builds.
- [ ] Have InfiltratorOS install and verify both individual FAT modules.
- [ ] Validate the current Defragmenter against independently installed matching FAT modules.

## Subsequent milestones

- [ ] Qualify `fat12.ko` on QEMU Linux 6.12.107, package tested bytes, support installation of a native module built with the running host kernel headers; expand physical Mint/Debian kernel compatibility before claiming universal support.
- [ ] Qualify `FAT12.efi` under OVMF with real read/write tests, then qualify physical firmware and boot-from-volume separately.
- [ ] Expand crash/recovery, negative-volume and interrupted-media testing and physical removable-media coverage.
- [ ] Remove imported unrelated Filesystems sources and duplicate helpers only after independent builds no longer need them.
- [ ] Introduce a backwards-compatible source/ABI release process and lifecycle compatibility matrix.

**The 0.1.2 branch is not complete based on compilation alone.** All three drivers, cross-format rejection, source-based install and exact tested release bytes must pass before the PR can merge. See [full acceptance criteria](COMPLETE_FILESYSTEM_ACCEPTANCE.md).
