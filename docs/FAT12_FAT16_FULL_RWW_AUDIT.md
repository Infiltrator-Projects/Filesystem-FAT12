# FAT12 and FAT16 driver qualification

The target is two independently identifiable filesystems, each with a `.so`,
Linux `.ko`, and UEFI `.efi`. This development work is not release approval.

## Sources and current implementation

- The EFI source in `native/filesystems/fat/efi/FatPkg` is the full TianoCore
  EDK2 EnhancedFatDxe implementation from commit
  `447c415c2678a2bab107d5d529b879e3d7713830`, with its upstream notices
  preserved. Two build descriptors give distinct driver identities and restrict
  each driver to its own FAT width. The EDK2 build framework is obtained at
  build time; the filesystem driver source itself is in this repository.
- The Linux sources under `native/filesystems/fat/linux` are the copied Linux
  FAT/VFAT implementation. Each variant builds a separately named namespace
  module. **Both depend on a compatible Linux FAT core**, so a successful
  `.ko` build alone does not establish portability across kernel versions or
  prove a self-contained filesystem module. If the host FAT core was built
  without VFAT and supplies an empty default I/O charset, the two variant
  adapters select `iso8859-1`; an existing host default is retained. A
  read-only BPB preflight rejects the other FAT width before the shared core
  marks a writable volume dirty. Explicit DOS 1.x FAT12 floppy fallback is
  still delegated to the Linux core.
- `FAT12.so` and `FAT16.so` each compile the complete portable ChaN FatFs
  engine into the individual shared object. The V3 ABI provides directory and
  file read/write, allocation through file growth, creation, deletion, rename
  and truncate, with device I/O and flushing supplied by the caller. The
  imported source headers retain their notices. The legacy V1 API remains
  available. V3 currently allows only one active volume per module and callers
  must serialize access and hold exclusive ownership of the device. V4 adds
  formatting with a write-free FAT-width preflight, attributes, modified-time
  updates and free-space reporting. Formatting remains a destructive operation
  and is not atomic if media writes or flushing fail.
- FatFs and the Linux/EDK2 implementations classify a 4,085-cluster volume
  differently. The portable modules reject this ambiguous boundary for both
  FAT12 and FAT16 instead of modifying a volume with an inconsistent identity.
- Directory moves are checked by on-media ancestry, including case and short
  aliases, to avoid moving a directory inside itself. A failed full-volume
  seek is not reported as a completed write or truncate. On an I/O error or
  full volume, partial allocation or extension can still occur; FAT and this
  interface do not guarantee transactional failure atomicity.
- The portable mount checks that the full volume and its data area fit inside
  the supplied partition before allowing file operations, including for a
  believable boot sector on a truncated image.

## Evidence and remaining gates

The original draft's CI has built both `.ko` and `.efi` images and exercised
both V3 `.so` files against independently formatted FAT images with mtools and
fsck. Those results pertain to the exact commits tested. Local synthetic-image
tests also cover long UTF-8 names, zero-filled growth, full media, directory
ancestry and valid directory reparenting. Synthetic fixtures are not an
independent format audit.

The runtime workflow in this branch boots disposable QEMU guests. It is intended
to load the actual FAT12 and FAT16 kernel modules and exercise create, write,
rename, delete and remount, then check images using independent FAT tools. It
also intends to invoke each EFI driver binding directly under OVMF, rejecting
the opposite format, writing and renaming files, stopping and rebinding, and
verifying results with independent tools. **Adding these tests is not proof
that they pass; consult the workflow results for the exact commit.** No physical
firmware, disk or installed system is touched by these tests.

Outstanding work includes successful runtime qualification on supported Linux
kernels and UEFI firmware, corrupted and interrupted-media handling, broader
geometry and fault-injection coverage, broader metadata semantics,
and package/install integration. V3 does **not** yet expose the block relocation
and recovery operations required to move Defragmenter's FAT engine into the
shared libraries. Do not remove that engine, mark this target complete, or
publish these drivers as fully qualified on build evidence alone.
