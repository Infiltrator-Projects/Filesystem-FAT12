# SCO BFS Design and Ownership

> **Working source (23 October 2026).** This filesystem's `linux/` directory contains the actual copied Linux implementation source, preserved with its original attribution and available for direct modification. It is not a replaceable research-only reference tree. The old independently-rewrite-first policy and any old directory-layout/refresh language below are historical, not a restriction on editing this working baseline. Only explicitly qualified drivers appear in `data/native-filesystems.tsv`.


## Qualified Linux module state

The actual original Linux BFS implementation now builds as its own `bfs.ko`
using explicit out-of-tree Kbuild registration. Debian 13 Module.symvers-backed
compilation and the independent Linux 6.12 QEMU mount/write/unmount/remount
runtime test passed (Filesystems CI run 39921073843). The Filesystems native
module helper owns installation, source digest, selection, signing, rollback and
removal. This does not qualify a UEFI reader or InfiltratorOS root filesystem.

The original implementation files and provenance are preserved. Older
imported-only and rewrite-first descriptions below are historical and do not
supersede this qualified Linux runtime status.

## Classification

BFS here means the SCO/UnixWare Boot File System. It is a conventional local
block filesystem, so its permanent Filesystems architecture is one
canonical filesystem engine with thin operating-system adapters.

## Current state

BFS is currently **imported-source development state**. The Linux source is preserved
unchanged under `linux/`; Filesystems does not claim that
implementation as project-authored code.

The upstream files retain their original names, licences and provenance.
`dir.c`, `file.c` and `inode.c` are reference translation units, not the
required permanent project file boundaries.

## Current directory contract

While BFS remains in imported-source development state, the filesystem root contains only
`DESIGN.md` and `linux/`, with the copied Linux implementation below
`linux/`. The future `core/`, `linux/` and `windows/` directories do
not exist until an independent rewrite actually begins; `kernel/` and
`userspace/` are not valid substitutes for that architecture.

CI enforces this present-state boundary and keeps the imported tree out of the
production CMake source graph.

## Target layout

```text
native/filesystems/bfs/
  DESIGN.md
  core/                 canonical BFS format and filesystem semantics
  linux/                thin Linux VFS/module adapter
  windows/              thin Windows IFS/WDK adapter when implemented
  linux/      pinned upstream Linux evidence
```

## Canonical ownership

The future `core/` owns BFS-defined superblock/inode/directory layout,
allocation and block mapping, filename/directory rules, metadata validation,
free-space/accounting rules and any exact mutation/recovery semantics.

Linux VFS inode/file/dentry objects, buffer/page interfaces, mount lifecycle and
kernel error translation remain in `linux/`. Windows IFS objects and Cache
Manager/IRP integration remain in `windows/`.

## Promotion rule

BFS leaves imported-source development state only after host-neutral format decoding,
independent fixtures, malformed-media tests and a real adapter are implemented.
Write support requires a separately reviewed durability/recovery contract.

Moving or renaming upstream source does not cross the authorship boundary.

## Copied implementation provenance

The editable `linux/` source contains original implementation code copied from the pinned Linux v6.12.107 source baseline. Original copyright, licence and attribution remain part of that code. Source refresh is not automatic and must never replace Filesystems modifications. Promotion into a managed native module requires its own host-integration and runtime qualification.
