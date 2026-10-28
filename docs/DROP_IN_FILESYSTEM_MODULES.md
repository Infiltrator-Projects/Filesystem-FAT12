# Drop-in filesystem modules: target product architecture

**Status: agreed architectural direction, not a claim of implemented runtime support.** This is the target for Filesystems, Boot, Defragmenter, Defragmenter Test Media, Files, and Setup. Existing filesystem modules, qualified Boot drivers, Linux mounts and physical partitions must remain untouched until their replacements have independent qualification.

## User-facing contract: one file, drag and drop

The native location is **`/System/Filesystems/`**, with **one regular file per filesystem identity**, directly inside that directory. No filesystem-specific subdirectories, sidecar manifests, registration databases, package-install commands or manual application registration are required to *discover* a drop-in module. This deliberately follows the direct module-discovery concept used by Calendar and Clock resources.

~~~text
/System/Filesystems/
    infiltratorfs.so
    ofs.so
    ffs.so
    FAT12.so
    FAT16.so
    FAT32.so
    exfat.so
    ext4.so
    ntfs.so
    ...
~~~

An ordinary user places `ffs.so` in that directory. After the file has finished copying, the system verifies it, discovers the filesystem identity and available operations from the file itself, and exposes only the verified capabilities to applications. Removing it withdraws new discovery without interrupting active use. A replacement is activated only after complete staging and verification. A module cannot register itself as a second identity for the same on-media format or override a currently active module without explicit selection policy.

**Presence is not execution permission or proof of support.** An untrusted or malformed binary is never executed merely because it has an `.so` suffix. A module can be listed as present but incompatible, untrusted or unqualified, with a clear diagnostic. A module must not gain privileged disk-write, kernel-load, or firmware-execution authority solely through drag and drop.

The installed folder is a **capability source**, not the same thing as kernel registration. **FAT12, FAT16 and FAT32 are independently selectable, independently installable files: `FAT12.so`, `FAT16.so`, and `FAT32.so`.** They may be grouped as the classic FAT *source-code family*, but there is no combined `fat.so` installation. Clicking **Install FAT12** puts **only** `FAT12.so` in `/System/Filesystems/`: no FAT16 or FAT32 file, registration, runtime dependency or automatic installation. Each binary contains all shared format code it needs. `exfat.so` is a separate format. Amiga OFS (`DOS\0`) and Amiga FFS (`DOS\1`) are distinct identities and must never masquerade as one another. InfiltratorFS is one identity with one module; its format is owned by its separate repository.

## Current engineering implementation (0.5.94)

The source currently builds four standalone **read-only** shared objects:
`OFS.so`, `FAT12.so`, `FAT16.so` and `FAT32.so`. Their existing
portable C interfaces provide format checks and limited directory/file
reads, not formatting, writable mounting, defragmentation or filesystem
repair. FAT12 and FAT16 reuse one implementation source but compile as
separate files; FAT32 separately implements 28-bit FAT and a clustered
root directory, and OFS retains its own independent AmigaDOS reader.

The Filesystems GTK UI exposes the three FAT module selectors in the
same classic FAT row, plus OFS's distinct selector. The selected module
is compiled on demand and atomically placed under `/System/Filesystems/`;
no ordinary selector action installs a kernel or EFI driver. Current
drop-ins include embedded, non-executed format identifiers that prevent
one format's .so from being accepted merely because it was renamed.
This is not cryptographic authentication or a UEFI module loader.

The planned general-purpose shared-module ABI, FAT32 nested folders,
long filenames, write/format/defrag operations, shared Defragmenter/
Test Media integration and FFS.so remain incomplete. Earlier phase
plans below describe desired capabilities beyond the current
read-only implementation and must not be read as already complete.

## Filesystems is a selector/compiler, not a driver installer

The target Filesystems **Install <format>** action is deliberately a
one-artifact build-and-copy operation. It must be functionally equivalent
to the user dragging a precompiled, compatible module into the folder.

```text
Click "Install OFS" in Filesystems
  -> select the OFS source tree already owned by the project
  -> compile its canonical core + agreed standalone module adapter
  -> produce one self-contained OFS.so
  -> validate the complete output
  -> atomically place OFS.so directly in /System/Filesystems/
  -> report success
```

```text
Drag an already compiled OFS.so into /System/Filesystems/
  -> detect the completed file and validate it
  -> expose its advertised capabilities
```

