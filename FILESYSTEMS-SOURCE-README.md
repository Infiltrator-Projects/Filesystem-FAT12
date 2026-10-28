# Filesystems

Filesystems is the InfiltratorOS filesystem capability and integration manager. It owns the catalogue of real persistent local-storage filesystem formats that InfiltratorOS can recognise, install support for and, as each format is qualified, use as the filesystem beneath an InfiltratorOS installation.

The application and repository are named **Filesystems**. **Boot** starts the
selected operating system, and **Setup** is the name reserved for the future
installer. **InfiltratorFS** remains a distinct filesystem format in the catalogue.

The authoritative source/release version is stored in [`VERSION`](VERSION). Published releases use the corresponding `v<version>` tag only after that exact source revision and its release artefacts have completed qualification.

The baseline Linux distribution is **Debian stable**. As of September 2026 that is Debian 13 "trixie". Derivatives can benefit where they retain Debian-compatible package names and APT behaviour, but the catalogue is authored and qualified against Debian first.

## Actual copied filesystem implementations

The 41-format catalogue is distinct from the source inventory. **The original
Linux implementation code for 26 additional formats is checked in under
`native/filesystems/<id>/linux/` as editable working source**, including NTFS
recovered from the historical NTFS3 source tree. These are not frozen references
and an automated upstream refresh is not permitted to overwrite modifications.

The existing nine individually managed native modules remain governed by
`data/native-filesystems.tsv`; simply having original code does not qualify a
module for installation. See
[`docs/SOURCE_BASELINE_STATUS.md`](docs/SOURCE_BASELINE_STATUS.md) for the
seven formats whose complete source baselines are still absent from this
repository. Neither design documents nor Debian provider names are counted as
copied implementation source.

## Product scope

The catalogue contains **41 real on-media filesystem formats**. A catalogue row is a filesystem format identity, not a Linux provider or a generic mount mechanism.

In scope are persistent local-storage formats such as InfiltratorFS, EXT2/3/4, XFS, Btrfs, FAT, exFAT, NTFS, HFS/HFS+, APFS, UDF, Amiga OFS/FFS/SFS/SFS2/PFS3, historical workstation formats and other genuine disk filesystems.

Out of scope are network and cloud mounts, distributed client protocols that do not define the selected local disk format, overlay/union filesystems, archive mounts, device namespaces, virtual filesystems, encryption/container layers that merely expose another filesystem, tools that are not a filesystem format, and duplicate provider identities such as `ntfs3`, `zfs-fuse`, `apfs-fuse` or `apfs-dkms`.

Provider choice is subordinate to filesystem identity. NTFS is one filesystem whether the current Linux access path is NTFS-3G, NTFS3 or a future Infiltrator-owned adapter. APFS is one filesystem whether accessed through a FUSE reference provider or a kernel driver. The user chooses a format, not an implementation brand.

The complete enumerated contract is maintained in [`docs/FILESYSTEMS_MATRIX.md`](docs/FILESYSTEMS_MATRIX.md). CI verifies that every compiled catalogue ID appears there exactly once.

## Safe format identity validation

Every currently built standalone filesystem .so carries a non-executed
`.ifsmeta` format identifier. The installed selector checks this ELF
metadata against the expected filesystem identity **before** reporting
the file as installed, preventing FAT12.so, FAT16.so, FAT32.so, OFS.so, EXT2.so or BFS.so
from being accepted merely because another binary was renamed. This is
a format consistency check, not cryptographic module authentication;
privileged loading still requires a separately defined trust policy.

## Current freestanding drop-in implementation

The source builds an independent read-only `OFS.so` and separate
`FAT12.so` and `FAT16.so` artifacts. Each FAT module retains a legacy V1
read-only API and also embeds the portable FAT engine behind the V3 API.
V3 supports long names, nested directories, reading, file creation and
allocation, writing, truncation, rename and deletion. V4 adds partition formatting, file attributes, modification
times and free-space queries; formatting checks the requested FAT width before
writing. Callers must provide
partition-relative I/O and flush callbacks and serialize access to an
exclusively held volume. Tests use disposable media; this does not establish
physical-media or crash-recovery qualification.

