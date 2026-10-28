# Bcachefs Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

Bcachefs is a conventional local copy-on-write block filesystem. Filesystem
Support therefore applies the canonical local-filesystem architecture:

```text
                 bcachefs/core/
          canonical Bcachefs semantics
               /              \
              /                \
     bcachefs/linux/       bcachefs/windows/
       thin VFS              thin IFS
       adapter               adapter
```

The existing Linux implementation is valuable semantic/reference evidence, but
its Linux-kernel translation-unit boundaries are not the project architecture.

## Canonical-core ownership

When Bcachefs is independently rewritten, filesystem-defined behaviour belongs
in `core/`, including as applicable:

- persistent superblock/format and feature interpretation;
- bkey/B-tree key ordering, traversal and update semantics;
- extent, allocation and free-space semantics;
- copy-on-write and backpointer/reference rules;
- journal/recovery and checkpoint ordering;
- device and replica placement semantics that are format-defined;
- checksums, compression/encryption format interpretation where supported;
- inode, directory, xattr and subvolume/snapshot semantics;
- corruption/range validation and filesystem-defined repair invariants.

Linux workqueues, folios, block-device objects, kernel locks, VFS inode/file
objects, netlink/debug plumbing and other host mechanisms are not canonical
filesystem semantics.

## Current implementation state

Bcachefs is currently **imported-source development state**.

There is no project-authored Bcachefs canonical engine or Filesystems
Bcachefs kernel module claimed by this directory. The large source tree under
`linux/` is copied from the pinned Linux reference source by the
refresh workflow.

The imported tree includes the mature Linux Bcachefs implementation, with
allocation, B-tree, journal, recovery, snapshot/subvolume, data-I/O and other
subsystems. Its size and completeness must not be mistaken for project
authorship.

Reference files retain their upstream SPDX identifiers, copyright/provenance,
filenames and implementation boundaries. They are deliberately not renamed
into Filesystems responsibility files until their implementation bodies
are independently replaced.

## Current directory layout

```text
native/filesystems/bcachefs/
  DESIGN.md
  reference/
    linux/                  pinned upstream Linux source, reference only
```

The permanent project layout, once rewrite work begins, is:

```text
native/filesystems/bcachefs/
  DESIGN.md
  core/                     canonical Bcachefs implementation
  linux/                    thin Linux VFS/module adapter
  windows/                  thin Windows IFS/WDK adapter when implemented
  linux/          preserved upstream evidence
```

The reference tree is not part of the Filesystems CMake production
source graph.

While Bcachefs remains in imported-source development state, the filesystem root therefore
contains only `DESIGN.md` and `linux/`. CI treats the appearance of
`core/`, `linux/`, `windows/` or `kernel/` as a deliberate promotion event and
rejects accidental production linkage of the imported tree. The reference
filenames remain upstream filenames until implementation bodies are actually
replaced.

## File-boundary rule

Upstream filenames such as `btree_*.c`, `alloc_*.c`, `journal_*.c` and
kernel-specific support files are retained unchanged in `linux/` so
their provenance is obvious.

Project-authored files must be cut around Filesystems responsibilities
and host-neutral ownership, not mechanically copied or renamed from Linux.
Moving an upstream file is never evidence that it has crossed the authorship
boundary.

## Promotion sequence

Bcachefs should leave imported-source development state only in deliberate stages:

1. document the supported on-disk format and feature set;
2. establish bounded host-neutral format decoding and feature negotiation;
3. implement read-only B-tree/key/extents traversal with independent fixtures;
4. qualify malformed/corrupt media failure behaviour;
5. add a thin Linux adapter over the canonical engine;
6. implement mutation only with explicit journal/recovery contracts and
   destructive qualification;
7. add Windows support against the same engine when the canonical contracts are
   mature;
8. retire reference reliance subsystem-by-subsystem only after independent
   replacement and tests.

## Failure policy

Unknown incompatible features, contradictory device/replica state, invalid
B-tree/key geometry, bad checksums, out-of-range extents and unrecoverable
journal state must fail closed. Multi-device and repair operations require
stronger evidence than a successful mount.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.