**Both workflows end with exactly the same file in the same directory.**
The selector must not install other filesystem files, packages, Linux
kernel modules, EFI drivers, DKMS registrations, udev rules, desktop
integrations, NVRAM changes or additional registries. **Uninstall** for
this target removes only the selected module file once it is safe to do
so. No action on another filesystem's module is implicit.

Source-code sharing is an engineering/build-time concern: each selected
module must contain its needed common-format code and have no implicit
runtime dependency on a different format's `.so`. For example, selecting
**Install FAT12** compiles the FAT12-specific build from the common FAT
source and places **only `FAT12.so`**, never FAT16/FAT32.

Actual compilation may have prerequisites supplied by the development
environment, but those prerequisites are not permission for the Install
button to mutate the operating system. Report missing compiler or
dependencies as an actionable build error rather than silently using APT
or deploying another driver. A failed build must leave any previously
working module unchanged.

This is the **intended replacement** for the current package/provider
and kernel-driver-management behaviour described elsewhere in this
repository. That older behaviour is the current implementation, **not**
the desired meaning of the future Filesystems *Install* button.
Separate kernel/root-driver and Boot ESP staging responsibilities, where
necessary, belong to their own explicit workflows. These downstream
runtime limitations must not be disguised as steps of the selector.

## One authoritative implementation; several consumers

Each drop-in file exports a **versioned, C-compatible filesystem module ABI** and embeds the metadata needed for safe discovery. The filesystem-specific semantics originate in one canonical source implementation, not independently maintained parsers in several applications. Consumers call only their supported operations, not a monolithic all-or-nothing driver.

| Capability (independently advertised and qualified) | Examples of consumers |
| --- | --- |
| Identify format and variants, validate geometry, report constraints | Filesystems, Files, Defragmenter, Setup, Boot |
| Read directory/file content via bounded positioned I/O | Boot (read-only), offline tools, Files when appropriate |
| Create/write/rename and maintain format-specific metadata | Filesystem access services where permitted |
| Format/create volume, populate fixtures, check/verify | Filesystems, Setup, Defragmenter Test Media |
| Expose allocation graph, extents/clusters, pinned metadata and fragmentation | Defragmenter |
| Plan/execute format-specific relocation and metadata updates | Defragmenter behind its privileged safety engine |
| Verify and recover a format-specific interrupted operation | Defragmenter and filesystem check/recovery tools |
| Create known fragmented media and test patterns | Defragmenter Test Media on disposable targets |

A module may support detection only, read-only inspection, formatting or defragmentation independently. It advertises exact format variants and geometry limits, supported write operations and qualification level; absent or unqualified operations are **disabled**, not silently emulated or delegated to another format.

The module ABI must define: format ID and variants; ABI version/size and architecture; feature/capability bitset; immutable machine-readable description and limits; opaque per-volume handles; bounded positioned host-I/O callbacks; allocation, memory, logging and cancellation callbacks; explicit read-only versus write authority; deterministic errors; object ownership and lifetime rules; and version negotiation. The exposed metadata is parsed **without first executing arbitrary code**, and is authenticated before enabling privileged functionality. The binary carries its own metadata (no sidecar registry).

The single-file requirement applies to the **installed application-facing format module**. Source code may remain multi-file, and host adaptations are compiled for their execution environments. It does **not** imply that a Linux `.ko`, Windows `.sys`, or firmware image is interchangeable with an ordinary Linux shared object.

## Boot: load the same module instead of a separate filesystem implementation

**Target:** Boot should be able to discover and run a compatible filesystem's read-only interface from the same **freestanding drop-in module artifact**, without requiring a separately implemented per-format `.efi` filesystem parser. Boot itself remains the firmware-loadable EFI application.

That requires a deliberate Boot-controlled loader for a **restricted, position-independent ELF module profile**, not UEFI's normal `LoadImage` operation on a Linux/glibc `.so`. The ELF profile must be specified, including architecture, relocations, symbol imports, memory permissions, runtime entry points, absence of glibc/syscalls/TLS assumptions, and firmware-compatible host services. The same binary may be used by Linux userspace **only if both loaders and its dependencies satisfy the identical ABI/profile**; feasibility must be demonstrated by a working cross-environment proof before promising a universal binary.