The FAT12 and FAT16 compiler helpers build one corresponding module into
`/System/Filesystems/`, without installing the other variant. Their EFI and
Linux module candidates have separate build and runtime qualification lanes;
they are not installed by the `.so` selector. A versioned defragmentation
interface now supplies the relocated FAT engine, its journal and recovery
operations to Defragmenter's child-process host. FAT32 includes the same
interface so existing FAT32 defragmentation can continue. See the
[FAT defragmentation ABI](docs/FAT_DEFRAG_MODULE_ABI.md).

## Current EXT2.so implementation (restricted in-place writes)

EXT2 now has an independent `EXT2.so` build and a separate Install/Remove
selector on its existing catalogue row. The binary embeds the canonical EXT2
engine, not a proxy to `ext2.ko`. Its supported operations include mount,
directory lookup/enumeration, regular-file reads and **in-place overwrites of
already allocated bytes of existing regular files** on clean compatible media.
It does not create, extend, truncate, allocate, delete, replay journals,
provide transactional recovery or replace the Linux kernel filesystem driver.
Writes require an explicit writable mount plus host read, write and flush
callbacks. All offsets are partition-relative. The compiled module and its
synthetic-disk qualification do not establish production write safety on
physical media.

OFS and the FAT32 general file API remain read-only; EXT2 and SCO BFS support restricted
in-place writes, and FAT12/FAT16 add the V3 writable interface described above. Format-specific module operation tables still differ. A unified,
portable, load-and-dispatch runtime ABI for Files, Defragmenter and Boot is
**not yet implemented**. Consequently these `.so` artifacts are not yet
interchangeable with Linux `.ko` or firmware `.efi` drivers.

## Current SCO BFS.so implementation (restricted in-place writes)

`BFS.so` is an independent freestanding module for **SCO UnixWare Boot File
System**, not BeOS BeFS. It uses repository-owned BFS on-disk semantics
to enumerate the root directory, look up case-sensitive names, read
contiguous file extents, and overwrite allocated bytes of existing files.
The helper builds the one file into `/System/Filesystems/BFS.so` from
`/Programs/Filesystems/Sources/bfs/`. This source target has a
compiler/helper path but no GTK Install/Remove button yet.

BFS writes require explicit host write and flush callbacks and refuse
interrupted relocation markers. CI tests both synthetic images and independently
formatted `mkfs.bfs` images; physical-device writes, metadata allocation,
create/rename/delete, resize, formatter, repair and defragmentation are
**not qualified**. As with EXT2, a real shared dispatcher for other
InfiltratorOS applications remains unfinished.

## Current FAT32.so implementation

The independent `FAT32.so` source implements read-only probing of actual
FAT32 media, 28-bit FAT entry traversal, root directory cluster chains,
8.3 root file enumeration, fragmented file reading and fragmentation counts.
It does not substitute FAT16 semantics. Separate FAT12, FAT16 and OFS modules
remain intact. General FAT32 file writes, formatting, nested directories and
VFAT long names remain unfinished. The separate defragmentation interface can
relocate data and recover journalled FAT32 transactions.

The FAT32 compiler helper builds and installs only `FAT32.so` under
`/System/Filesystems/`; it neither installs another FAT variant nor a
kernel or EFI driver.

## Drop-in filesystem modules — target architecture

The intended native installation model is **one drag-and-drop file per on-media
filesystem identity, directly under `/System/Filesystems/`**, following the
discoverable Calendar/Clock module model. A compatible and trusted
`FAT12.so`, `FAT16.so`, `FAT32.so`, `exfat.so`, `ofs.so` or `ffs.so` advertises capabilities to
Filesystems, Files, Defragmenter, Test Media and eventually Boot without
separate per-application registration. **Install FAT12** places only `FAT12.so`
in the directory. FAT16 and FAT32 have their own independent single-file
installations; all three reuse common FAT *source code*, not a combined
`fat.so` binary or a shared runtime dependency.

