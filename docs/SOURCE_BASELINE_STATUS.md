# Filesystem source availability

Of the **41 catalogue formats**, **37** currently have implementation
source available in this repository:

- **13** project-managed independent native Linux drivers: EXT2, EXT3,
  EXT4, OFS, FFS, SFS, SFS2, PFS3, Minix, BFS, NTFS3 (NTFS format identity),
  exFAT and UDF. Deployment authority: `data/native-filesystems.tsv`.
- **22** further editable original Linux source trees in
  `native/filesystems/<id>/linux/`, not yet managed.
- **2** editable original userspace implementations for CP/M and
  Smaky/Fosfat, requiring a proper native Linux VFS adapter.
- **4** source baselines not yet imported into this repository:
  InfiltratorFS (maintained independently), VMFS3/VMFS5, VMFS6 and ZFS.

Actual filesystem source, an independently qualified native `.ko`,
a read-only EFI reader and safe bootable InfiltratorOS root semantics are
four distinct engineering milestones.

Minix/BFS, NTFS3, exFAT and UDF have passed focused Linux 6.12 native
QEMU mount/write/remount tests. NTFS3 passed independent ntfscat cross-check,
exFAT independent fsck.exfat, and UDF independent host-kernel readback.
These tests do not certify untested format revisions, hardware failures,
journal recovery, removable media or system-root suitability.

The original SFS, SFS2 and PFS3 snapshots also remain under per-format
`original/` directories. Code attribution is retained. No discarded
reference-only filesystem source quarantine remains.