Boot provides bounded block I/O, memory and diagnostics and exposes **read-only** filesystem operations. It never loads defrag, formatter or mutation interfaces with write authority. Boot still owns device discovery, verified module loading, boot selection, kernel/initramfs loading and handoff; format semantics remain within the filesystem module.

**Bootstrap:** firmware must read Boot and its initial module from storage it can already access. The chosen root filesystem cannot be used to load its own first reader. Setup therefore stages a **verified copy of the same module** on the ESP (or supplies an independently qualified minimal bootstrap path) before Boot can read `/System/Filesystems` on the target. That staged copy is not a second implementation or a second user-facing registration. Boot must bind the selected format and module version deterministically and protect Secure Boot trust assumptions. Adding a module must not imply automatic NVRAM, ESP or firmware changes from an ordinary file-manager drag and drop.

Until the new loader and ELF ABI pass qualification, **existing independent `.efi` filesystem drivers remain the supported/experimental transitional mechanism**. The present Boot runtime contract remains UEFI Driver Binding + EFI Simple File System; this document does not claim that Boot currently loads `.so` files.

## Running OS: kernel and application execution are different

A Linux application can load a compatible userspace module with its declared ABI. A normal mounted Linux filesystem continues to require a qualified Linux VFS provider (built-in driver, `.ko`, or appropriate userspace filesystem service). A standalone `.so` is **not** a Linux kernel driver, cannot automatically mount the host root, and does not replace `modprobe`/DKMS, initramfs placement or Secure Boot module signing.

The existing `/System/Kernel/Modules` native namespace and Linux's active module deployment stay independent of `/System/Filesystems`. A module may expose raw-image and offline file access to tools before native mounted support is qualified. Report these capabilities separately; never imply that Boot can load a kernel from a volume merely because Linux can mount it, nor imply root suitability from a successful Boot read.

Windows likewise requires its own native integration where appropriate; do not claim a Linux ELF module directly substitutes for a Windows filesystem driver.

## Defragmenter and Test Media: eliminate format duplication, not safety

There is material duplicate format logic today:

- Filesystems owns native FAT, exFAT, OFS, FFS and other source trees and host adapters.
- Defragmenter has its own per-format native engines, including FAT geometry, cluster/chain and directory handling, exFAT bitmaps/checksums, and combined Amiga AFFS parsing.
- Defragmenter Test Media contains independent format creators, payload populators and verification routines.
- Defragmenter's current worker registry explicitly lists filesystem-specific executables; it does not yet use a drop-in Filesystems module registry.

**Migration direction:** identify equivalent functions and reconcile their behaviour and edge-case handling, then move common format interpretation, allocation metadata and filesystem-specific mutation primitives behind the canonical per-format module. Defragmenter's **generic** safety/privilege controls, target-identity verification, unmounted-device exclusion, placement policy (including growth reserve), staging, durable journal orchestration, stop/cancellation and final commit governance stay in Defragmenter. The filesystem module supplies only its qualified format-specific analyses, relocation primitives, verification and recovery hooks. No module gets direct arbitrary block-device authority from a generic plugin call.

Test Media may use the module's format/create/populate API, but independent fixtures and verifiers must be retained as **external correctness oracles**; never accept a writer's own checksum as the sole proof that its output is correct. Any destructive operation still requires a confirmed sacrificial target and a stable target identity.

The first migration candidates are **three independently installable modules `FAT12.so`, `FAT16.so`, `FAT32.so`**, built from shared FAT source with independently verified 12-, 16- and 32-bit format rules; followed by exFAT (`exfat.so`), OFS (`ofs.so`), and FFS DOS1 (`ffs.so`). FAT16 is the easiest first engineering proof; that build order must not cause FAT12 or FAT32 to be installed. The AFFS worker's OFS/FFS distinction must survive splitting into separate format identities. The existing working InfiltratorFS filesystem, module, EFI boot path and physical partition **must not be modified as the test platform** for the new plugin architecture; integrate it only after equivalence tests on disposable images and separate qualification.

Do not delete existing format writers, fixers, tests or working native modules until the replacement covers their documented functionality and independent runtime evidence. One canonical source does not mean accepting a regression or dropping a feature.

## Discovery, update and privilege contract

