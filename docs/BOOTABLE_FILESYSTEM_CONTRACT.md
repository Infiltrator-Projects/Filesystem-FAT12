# Bootable filesystem integration contract

This document records the agreed cross-project boot/install target for **Boot**, **Filesystems**, the future **Setup** installer, and **InfiltratorOS**. It is a *target contract*, not a statement that a full installer or InfiltratorOS kernel exists today.

Ownership remains deliberately separate:

- [Boot](https://github.com/Infiltrator-Projects/Boot/blob/main/docs/ARCHITECTURE.md) owns UEFI discovery, loading and handoff, including dynamic EFI filesystem drivers.
- [Filesystems](NATIVE_FILESYSTEM_VISION.md) owns on-media format identities, format capabilities, native adapters, qualification and format-specific installation planning.
- [InfiltratorOS](https://github.com/Infiltrator-Projects/InfiltratorOS/blob/main/docs/FILESYSTEM.md) owns operating-system namespaces and runtime expectations.
- **Setup** is the reserved name for the future installer and coordinates these owners. It is not a completed installer today.
- [InfiltratorFS](https://github.com/Infiltrator-Projects/InfiltratorFS/blob/main/docs/PLATFORM_ADAPTERS.md) owns its own on-disk format and adapters. It is not the name of the shared Filesystems catalogue.

## Goal

Install an independently chosen filesystem on storage, put a compatible kernel image and any needed early-boot payloads on it, and make Boot find and start that kernel **without relying on GRUB or copying the kernel onto the FAT EFI System Partition (ESP)**. The Boot executable itself resides on an ESP the firmware can initially access; its kernel payload may live on an otherwise unreadable-to-firmware filesystem.

The proof-of-concept is concrete: format a *disposable* test device as authentic AmigaDOS **OFS**, put an EFI-compatible x86-64 Linux kernel (and initramfs when needed) at an agreed path such as `/boot/vmlinuz.efi`, let Boot's future OFS UEFI driver read it, and boot Linux. At first, Linux may mount an existing, independent ext4 root filesystem; Linux **does not need to mount OFS merely because Boot loaded its kernel from OFS**.

Kernel filename and location are policy; `/boot/vmlinuz.efi` is illustrative, not a hard-coded filesystem rule. Boot must discover/resolve the configured payload on each supported format. An arbitrary binary named `.kernel` is not automatically UEFI-bootable: the candidate must implement the handoff protocol Boot uses (for the initial demonstration, Linux EFI stub).

## Two independent module interfaces

```text
UEFI firmware reads the FAT ESP
    -> Boot/BOOTX64.EFI
       -> qualified filesystem UEFI driver (.efi) from drivers_x64
       -> detect OFS, FFS, ext4, InfiltratorFS, ... as supported
       -> read kernel + initramfs from the chosen filesystem
       -> start compatible kernel through an explicit boot protocol

Kernel begins running; UEFI boot-services filesystem access ends
    -> early userspace/initramfs loads required block/partition/fs modules
    -> identifies the *root* volume without fragile /dev/sdX assumptions
    -> mounts root using kernel-side filesystem support (.ko for Linux)
    -> continues operating-system startup
```

1. The EFI reader handles Boot's *source filesystem*: identification, read-only traversal and reading boot assets. Each EFI driver presents the standard EFI Simple File System interface; Boot does not embed filesystem-specific directory parsers or know how OFS stores blocks. Independently packaged `.efi` drivers can be added, removed or replaced without rebuilding Boot. Several may coexist.
2. The kernel-side filesystem driver handles any filesystem the *running OS* must access, especially the root volume. Linux uses modular VFS adapters (`.ko` and dependencies); the eventual native InfiltratorOS kernel may use a different module ABI or extension. A UEFI driver is **not** a substitute for the kernel-side driver, and a Linux `.ko` is **not** a UEFI driver.
3. A custom, deliberately modular Linux build is the intended intermediate test environment. Individual filesystem implementations should be selected as loadable modules rather than assumed built into a distribution kernel. Modules required to mount root, plus any needed device drivers, partition parsers, dependencies and loader infrastructure, must be available before mounting root, typically inside a generated initramfs.
4. The kernel need not support the boot-payload filesystem if it never mounts it. Conversely, choosing OFS for the kernel image does **not** make an ext4 Linux root become OFS. Source and root are independent choices, though Setup may choose the same format for both once qualified.

## Future alternative to per-format EFI drivers

The current `.efi` driver sequence above describes the **existing
transitional Boot integration**, not a permanent requirement to maintain
separate format implementations. The [Drop-in Filesystem Module
Contract](DROP_IN_FILESYSTEM_MODULES.md) records the longer-term target:
a single, freestanding, self-describing filesystem `.so` per format, with a
strictly read-only interface executable by a purpose-built Boot ELF loader
and by compatible userspace consumers. This would share format semantics
with Filesystems, Defragmenter and Test Media.

An ordinary Linux `.so` cannot be passed to UEFI `LoadImage`. The
proposed ELF profile, loader, trust policy, limited firmware host services,
Secure Boot integration and cross-environment ABI **remain unimplemented and
require independent qualification**. A verified initial module copy must
be staged on firmware-readable media (normally the ESP) before Boot can
read a filesystem that contains `/System/Filesystems`; that copy is not a
second format engine. The OS's Linux root still needs kernel/early-boot
support independently. Existing proven `.efi` deployment, device-binding
and boot tests must continue until the new path passes equivalent gates.

## Capabilities rather than pretended compatibility

A disk format is a stable identity; its EFI reader, formatter, Linux module, and future native-OS module are separate deployable capabilities. A Filesystems capability profile for each target should distinguish at least:

- read-only EFI boot-payload access, driver binding and device layout (GPT/MBR partition, Amiga RDB partition, or whole disk);
- format/create and repair tooling, bounded write paths and data-integrity guarantees;
- root read/write and early mount suitability;
- path/name constraints, encoding, case handling, collision rules and maximum sizes;
- directories, symbolic links, hard links, permissions/ownership, special objects, executable and memory-mapping behaviour;
- durability, atomic rename/replacement, crash recovery and update semantics;
- exact kernel, target architecture, module ABI/signature and Secure Boot dependencies;
- explicit qualification state and known limitations.

Filesystem drivers translate their real on-disk structures to the common interface. They **must not falsely claim semantics they cannot preserve**. For example, Setup may use real directories instead of symlinks where the OS layout allows it. Duplicating a file is *not* equivalent to a hard link; simulating links or Unix permissions may require a persistent, crash-consistent metadata scheme and compatibility policy. Where root-critical semantics cannot be implemented safely, the format remains eligible for **boot-payload storage only** (or general data storage), not as a qualified system root. The installer must never silently swap the user's selected filesystem for ext4.

## Future Setup transaction

When Setup exists and the selected format/root profile is qualified:

1. Display supported formats and their real *boot-payload* and *root-volume* capabilities, with unqualified choices disabled or explicitly described as experimental.
2. Confirm the intended destructive target and partitioning scheme, format the selected storage without changing unrelated devices.
3. Install Boot to the ESP and only the required qualified `.efi` filesystem readers in its auto-loaded driver directory (with optional additional readers selected by policy).
4. Copy the kernel and early-boot assets to the selected boot-payload filesystem at paths recorded in validated Boot configuration.
5. Install the chosen OS's corresponding filesystem modules and dependencies in the appropriate runtime location. For intermediate Linux, construct/verify an initramfs that can find and mount the selected *root* filesystem independently of Boot.
6. Pass stable volume identity and root-mount policy through the kernel boot protocol / initramfs, accounting for formats without ordinary Linux PARTUUIDs. Do not assume that a firmware device path has the same `/dev/sdX` name inside Linux.
7. Verify the complete boot, filesystem mount, system update and recovery chain before declaring this installation option supported. Keep recovery paths and failure handling explicit.

The current InfiltratorOS native layout reserves `/System/Kernel/Images` and `/System/Kernel/Modules`, but the current Debian/Linux environment still uses its existing `/boot` and module locations. Setup's eventual native placement policy must be deliberate; today's test path is not a premature mandate to migrate files.

## Development phases and acceptance criteria

**Phase A — current reality:** Boot contains no filesystem implementation source or bundled EFI drivers. Filesystems provides independent ext4 and experimental OFS component payloads; the imported InfiltratorFS EFI adapter remains unshipped migration source pending qualification. Boot has loaded Linux from ext4 on the physical Dell; broader startup optimisation has not yet been physically verified. Filesystems contains independent OFS and FFS Linux modules, but their existence does not imply a qualified AmigaDOS EFI reader or root installation. No Setup installer or native InfiltratorOS kernel is claimed.

**Phase B — prove boot-source independence:** create and verify an authentic AmigaDOS OFS test image/device, implement and qualify a *read-only* OFS UEFI reader (including Amiga RDB layouts as applicable), load an EFI-compatible Linux kernel and initramfs from OFS, and successfully boot Linux with its root still on a separately supported filesystem. Show that removing the OFS EFI reader only removes that boot source, not Boot itself.

**Phase C — prove modular root selection:** compile a reproducible custom Linux kernel with selected filesystem drivers as loadable modules; assemble only necessary early-boot modules in the initramfs; prove mounting a selected qualified root, with missing-module and corrupt-volume recovery tests. A format used only as the boot source need not be included in the kernel module set.

**Phase D — future Setup:** implement the coordinated transactional installer, format-driven driver selection and install qualification. OFS/FFS and other non-POSIX roots remain unavailable as system targets until their required semantics are qualified; storing a boot kernel on those formats is a different, earlier milestone.

**Phase E — future native InfiltratorOS:** reuse Boot's filesystem-neutral loading and explicit handoff contract, adapting the kernel payload format, runtime module ABI and installer placement. The operating system is a long-term goal, not an implemented feature today.

## Verification rules

A successful compile or catalogue entry proves neither EFI binding nor root suitability. Required evidence is: valid disk-format structures; no writes from the EFI reader; bounded probing of unsupported and damaged volumes; deterministic filesystem-driver selection with multiple drivers installed; kernel/initramfs read integrity; actual EFI-stub handoff; root-device resolution after ExitBootServices; early-boot filesystem-module loading; safe mount/failure/recovery behaviour; and an end-to-end physical or equivalent boot demonstration. Tests must distinguish **kernel-file accessibility**, **kernel execution**, and **root filesystem suitability**.

See also [Native Filesystem Vision](NATIVE_FILESYSTEM_VISION.md), [Filesystem Architecture](ARCHITECTURE.md), [Amiga Filesystem Architecture](AMIGA_FILESYSTEMS.md), [Boot's EFI module contract](https://github.com/Infiltrator-Projects/Boot/blob/main/docs/ARCHITECTURE.md), and [InfiltratorOS namespace policy](https://github.com/Infiltrator-Projects/InfiltratorOS/blob/main/docs/FILESYSTEM.md).
