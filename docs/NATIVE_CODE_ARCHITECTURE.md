# Native Code Architecture

## Architectural rule

Filesystems owns one canonical implementation of each **in-scope persistent local-storage filesystem format**.

Filesystem semantics are not independently rewritten for Linux, Windows and UEFI. Each platform adapter translates its host contract around the same authoritative format rules.

```text
                         filesystem catalogue
                                |
                                v
                    canonical filesystem engine
                    /            |             \
                   /             |              \
          Linux VFS adapter   Windows IFS     EFI reader
                 |               |               |
                .ko             .sys        rEFInd/UEFI
```

Userspace inspection, formatting and qualification harnesses call the same canonical format logic through host-neutral I/O contracts where practical.

The Windows migration and preserved ExtFS-for-Windows evidence are described in [`WINDOWS_FILESYSTEM_ARCHITECTURE.md`](WINDOWS_FILESYSTEM_ARCHITECTURE.md) and [`EXTFS_FOR_WINDOWS_MIGRATION.md`](EXTFS_FOR_WINDOWS_MIGRATION.md).

## Scope rule

A native filesystem tree exists to implement a real on-media filesystem format. Provider brands, network/cloud clients, overlays, archive mounts, encrypted containers, device namespaces and pseudo-filesystems do not acquire canonical disk-format engines merely because Linux exposes them through a mount API.

A provider-specific reference may exist temporarily as research evidence, but it is not a catalogue identity and must not drive product architecture.

Examples:

- `ntfs/` owns NTFS semantics; NTFS3 and NTFS-3G are provider/reference paths.
- one APFS canonical engine owns APFS semantics; FUSE/DKMS providers are not separate filesystems.
- `fat/` and `exfat/` own their respective format semantics; FUSE providers do not duplicate them.
- NFS, SMB, SSHFS, cloud mounts, OverlayFS, BitLocker, FileVault, archive mounts and device namespaces are outside this repository's selectable disk-filesystem architecture.

## What belongs in the canonical filesystem engine

The canonical per-filesystem engine owns facts dictated by the filesystem format itself, including:

- on-disk structures, feature bits and compatibility rules;
- superblock, inode/object and directory interpretation;
- block/extent mapping and allocation rules;
- journal/transaction record formats and filesystem recovery rules;
- metadata/data checksum rules;
- filesystem-specific attributes and metadata;
- filename and namespace rules;
- corruption and range validation;
- filesystem-defined ordering and crash-consistency behaviour;
- mutation logic that is independent of a host kernel object model.

If the same rule would otherwise be implemented in Linux, Windows and EFI code, that rule belongs in the canonical engine or a mechanically shared/verified subset of it.

## What belongs in an OS adapter

The Linux adapter owns Linux-specific VFS registration, mount lifecycle, inode/file/folio/page-cache/block-device integration, Linux locking/lifetime rules, module registration and errno translation.

The Windows adapter owns DriverEntry/filesystem registration, IRP dispatch, VCB/FCB/CCB lifetime, Cache/Memory Manager integration, volume lifecycle, NTSTATUS translation and WDK packaging/signing.

A portable firmware-facing EXT2 file reader now exists in
`native/filesystems/ext2/efi/` and reuses the canonical EXT2 inode engine.
This is a read-only source component, not a loadable .efi firmware driver.
Other formats do not yet have complete firmware-facing file readers, and
none become qualified installation roots merely by adding this source.

The EFI adapter owns the small UEFI filesystem-reader surface required by the boot manager: volume recognition, safe path traversal, file open/read, metadata required by rEFInd and UEFI Block I/O integration. It is normally read-only and must not grow into a second full filesystem implementation.

A Linux `.ko` is not an EFI driver and a Windows `.sys` is not a Linux driver. The adapters are different binaries with different host contracts; the filesystem semantics beneath them remain one authority.

## Drop-in filesystem module target

The installed application-facing interface is intended to be **one
self-describing file per filesystem identity**, placed directly in
`/System/Filesystems/`, discovered without a package-manager install,
sidecar manifest, manually curated filename list or per-format subdirectory.
FAT12, FAT16 and FAT32 share reusable format source, but compile into **three
self-contained, independently installable artifacts** (`FAT12.so`, `FAT16.so`,
`FAT32.so`). Installing one cannot implicitly install or depend on either of the
others. The Filesystems Install FAT12 action places only `FAT12.so` in the
native directory. Source-code families do not define installation units.
**Target install semantics:** clicking `Install OFS` in Filesystems compiles
OFS's already-present canonical source and module adapter into a standalone
`OFS.so`, then atomically copies that single file into
`/System/Filesystems/`. Dropping a precompiled `OFS.so` into the same
folder has exactly the same installed outcome. Neither action installs a
Linux `.ko`, EFI driver, package, metadata sidecar or another filesystem;
those belong to separate, explicit host deployment workflows. Build
prerequisites are reported, not installed implicitly. This replaces the
current native-driver manager as the meaning of the application's main
`Install` selector; do not mistake the existing legacy `Install native`
kernel operation below for this target.

A compatible module exposes a versioned C ABI and independently qualified
detect, read, write, create, analyse, relocate, verify and recovery capabilities.
Defragmenter and Test Media are consumers of the *same format semantics*,
not alternative owners of duplicated format parsers. Defragmenter retains
general safety, staging, transaction and privileged write policy.