1. List only regular candidate module files immediately under `/System/Filesystems`; no recursive provider directories or magic filenames outside it.
2. Wait for a completed/atomically replaced file. Open by file descriptor, reject symlinks or unexpected ownership/mode, bound all ELF and metadata parsing, and guard against path replacement between validation and load.
3. Verify trusted identity, integrity/signature, architecture, compatible ABI, declared format identity and capabilities **before executing module code**. On systems where the directory is user-writable, privileged consumers must require an independent trust policy; a writable folder is not a root/firmware plugin authorization channel.
4. Advertise real qualification independently for read, write, format, defrag, recovery, test-media creation, EFI read and mounted/root support. Discovery must never trigger formatting, driver replacement or raw writes.
5. Use a consistent snapshot of the selected module for each active operation. Rename/removal blocks new sessions, while active handles remain valid until safely released. Updating one filesystem does not change another's module or active transactions.
6. Surface explicit diagnostic state for missing, corrupt, untrusted, incompatible, unsupported and in-use modules; no silent fallback or different format substitution.
7. Maintain an independent recovery path if filesystem-module discovery fails; a bad plugin must not make Boot or Filesystems unable to recover.

## Qualification and migration gates

**Gate 0 — source inventory:** catalogue duplicate format-specific responsibilities across Filesystems and Defragmenter/Test Media, with concrete tests and source owners. Treat format identity separately from Linux provider identity.

**Gate 1 — stable module ABI:** document the C ABI, ELF subset, self-description, trust model, host I/O, version negotiation and capability flags. Build a userspace discovery harness and prove that one dropped file is enough for application discovery without an installer.

**Gate 2 — FAT16 proof:** implement self-contained `FAT16.so` first, test detection, reads, writes (where qualified), create/verify and defragmentation on disposable FAT16 images. Prove that Defragmenter and Test Media use it without losing independent verification. Then build and separately qualify `FAT12.so` and `FAT32.so` from the shared FAT source. Test that installing or removing FAT12 affects *only* `FAT12.so`, never FAT16 or FAT32. Ensure filesystem discovery and the Filesystems **Install FAT12** action select the exact module file, not a family aggregate.

**Gate 3 — exFAT and Amiga:** qualify `exfat.so`, `ofs.so` and `ffs.so` independently, including DOS variant and short-name behaviour and recovery. Preserve existing working independent Linux drivers.

**Gate 4 — Boot loader:** implement a restricted ELF loader in Boot on a test branch, load the same freestanding file on UEFI and in the userspace harness, reject malicious/unsupported binaries, read known kernel/initramfs hashes from a disposable volume, then test firmware boot and Secure Boot policy. Maintain the existing `.efi` fallback until a separate acceptance decision.

**Gate 5 — deployment:** integrate the drop-in discovery across Filesystems, Files, Defragmenter, Test Media and Setup as each product is qualified. Establish a trusted ESP staging/update path, root-driver/initramfs availability, rollback and physical boot qualification.

**No gate is implicitly passed because code exists, a module compiles, or a Linux driver mounts a volume.** Current source trees and the 41-format catalogue are not evidence that all 41 drop-in modules are implemented. Do not publish the new ABI as production-ready or change active InfiltratorFS/OFS boot deployments until the applicable gates pass.

## Cross-project ownership

- **Filesystems** owns this module contract, format identities, canonical format engines and installed discovery.
- **InfiltratorFS** owns its filesystem format and canonical engine; Filesystems integrates its published module without introducing a competing copy.
- **Boot** owns its firmware runtime, future restricted ELF loader, staged module trust and read-only boot services.
- **Defragmenter** owns general defrag policy, unmounted-device protection, privileged lifecycle, staging and recovery orchestration; Test Media owns test workflows and independent qualification.
- **Files** consumes filesystem capabilities and the OS mount interfaces; it does not fork disk-format logic.
- **Setup** coordinates target filesystem choice, formatter, boot staging, kernel/initramfs and driver installation after each capability is qualified.
- **InfiltratorOS** owns the native installation namespace `/System/Filesystems`.

Related current contracts: [Native Code Architecture](NATIVE_CODE_ARCHITECTURE.md), [Bootable Filesystem Integration Contract](BOOTABLE_FILESYSTEM_CONTRACT.md), [Filesystem Matrix](FILESYSTEMS_MATRIX.md), [Native Filesystem Vision](NATIVE_FILESYSTEM_VISION.md).
