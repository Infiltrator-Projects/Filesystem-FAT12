# JFS Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

JFS is a conventional local journalled block filesystem. Filesystems
therefore applies the one-canonical-filesystem architecture: format and
filesystem semantics belong in a host-neutral core; operating-system code is an
adapter around that core.

## Current implementation state

JFS is currently **imported-source development state**.

There is no project-authored canonical JFS engine and no Filesystems JFS
native module claimed by this directory. The source under `linux/`
is the pinned upstream Linux JFS implementation retained as semantic,
interoperability and failure-behaviour evidence.

The reference files retain their original provenance, licences, filenames and
implementation boundaries. Their relocation under `linux/` is not
an implementation rewrite.

## Current layout

```text
native/filesystems/jfs/
  DESIGN.md
  reference/
    linux/               pinned upstream Linux JFS evidence
```

A future project implementation is added beside the reference tree:

```text
native/filesystems/jfs/
  DESIGN.md
  core/                  canonical JFS format/filesystem semantics
  linux/                 thin Linux VFS/module adapter
  windows/               thin Windows IFS/WDK adapter when implemented
  userspace/             optional image/qualification adapter
  linux/       preserved upstream evidence
```

## Canonical-core ownership

The future core owns JFS-defined semantics independent of the host OS,
including:

- superblock and aggregate/fileset geometry;
- inode allocation groups and inode-map semantics;
- block allocation maps and extent descriptors;
- directory B+tree and name/record rules;
- file extent mapping;
- journal/log record format, transaction and replay invariants;
- extended attributes and filesystem metadata records;
- free-space/accounting rules;
- corruption/range validation;
- exact mutation and crash-recovery semantics.

Linux VFS objects, page/buffer cache, block-device plumbing, workqueues,
credentials, quota framework glue and module lifetime belong in the Linux
adapter.

## File-boundary rule

Upstream Linux filenames remain unchanged beneath `linux/`.
Project-owned source must be organised around Filesystems
responsibilities and host-neutral JFS invariants, not copied or mechanically
renamed from Linux.

Moving, renaming, merging or recommenting external source is never evidence that
the implementation has crossed the project-authorship boundary.

## Promotion sequence

JFS leaves imported-source development state only through an independent rewrite:

1. document supported on-disk format and journal variants;
2. implement bounded superblock, inode-map, allocation-map and extent decoding;
3. implement read-only directory/file traversal with independent fixtures;
4. implement and test journal/log decoding and replay invariants;
5. qualify malformed media and crash-state handling;
6. add a thin Linux adapter over the canonical core;
7. add mutation only after transaction/recovery ordering is independently
   implemented and destructively qualified;
8. add a Windows adapter against the same core when ready.

## Safety and failure behaviour

Invalid geometry, contradictory allocation/inode maps, malformed B+tree nodes,
out-of-range extents, bad journal records and uncertain replay state must fail
closed.

Write support must not be inferred from the capabilities of the reference
driver.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.

## Completion rule

The current tree is reference evidence only. JFS is not a Filesystems
native implementation until the canonical core and applicable adapter are
independently implemented and qualified.
