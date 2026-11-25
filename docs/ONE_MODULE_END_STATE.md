# FAT12: one authoritative filesystem module

## Governing architecture

One installed `FAT12.so` owns the native format semantics and exposes only `infiltrator_filesystem_api_v2`. Common owns the [V2 ABI and host services](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md). The hosted and freestanding profiles use the same canonical engine; qualification of each profile is separate. Consumers negotiate actual operation masks and receive bounded partition-relative I/O. No V1/V3/V4 compatibility path, filesystem-specific .ko/.efi product, FUSE, VFS proxy, private engine or application-side parser is a permitted production substitute.

- Files, Setup, Disks and Test Media use actual V2 PROBE/MOUNT/READ/FORMAT/CREATE/WRITE operations where qualified. Test Media must not construct the format itself. Refuse unavailable callbacks rather than claiming success.
- Defragmenter uses only V2 DEFRAG operations, each separately qualified. Mapping is not relocation, and a historical worker or dedicated legacy interface is not qualification for current V2. Keep old tests solely as provenance.
- BOOT consumes the exact installed module's freestanding profile using generic host services. Target copies: `/System/Filesystems/FAT12.so` and `/EFI/InfiltratorOS/BOOT/drivers_x64/FAT12.so`. Kernel integration also uses the generic module loader; neither consumer owns a second format parser. Do not claim present BOOT or kernel compatibility without exact-package integration tests.

## Release and qualification

Only portable amd64 DEB and source-downloading CPU-native RUN are manually published; GitHub generates source archives. No embedded source or DEB payload. The current workflow and actual installer must be verified against this policy; this document does not certify build code. Preserve original format invariants and distinguish historical evidence from the exact current tested revision. For original media structures see [field-level specification](FORMAT_SPECIFICATION.md) and [manual](FAT12.pdf).

[Earlier transition and qualification record](https://github.com/Infiltrator-Projects/Filesystem-FAT12/blob/45d00abb435718dff345f1bf3e6e27389a756aeb/docs/ONE_MODULE_END_STATE.md) is retained in Git history; its legacy interfaces and adapters are not current instructions.
