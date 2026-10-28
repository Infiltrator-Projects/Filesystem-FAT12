# Amiga FFS canonical implementation

**Canonical FFS on-disk format and DOS1/3/5/7 identifiers:** [FFS_SPECIFICATION.md](FFS_SPECIFICATION.md). This belongs to FFS; OFS keeps its own DOS0/2/4/6 identifiers.

FFS in this repository is the canonical Infiltrator Projects FFS implementation. The source tree is authoritative.

There must be one FFS engine only. No FUSE, vendored, reference, compatibility, or alternate implementation belongs in the active source tree or application catalogue.

FFS is separate from OFS and SFS. Shared AmigaDOS format concepts do not imply a shared filesystem engine.

Qualification requires supported-target builds, destructive read/write verification, independently created test media, and source review confirming that the active implementation is project-authored code.
