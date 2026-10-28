# GFS2 Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Classification

GFS2 is a shared-disk clustered filesystem. Multiple hosts coordinate access to
the same block storage through distributed locking and filesystem journal/
recovery rules.

It is not merely a network namespace, so its permanent architecture includes a
canonical filesystem engine. Cluster lock transport and OS objects remain
adapters around that engine.

## Current state

GFS2 is currently **imported-source development state**. The pinned upstream Linux GFS2
implementation under `linux/` is compatibility and design evidence
only and is not project-authored Filesystems code.

## Target layout

```text
native/filesystems/gfs2/
  DESIGN.md
  core/                 canonical GFS2 format/filesystem/cluster semantics
  linux/                thin Linux VFS/block adapter
  windows/              thin Windows adapter if eventually justified
  cluster/              host-neutral cluster-lock service adapter boundary
  linux/      pinned upstream Linux evidence
```

The exact cluster transport may evolve, but filesystem semantics must not be
forked per operating system.

## Canonical ownership

The core owns GFS2-defined superblock/resource-group/inode/directory layout,
block mapping and allocation, journals/log replay, quota/statfs semantics,
glock state transitions that are filesystem-defined, and corruption/recovery
invariants.

The Linux VFS/page-cache implementation and Linux DLM calls are adapter
mechanisms. A cluster-lock provider supplies locking/lease operations without
becoming a second implementation of GFS2 metadata or journals.

## Promotion sequence

Begin with bounded host-neutral media decoding and single-node read-only
validation. Add journal/recovery and allocation with independent fixtures.
Clustered mutation is not complete until lock ordering, node failure, journal
recovery and fencing-related failure modes are exercised on real multi-node
test storage.

## Failure policy

Invalid resource groups, lock-state contradictions, bad journal descriptors,
out-of-range block mappings, unsupported format features and uncertain
multi-node recovery state must fail closed.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.
