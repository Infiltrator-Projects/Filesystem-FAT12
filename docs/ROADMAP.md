# Filesystem-FAT12 roadmap

## Immediate standalone milestone

- [x] Preserve full pinned source in an isolated migration branch, including duplication where required.
- [ ] Build and qualify this repository's own `FAT12.so` with its V1/V3/V4 and defrag ABIs.
- [ ] Release only the matching standalone `.deb` and native `.run` after successful CI, plus GitHub's default source ZIP.
- [ ] Transfer installer ownership from Filesystems without colliding with old selector builds.
- [ ] Have InfiltratorOS install and verify both individual FAT modules.
- [ ] Validate the current Defragmenter against independently installed matching FAT modules.

## Subsequent milestones

- [ ] Qualify `fat12.ko` against supported actual Linux kernels, including mount/write/rename/remount and FAT-width rejection, then design a safe version-specific delivery model.
- [ ] Qualify `FAT12.efi` under OVMF and physical UEFI firmware, including correct read/write and explicit root/boot expectations.
- [ ] Expand crash/recovery, negative-volume and interrupted-media testing and physical removable-media coverage.
- [ ] Remove imported unrelated Filesystems sources and duplicate helpers only after independent builds no longer need them.
- [ ] Introduce a backwards-compatible source/ABI release process and lifecycle compatibility matrix.
