# System V / Xenix / Coherent Filesystem Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

The SysV filesystem family represented by the Linux `sysv` driver covers
historical System V-derived disk formats, including recognised Xenix, System V
and Coherent variants. These are conventional local block filesystems.

Filesystems therefore applies one canonical host-neutral engine for the
supported SysV-family variants, with thin operating-system adapters.

## Current implementation state

SysV is currently **imported-source development state**.

There is no project-authored canonical SysV-family engine and no Filesystem
Support SysV native module claimed by this directory. The source beneath
`linux/` is the pinned upstream Linux implementation retained as
format, variant-detection and interoperability evidence.

The upstream licences, copyrights, filenames and translation-unit boundaries
remain intact. The former top-level `kernel/` staging directory was misleading
because it looked like project-owned production source.

## Current directory layout

```text
native/filesystems/sysv/
  DESIGN.md
  reference/
    linux/               pinned upstream Linux SysV-family source, reference only
```

When an independent implementation begins:

```text
native/filesystems/sysv/
  DESIGN.md
  core/                  canonical supported SysV/Xenix/Coherent semantics
  linux/                 thin Linux VFS/module adapter
  windows/               thin Windows adapter when implemented
  linux/       preserved upstream evidence
```

## Canonical ownership

A future `core/` owns variant detection plus the filesystem-defined
superblock, inode, free-block/free-inode accounting, directory, block mapping,
byte-order and corruption rules for each explicitly supported variant.

Variant-specific differences must be represented explicitly in the canonical
model rather than hidden in duplicated platform implementations.

Linux buffer/page APIs, VFS objects, credentials, locking and mount/module
lifecycle remain Linux-adapter concerns.

## File-boundary rule

The imported `balloc.c`, `ialloc.c`, `dir.c`, `inode.c`,
`itree.c`, `namei.c`, `super.c` and `sysv.h` remain unchanged in
`linux/`.

Project-owned source must be cut around portable format/variant responsibilities
rather than mechanically preserving those Linux file boundaries.

## Promotion sequence

1. enumerate and document the exact supported SysV/Xenix/Coherent variants;
2. implement bounded host-neutral variant detection and geometry validation;
3. implement inode, directory and block-tree traversal with independent media;
4. qualify read-only operation and malformed-media rejection per variant;
5. add allocation/mutation only where exact update/recovery semantics are known;
6. add thin platform adapters over the same canonical engine.

## Failure policy

Unknown/ambiguous variants, impossible geometry, out-of-range inode/block
references, corrupt free-list state and malformed directory records must fail
closed. Variant guessing must never turn an uncertain image into a writable
mount.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.

## Completion rule

The current tree is reference evidence only. SysV becomes a Filesystems
first-party implementation only when the canonical engine and applicable
adapter have independent implementation and qualification evidence for every
variant claimed.