**Filesystems' target Install action is only a selector and compiler:**
click **Install OFS** to compile the existing OFS source into one self-contained
`OFS.so` and place that file in `/System/Filesystems/`. Dragging a
precompiled `OFS.so` directly into that directory has the identical result.
Neither route installs an extra package, kernel module, EFI driver, another
filesystem module or registry. **Install FAT12** similarly builds and places
only `FAT12.so`. See the full [selector/build contract](docs/DROP_IN_FILESYSTEM_MODULES.md#filesystems-is-a-selectorcompiler-not-a-driver-installer).

The filesystem-specific parsing, allocation, formatting, write/verification and
defragmentation primitives are to be consolidated from the existing Filesystems,
Defragmenter and Test Media implementations **without dropping behaviour**.
Defragmenter retains its generic safety, placement and transaction authority.

This is **a design target, not implemented drop-in support**. Ordinary Linux
`.so` files are not kernel drivers or UEFI images. Boot's proposed restricted
ELF loader must first be engineered and qualified; its initial reader must be
reachable before the root filesystem can be read. Existing Linux `.ko` and
firmware `.efi` deployment remain necessary during migration. The working
InfiltratorFS driver and physical test media must not be disturbed.

See the [Drop-in Filesystem Module Contract](docs/DROP_IN_FILESYSTEM_MODULES.md)
for the module ABI, capability model, Boot bootstrapping, privilege boundaries,
deduplication plan and qualification gates.

## Native application layout (Linux compatibility preserved)

Filesystems' **actual executable and application-owned resources** install
under `/Programs/Filesystems/`, even in the Debian/Mint package:

```text
/Programs/Filesystems/
    Filesystems
    Helpers/fat16-module-helper
    Helpers/ofs-module-helper
    Sources/fat16/
    Sources/ofs/
    Resources/EFI/
/System/Filesystems/
    FAT16.so              # only after choosing Install FAT16
    OFS.so                # only after choosing Install OFS
```

The Linux package carries a tiny `/usr/bin/filesystems` command launcher
and the desktop launcher; these make the same native application executable
discoverable on Linux without putting a duplicate copy under `/usr/bin`.
Linux-only kernel module building, source provisioning, and GNOME/udev
desktop integration currently retain their existing `/usr/lib/filesystems/`
system integration resources. Those are **not** the FAT16/OFS `.so` module
installation interface and are not assumed present on native InfiltratorOS.
The experimental EFI build products are application resources, not ESP
drivers deployed by package installation.

The selected on-demand compile action consumes only source files packaged
beside the application; it places exactly one resulting `.so` under
`/System/Filesystems/` and never formats or writes a filesystem partition.
Only explicit separate Boot/setup and Linux kernel driver workflows may
create/use `.efi` or `.ko` outputs. The current helper scripts still use
Linux/bash/compiler prerequisites; a future *native* InfiltratorOS build
backend needs its own qualified compiler/runtime implementation. This path
correction does **not** claim Linux integrations are portable kernel drivers.

## Installation namespaces and the Linux-only exception

The native InfiltratorOS Filesystems application lives in
`/Programs/Filesystems/`, including its executable, compiler helpers,
`Sources/`, and `Resources/EFI/`. The five current graphical selectors
(FAT12/FAT16/FAT32/OFS/EXT2) and the sixth independently buildable
BFS drop-in **always** install exactly one compiled module directly
under `/System/Filesystems/`, or accept that same module dropped manually.
Their compiler helpers find packaged source relative to their own installed
application location. Neither a source tree nor the compiler helper for a
selected `.so` is installed under `/usr/lib/filesystems/`.

**Mint/Debian compatibility exception:** the current Debian package also
retains a limited `/usr/lib/filesystems/` tree for Linux-specific kernel
module transactions and their Linux VFS source, manifest and removal guard.
Those objects are needed for the *existing Linux host integration*; they are
not part of the native Filesystems application layout and must not be
installed into a native InfiltratorOS image. The distro-specific
`/usr/bin/filesystems` file is only a launcher for
`/Programs/Filesystems/Filesystems`. The Linux-only helpers must never be
used as a runtime dependency by `FAT16.so`, `OFS.so` or `EXT2.so`.

This rule is checked in the source-architecture regression suite. Actual
kernel `.ko` and EFI `.efi` compilation/deployment remain independently
qualified operations: clicking **Install FAT16** or **Install OFS** is not
permission to mutate a kernel, EFI partition or boot configuration.

The package includes FAT12/FAT16 EFI binaries tested with OVMF, their Linux
kernel build sources, and the exact Linux 6.12.107 QEMU test modules. Those
reference `.ko` files are specific to the test kernel; the included
`/Programs/Filesystems/Helpers/fat-kernel-build` can build a selected module
against a compatible prepared kernel tree. See the packaged
`/Programs/Filesystems/Sources/fat/DESIGN.md` for the build and qualification
limits.

## OFS standalone module without losing kernel or EFI outputs

OFS now has a **parallel** read-only `OFS.so` drop-in adapter assembled
from the existing canonical OFS core and EFI reader. **Install OFS** in
Filesystems compiles just this `.so` and places it in
`/System/Filesystems/`; dragging in a compatible precompiled `OFS.so`
has the same result. The module currently supports a restricted subset:
512-byte DOS0/DOS2/DOS4 identification, bounded directory traversal and
read-only file access. It is not yet a full-featured Defragmenter,
formatter or Boot ELF-plugin replacement.

**Crucial:** the independently built `ofs.ko` Linux driver, experimental
`OFS_x64.efi` UEFI driver and `filesystems-ofs-image` formatter remain
available and are built/tested using their existing paths. Neither the new
Install/Remove OFS button nor the new `.so` changes those binaries,
active mounts, UEFI boot files or physical OFS partitions. Do not remove
the legacy boot/runtime outputs until an independently qualified
replacement exists.

## FAT16 standalone implementation (experimental)

FAT16 is now the **first** independently selectable module implementation.
Filesystems' **Install FAT16** action compiles the packaged,
freestanding FAT16 C source and places just `FAT16.so` in
`/System/Filesystems/`; **Remove FAT16** removes just that file.
Dropping a precompiled, compatible `FAT16.so` into that directory is
an alternative. The module does not install `dosfstools`, touch FAT12/FAT32,
call `modprobe`, replace any kernel filesystem driver or write to an ESP.

The present ABI is **read-only and limited**: geometry/BPB classification,
8.3 root-directory file enumeration, FAT16 cluster-chain reads and root-file
fragmentation counting. It is qualified against disposable synthetic media,
not yet integrated into Defragmenter's mutation engine, Linux VFS mounts,
VFAT long-name traversal or Boot's future freestanding ELF loader. No
formatting, writable FAT16 support or defragmentation claim is made yet.
The module source is under
`native/filesystems/fat/module/`. A compiler and binutils must already be
present; Filesystems will **not** install them automatically.

## Desktop integration ownership

GNOME Disks, Nemo and the native filesystem manager are distinct integration
surfaces. Filesystems only owns a `dpkg-divert` launcher when no other package
already owns that launcher. In particular, systems with InfiltratorFS's GNOME
Disks diversion retain its existing binary, formatting support and ownership
unchanged. Filesystems remains installable and its independent CLI formatter,
kernel module support and device-identification rules still work.

If another package owns a launcher, Filesystems does **not** chain or inject its
desktop preload into that application: Filesystems-specific visual presentation
inside that application is unavailable until an independently designed shared
integration interface exists. It does not compromise the other package to
provide cosmetic support. Missing or inconsistent foreign launcher state is
reported instead of overwritten. The ownership contract is regression-tested
using independent private `dpkg-divert` databases.

## Two product surfaces, one identity contract

Filesystems has different presentation contexts that consume the same stable filesystem identities while keeping platform-specific support policy separate.

**Current legacy desktop-manager behaviour (to be migrated):** it combines the identity contract with Linux support policy: it discovers support already present on the running system, installs or removes the current provider for a filesystem, loads or unloads eligible kernel modules, reports limitations and integrates supported formats with the desktop storage stack. **This is not the target Filesystems Install action.** The target action only compiles a selected self-contained `.so` and places it in `/System/Filesystems/`; runtime kernel integration is separate.

The planned **Setup** installer has its own interface. During operating-system installation it will ask which qualified filesystem should be used for the target storage and drive the installation transaction through the Filesystems setup capability contract. Windows, EFI and setup consumers use the identity-only API rather than inheriting Debian package or Linux module policy.

The intended setup flow is:

```text
choose target filesystem
        |
        v
format the target storage in that format
        |
        v
install/copy InfiltratorOS
        |
        +--> install the matching Linux VFS support
        |    and include it in the initramfs when required
        |
        v
boot the installed system
```

The agreed cross-project separation of **kernel stored on a filesystem** from **operating system rooted on a filesystem**, together with modular `.efi` / `.ko` handling, capability adaptation and phased tests, is recorded in [the Bootable Filesystem Integration Contract](docs/BOOTABLE_FILESYSTEM_CONTRACT.md).

The setup surface exposes only filesystems whose complete install path is qualified. It does not infer support merely because a catalogue entry exists or because a Linux package can be installed.

## OFS specification-completeness audit

The current OFS implementation is **not yet complete** across all documented
512..32768-byte filesystem block sizes or full unsigned 32-bit addressing.
The image formatter has experimental block-size/large-volume support; Linux,
EFI and physical formatting remain separately gated. See
[OFS specification-completeness audit](docs/OFS_SPECIFICATION_COMPLIANCE.md).

## AmigaDOS OFS partition formatting (experimental)

A deliberately explicit Linux-only mode can now format an already-created
**unmounted block partition**, not a complete disk:

```sh
sudo filesystems-ofs-image --format-partition /dev/mmcblk0p1 TestOFS --confirm /dev/mmcblk0p1
sudo filesystems-ofs-image --verify-partition /dev/mmcblk0p1
```

Those commands are **destructive** and require prior device identity
verification. The formatter refuses a whole disk, mounted/busy partitions,
non-512-byte logical sectors, unsupported sizes, and paths without matching
confirmation. It does not create a partition table, mount media, install a
kernel or provide an OFS UEFI reader. For SD-card-class devices, the experimental formatter uses the whole
confirmed partition when requested; it does not impose a 2 GiB volume limit.
Its supported range is 1 MiB to the shared 31-bit block-count boundary
(just under 1 TiB with 512-byte blocks). A sparse 120 GiB regression image
exercises extended bitmaps without allocating 120 GiB of host storage.
Actual SD-card formatting remains destructive and unqualified until
independent physical-media validation.
This is developer-preview functionality, not a general-purpose supported
physical-drive formatter. The block-partition path must be independently
qualified on expendable physical media before it can be promoted.

## AmigaDOS OFS test-image creation (developer preview)

The Filesystems package supplies `filesystems-ofs-image`, an independent
AmigaDOS OFS creator and verifier. In image mode it creates **new regular-file
images only**, refusing existing paths, symlinks and block devices, then
verifying its result before reporting success. Its separate, confirmed
partition mode is documented above. Neither mode partitions an entire drive
nor places a kernel into the filesystem.

```sh
filesystems-ofs-image --create ofs-test.img 64 TestOFS
filesystems-ofs-image --verify ofs-test.img
```

The `64` is MiB (1–1048575 supported by the current block-count architecture). These image files
are suitable for controlled VM/loopback testing. Root filesystem and Boot EFI
driver qualification remain separate. See
[Amiga filesystem architecture](docs/AMIGA_FILESYSTEMS.md).

## Support discovery

Filesystems treats support as a capability that must be observed rather than assumed.

The Linux manager scans the running system for:

- kernel-registered filesystems;
- loaded and available kernel modules;
- installed Debian packages;
- repository package availability;
- project-owned native module state;
- desktop identification/integration state.

Probe failures are explicit. A failed package, repository, module or kernel inventory is not converted into a normal "missing" result and does not enable an action based on incomplete state.

## Legacy provider installation and removal (existing implementation, not target module selector)

Install and remove operations are scoped to the selected filesystem. Project-native filesystems deploy their own kernel driver and their own desktop-identification material. Package-backed filesystems use the selected provider package without turning provider package names into new filesystem identities.

Destructive package removal is bound to the exact installed package names and architectures shown for confirmation. The installer repeats its simulation for early feedback, then an installed APT guard checks the final removal actions and Debian protection metadata under APT's frontend lock. A changed plan is refused before any package removal, including changes made while authentication was pending.

Privileged executable selection is fixed to trusted system/application locations rather than inherited from the caller's `PATH`, and privileged operations use structured argument vectors. APT's required shell-hook interface receives a fixed installed guard path with individually quoted package identities; it never receives arbitrary command text.

## Project-native Linux filesystems

The project currently owns native Linux implementations for:

- EXT2
- EXT3
- EXT4
- Amiga OFS
- Amiga FFS
- SFS
- SFS2
- PFS3

Each native filesystem is an independent deployment unit. Installing or removing one must not silently install, remove, unload or mutate another filesystem's driver.

Kernel-module replacement is transactional. If replacement fails, rollback restores both the previous persistent module image and the previous runtime selection when that module had been loaded.

## Desktop integration

Desktop identification is a separate transaction from kernel-driver deployment. It publishes filesystem identity to udev/UDisks and desktop tools without making desktop metadata authoritative over kernel support.

A successfully committed native driver is not rolled back merely because a later desktop-integration refresh fails; that partial success is reported explicitly so the failing layer can be repaired independently.

## Windows

The Windows tree contains the canonical project EXT2 driver build path and packaging/validation material. Windows support is maintained as a separate platform surface rather than by importing Debian/Linux package policy.

## Build

A normal Linux build uses CMake/Ninja:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Release qualification additionally exercises project-native filesystem builds, sanitizers/adversarial parser tests, runtime qualification, packaging, desktop-identification validation and Windows EXT2 code analysis.

## Packaging

The release produces one Debian package and one self-extracting `.run` installer. Per-filesystem desktop templates are carried inside the main package and activated only for the selected filesystem; there is no supplemental desktop/UDisks Debian package.

The central Infiltrator repository publication path accepts only an exact qualified release revision and verifies the advertised application/version after publication.

## Architecture documentation

Key contracts are documented under `docs/`, including:

- `FILESYSTEMS_MATRIX.md` — enumerated filesystem contract and status;
- `DROP_IN_FILESYSTEM_MODULES.md` — drag-and-drop single-file modules, shared Defragmenter/Test Media capabilities and future Boot loader;
- `BRITTLENESS_REDUCTION.md` — ownership, transaction, coupling and failure-reporting boundaries;
- platform/build/runtime qualification documents and implementation notes.

The implementation deliberately keeps filesystem semantics, support policy, privileged mutation, desktop presentation and setup capability as separate responsibilities so that adding or repairing one filesystem does not create hidden coupling to unrelated formats.

[Validation](docs/VALIDATION.md) defines local tests, mounted-runtime evidence and immutable artifact publication. [PHILOSOPHY.md](docs/PHILOSOPHY.md) owns the engineering standard.

## EFI filesystem components

Firmware filesystem implementations are maintained by Filesystems, independently
of Boot. The migrated adapters and provenance are documented in
[`firmware/uefi/README.md`](firmware/uefi/README.md). The Debian release carries
the ext4 reader and the separately marked experimental OFS reader under
`/Programs/Filesystems/Resources/EFI/`. Installing Filesystems does not copy modules into an
EFI partition or alter a boot-manager installation. Setup, or an explicit local
test, selects the required drivers for its chosen target directory.