Boot's longer-term extension may use a purpose-built loader for a restricted,
freestanding ELF `.so` profile, with read-only host callbacks. That does not
mean UEFI can directly load ordinary Linux shared objects; an exact binary
ABI, trustworthy pre-root/ESP staging, and successful dual-environment tests
are prerequisites. Existing independent `.efi` firmware readers and Linux
`.ko` modules are **not removed by this design document**. The application
plugin is neither a Linux VFS adapter nor a substitute for a kernel root
driver. Preserve the working InfiltratorFS deployment during migration.

See [Drop-in Filesystem Modules](DROP_IN_FILESYSTEM_MODULES.md) for the
authoritative product contract, deduplication inventory and phased gates.

## Source layout direction

The target layout is responsibility-based:

```text
native/
  core/                         filesystem-neutral primitives
  filesystems/
    <format>/
      core/                     canonical format/semantic engine
      linux/                    thin Linux-specific adapter when needed
      windows/                  thin Windows-specific adapter when needed
      efi/                      thin read-only EFI adapter when needed
  platform/
    linux/                      reusable VFS/module infrastructure
    windows/                    reusable IFS/driver infrastructure
    efi/                        reusable UEFI/rEFInd infrastructure
    userspace/                  image/device/formatter/test adapter
```

Not every filesystem needs files under every adapter directory. Reusable platform infrastructure is preferred; filesystem-specific adapter glue exists only where the host contract genuinely requires it.

The number and names of translation units are implementation details. Do not preserve imported upstream file boundaries merely for familiarity, and do not split cohesive code merely to imitate the example tree.

## Shared engine contract

Host-neutral I/O is bounded and positioned. Filesystem parsing must not care whether bytes come from a test image, Linux block device, Windows volume, UEFI Block I/O device or fuzz buffer.

Checked decoding, arithmetic, endianness and corruption rejection should be shared where the execution environment permits it. Kernel and EFI builds must never acquire an invalid userspace dependency merely in the name of reuse.

Libraries may supply genuinely generic userspace/portable primitives, but the userspace Common library is not automatically kernel-safe or EFI-safe. Filesystem semantics, GUI code and package-manager code must remain separate.

## One filesystem implementation, independently deployable adapters

Filesystems must not become one enormous kernel module or Windows driver. Conventional filesystems remain independently deployable. Shared platform code is promoted only when its ABI/contract is proven stable and does not create unnecessary failure coupling.

During current Linux development, required canonical-core source may be compiled directly into each `.ko` rather than introducing a private shared kernel-module ABI. That is intentional.

## Reference/import and rewrite states

An in-scope disk filesystem waiting for independent implementation may retain upstream source as **reference evidence**. Such source retains original licence/provenance and must not be represented as project-authored code or compiled into production merely because it exists.

When a filesystem is promoted to rewrite state:

1. record the format contract and required compatibility behaviour;
2. freeze the reference material as evidence;
3. replace implementation units with project-authored canonical engine/adapters;
4. qualify corruption handling and read interoperability;
5. add safe mutation and recovery;
6. qualify platform adapters independently; and
7. qualify EFI/root/setup integration before enabling the format in the InfiltratorOS setup shell.

EXT2, EXT3, EXT4 and the five Amiga formats documented in [`AMIGA_FILESYSTEMS.md`](AMIGA_FILESYSTEMS.md) are current rewrite/project-native targets.

Historical research directories for technologies removed from the product catalogue are not candidates for automatic promotion. They may be deleted when they cease to provide useful engineering evidence; their mere presence does not define scope.

## Linux native installation contract

A project-owned Linux filesystem is considered natively installed only when Filesystems has installed the project's actual VFS adapter for the running kernel.

For a qualified module, **Install native** means: identify the running kernel, require matching headers, compile the canonical core plus Linux adapter, install the resulting `.ko` under an Infiltrator-owned `updates/` location, run `depmod`, load it with `modprobe`, and verify that the intended module is genuinely active.

**Remove native** must refuse unsafe unload, unload the module, remove only the Infiltrator-owned module, run `depmod`, and re-probe actual state.

If a same-named built-in/in-use stock module cannot safely be replaced, installation fails closed. Filesystems must never report the project-native implementation as active when another driver is servicing the filesystem.

Secure Boot is a normal deployment case. Project-native `.ko` files must follow the enrolled signing/MOK path where module signature enforcement is active; Filesystems must not disable Secure Boot or claim success after signature rejection.

## InfiltratorOS root/setup contract

Native Linux support alone is not enough for setup eligibility. A filesystem becomes a selectable InfiltratorOS root only after the formatter, Linux root semantics, initramfs availability and matching EFI/rEFInd reader are all qualified.

The same format identity drives the full installation plan:

```text
format target -> populate OS -> install Linux adapter -> build initramfs
              -> install matching EFI reader -> configure rEFInd -> boot test
```

A format failure must never silently substitute another filesystem.

## Windows contract

Windows consumes the same canonical format engine behind a native IFS/WDK adapter. The former ExtFS portable/WDK work is migration evidence, not a second production filesystem engine.

Production drivers follow supported Windows signing and Secure Boot trust paths. Test signing is development-only.

## Development and qualification

`fsinspect` and filesystem-specific tests should exercise canonical engines directly rather than spawning one unrelated utility per format.

Qualification must include independently manufactured fixtures, malformed input, sanitizer/static-analysis passes where applicable, cross-implementation differential checks and destructive disposable-media tests for writers. Mutation success requires an independent verification pass, and durable writes require explicit recovery boundaries.

Root qualification additionally covers format -> populate -> boot-reader access -> kernel/initramfs startup -> root mount -> ordinary workload -> update/reboot -> recovery.

If an abstraction proves wrong while the architecture is still pre-stable, fix the abstraction rather than preserving it for compatibility.
