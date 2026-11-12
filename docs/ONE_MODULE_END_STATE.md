# FAT12: one module, complete filesystem, universal consumers

The canonical filesystem contract is Common V2. BOOT and applications use
the same FAT12 engine. Qualified relocation and journal recovery still use
the dedicated Defragmenter interface until equivalent Common V2 operations
pass independent media tests.

**Authoritative end state:** one independently distributed `FAT12.so` is the **entire FAT12 filesystem driver** and single source of filesystem semantics. The module lives under `/System/Filesystems/FAT12.so` and is discovered by BOOT, the InfiltratorOS kernel and all applications through reviewed, versioned module interfaces and generic loader/host adapters. See [the cross-project authority](https://github.com/Infiltrator-Projects/Filesystems/blob/main/docs/UNIFIED_FILESYSTEM_MODULE_ARCHITECTURE.md).

The end user must **not** have to manage `fat12.ko` and `FAT12.efi` as two additional filesystem products. The preferred engineering result is one source implementation reused across all execution environments. Internal executable representations and bridges are acceptable only where proven necessary; they must not fork FAT12 parsing, write/format/recovery logic into three separately maintained engines.

- **BOOT:** the BOOT-owned generic ELF loader executes the verified freestanding read-only interface **from the same FAT12.so file** and supplies bounded UEFI block I/O and boot-file services. A conventional Linux userspace ELF cannot be passed directly to firmware `LoadImage`; a controlled module profile, verified payload or other proven universal loader is required. BOOT's default boot-path operations remain read-only.
- **Kernel:** the eventual kernel-owned generic module loader or filesystem bridge consumes the same FAT12 implementation. Stock Linux still uses VFS/kernel-safe drivers or an appropriate userspace filesystem service; any interim `fat12.ko` is an implementation bridge, not a separate future filesystem product. Early-root availability, kernel ABI and signing are requirements.
- **Filesystems, Defragmenter, Test Media, Disks, Files and Setup:** consume FAT12.so's format operations; generic I/O, privilege and safety adapters remain generic. Independent external image tools validate changes so the engine cannot certify itself.

## Qualification

The Boot consumer workflow builds the production loader and exercises Common
V2 with the same portable and native module bytes under OVMF. It loads a real
Linux kernel and initramfs and checks that a module missing the required
export is rejected. A generic kernel loader for this module and physical
firmware qualification remain open.

Defragmenter, Files and Test Media require independent operation and
disposable-media qualification. Kernel mount/read/write, early root,
Secure Boot and power-loss recovery require separate evidence.
