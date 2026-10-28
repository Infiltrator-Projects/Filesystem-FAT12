# F2FS Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

F2FS is a conventional local flash-oriented filesystem with checkpoint,
segment, node and garbage-collection semantics. Filesystems must
implement those semantics once in a host-neutral core.

## Current state

F2FS is currently **imported-source development state**. The Linux implementation under
`linux/` is preserved unchanged as upstream evidence and is not
project-authored source.

## Current directory contract

While F2FS remains in imported-source development state, the filesystem root contains only
`DESIGN.md` and `linux/`, with the pinned upstream Linux implementation
under `linux/`. The future `core/`, `linux/` and `windows/`
directories appear only with independently authored implementation. `kernel/`
and `userspace/` are not valid alternate source trees.

CI enforces this boundary and rejects accidental production linkage of the
imported reference source.

## Target layout

```text
native/filesystems/f2fs/
  DESIGN.md
  core/                 canonical F2FS format/filesystem semantics
  linux/                thin Linux VFS/module adapter
  windows/              thin Windows adapter when implemented
  linux/      pinned upstream Linux evidence
```

## Canonical ownership

The core owns F2FS-defined superblock/checkpoint validation, segment/NAT/SIT
state, node addressing, allocation, summary blocks, recovery, directory/hash
rules, inline data/xattrs, compression-format interpretation, zoned/flash
geometry policy where format-defined, and exact mutation/recovery invariants.

Linux VFS objects, bios/folios, writeback, workqueues, sysfs/debug plumbing and
kernel-specific crypto/verity hooks are adapter mechanisms.

## Promotion sequence

Begin with bounded host-neutral format/checkpoint decoding and read-only node,
directory and data traversal. Qualify independently manufactured images and
malformed checkpoint/segment metadata. Add writes, GC, checkpoint publication
and recovery only after explicit durability/destructive qualification.

## Failure policy

Invalid checkpoint packs, contradictory NAT/SIT/segment state, bad node
addresses, unsupported incompat features and unsafe recovery state fail closed.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.
