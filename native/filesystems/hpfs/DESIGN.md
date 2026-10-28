# HPFS Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

HPFS (High Performance File System) is a conventional local block filesystem
used by OS/2 and related systems. Filesystems therefore applies the
normal one-canonical-filesystem architecture.

## Current implementation state

HPFS is currently **imported-source development state**.

There is no project-authored HPFS canonical engine and no Filesystems
HPFS Linux or Windows driver claimed by this directory. The source under
`linux/` is the pinned upstream Linux HPFS implementation retained
as compatibility and semantic evidence.

Those files keep their upstream SPDX/licence/copyright notices, filenames and
implementation boundaries. Relocating them beneath `linux/` is
provenance hygiene only and is not an implementation rewrite.

## Current layout

```text
native/filesystems/hpfs/
  DESIGN.md
  reference/
    linux/               pinned upstream Linux HPFS evidence
```

When independent implementation begins, project-owned source is added beside
the reference tree:

```text
native/filesystems/hpfs/
  DESIGN.md
  core/                  canonical HPFS format/filesystem semantics
  linux/                 thin Linux VFS/module adapter
  windows/               thin Windows IFS/WDK adapter
  userspace/             optional inspection/image adapter
  linux/       preserved upstream evidence
```

## Canonical-core ownership

The future `core/` owns HPFS-defined semantics that are independent of the
host operating system, including:

- boot/superblock and spare-block decoding and validation;
- sector, band and allocation-bitmap geometry;
- fnode and anode structures and file block mapping;
- dnode directory B-tree structure, ordering and traversal;
- filename, codepage and case/collation rules defined by HPFS;
- extended attributes and filesystem metadata records;
- free-space accounting and allocation;
- hotfix/remapping state where supported by the format;
- timestamps, attributes and object metadata;
- corruption/range validation;
- exact mutation, ordering and recovery semantics for any supported writer.

Linux VFS objects, buffer/page-cache integration, mount/module lifetime,
credentials and Linux error translation belong in `linux/`.

Windows IFS/WDK objects, IRPs, VCB/FCB/CCB lifetime, Cache Manager integration
and NTSTATUS translation belong in `windows/`.

## File-boundary rule

Reference filenames such as `alloc.c`, `anode.c`, `dnode.c`, `map.c`,
`namei.c` and `super.c` remain unchanged under `linux/` so
their provenance is explicit.

Project-authored source must be cut around Filesystems responsibilities
and portable filesystem invariants. Moving, renaming, merging or recommenting
copied Linux source is never evidence that the implementation has been
rewritten.

## Promotion sequence

HPFS leaves imported-source development state only through a deliberate rewrite:

1. define supported HPFS versions, codepages and media geometry;
2. implement bounded host-neutral superblock/spare-block/fnode/anode decoding;
3. implement allocation and directory-tree traversal with independent fixtures;
4. qualify names, codepage/case behaviour and extended attributes;
5. test malformed/corrupt media and out-of-range structures;
6. add a thin read-only Linux adapter over the canonical core;
7. add mutation only after allocation/update/recovery rules are explicitly
   documented and destructively qualified;
8. add a Windows adapter against the same core when ready;
9. remove inherited provenance only from bodies that have actually been
   independently replaced.

## Safety and failure behaviour

Invalid geometry, contradictory allocation maps, malformed fnode/anode chains,
invalid dnode ordering, out-of-range sectors, unsupported codepage/collation
state and unsafe recovery state must fail closed.

The presence of write paths in an external reference implementation is not
evidence that Filesystems has qualified HPFS writes.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.

## Completion rule

HPFS remains reference/import only until a canonical project-authored engine
and at least one adapter have independent implementation and qualification
evidence.
