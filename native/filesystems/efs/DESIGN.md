# SGI EFS Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

EFS is Silicon Graphics' legacy Extent File System, predating XFS. It is a
conventional local block filesystem and the present catalogue exposes it as
read-only.

## Current state

EFS is currently **imported-source development state**. The pinned Linux implementation is
preserved under `linux/` with its original licensing, provenance
and source boundaries. Filesystems does not claim it as project-authored
code.

## Current directory contract

While EFS remains in imported-source development state, the filesystem root contains only
`DESIGN.md` and `linux/`, with the pinned upstream Linux implementation
under `linux/`. The future `core/`, `linux/` and `windows/`
directories appear only with independently authored implementation. `kernel/`
and `userspace/` are not valid substitutes for that architecture.

CI enforces this present-state boundary and rejects accidental production
linkage of the imported reference source.

## Target layout

```text
native/filesystems/efs/
  DESIGN.md
  core/                 canonical EFS format/read semantics
  linux/                thin Linux VFS/module adapter
  windows/              thin Windows adapter when implemented
  linux/      pinned upstream Linux evidence
```

## Canonical ownership

The core owns EFS-defined superblock/geometry validation, inode layout,
extent/block mapping, directory/name semantics, symlink interpretation and
corruption/range checks.

Linux VFS inode/file/dentry objects, buffer cache and mount/module lifecycle
belong only in the Linux adapter.

## Read-only rule

The project must not infer write semantics from the existence of inode/file
operations in another implementation. EFS remains read-only until exact
allocation/update/recovery semantics are independently documented and a
deliberate write milestone is admitted.

## Promotion sequence

Implement bounded host-neutral parsing first, qualify against independent EFS
images and malformed fixtures, then add a thin read-only host adapter.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.
