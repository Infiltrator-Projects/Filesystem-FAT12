# Changelog

## 0.5.97 — 2026-10-27 (independent SCO BFS.so)

- Centralise the Mint/Debian native-kernel-module coordinator and removal-guard lookup in an explicitly Linux-only host adapter; remove Linux directory assumptions from application probe and installer. Add source/layout and helper-resolution regression tests. No packaging or physical system change.

- Implement a separate SCO BFS filesystem module using repository-owned on-media semantics: root directory enumeration, case-sensitive lookup, bounded contiguous reads and restricted in-place overwrites of existing allocated bytes. The module requires a host flush callback for writes.
- The safe write policy refuses any incomplete BFS relocation markers, including an individually set source or destination marker.
- Add trusted one-file build/install helper, packaging, independent synthetic regressions and real mkfs.bfs compatibility tests on disposable CI images. No allocation, create, delete, full filesystem mounting, format, kernel, EFI or physical-media qualification is claimed.

## 0.5.96 — 2026-10-27 (first standalone EXT2 engine adapter)

- Introduce real EXT2.so with independent ABI and canonical EXT2 directory lookup/enumeration, regular-file reads and *constrained* overwrite of existing, allocated file data. Writes require a clean supported volume, explicit read/write device callbacks and a flush callback; no create, resize, sparse allocation, metadata mutation, journalling or transactional crash recovery is claimed.
- Compile/link the same freestanding module source in CI and through a trusted, application-relative compiler helper. Install only the requested EXT2.so under /System/Filesystems, using the /Programs/Filesystems source/helper namespace.
- Add read-only and bounded-write synthetic-image regression qualification and a separate graphical EXT2.so selector while retaining independent Linux ext2.ko management. No EFI, root filesystem, partition formatting or live disk changes.
- This is a fifth standalone .so build target; it is NOT a complete 41-filesystem shared-module implementation.


## 0.5.95 — 2026-10-27 (native application-path regression boundary)

- Protect the native `/Programs/Filesystems` application layout and `/System/Filesystems` one-module destination with source-architecture regression checks.
- Prohibit FAT12/FAT16/FAT32/OFS selector compiler helpers, packaged portable format source, or standalone .so outputs from being installed under `/usr/lib/filesystems`. The Linux-specific kernel transaction source/helpers and Debian package-removal guard retain their existing distro-only compatibility paths; the Mint package continues to work.
- Explicitly document why `/usr/lib/filesystems` exists only as a Linux packaging exception and must not be created for native InfiltratorOS.
- No physical disk, boot-sector, EFI, Linux driver or mount changes.

## 0.5.94 — 2026-10-27

- Correct README and drop-in module design-state descriptions to match verified implementation: independent FAT12/FAT16/FAT32/OFS read-only .so artifacts, graphical classic-FAT selectors and embedded format checks.
- Explicitly preserve unfinished FFS.so, write/format/defrag, general shared-module ABI and EFI Boot loading milestones rather than treating planned features as delivered.


## 0.5.93 — 2026-10-27

- Embed explicit non-executed per-format ELF metadata into OFS.so, FAT12.so, FAT16.so and FAT32.so, and require format identity during install/status validation.
- Reject a wrong-format .so renamed under another filesystem identity; add cross-format substitution regression tests on disposable runners.
- Extend dedicated fast .so CI to validate every compiled ELF format identity without loading the binary.


## 0.5.92 — 2026-10-27

- Correct Bash brace delimiting in the rapid .so CI driver-source paths under set -u, so module build and read-only regression tests execute rather than exiting before compilation.


## 0.5.91 — 2026-10-27

- Add dedicated rapid CI compiling and testing actual OFS.so, FAT12.so, FAT16.so and FAT32.so artifacts independently from lengthy Linux kernel and EFI build pipelines.
- Validate strict C compilation, real read-only filesystem regression fixtures, ELF shared-object entry points, zero runtime library dependencies and on-demand compiler helper shell integrity.


## 0.5.90 — 2026-10-27

- Expose independent FAT12 and FAT32 Install/Remove controls alongside FAT16 in the existing classic FAT catalogue row, without adding duplicate filesystem catalogue identities.
- Probe each .so helper independently; mark missing/untrusted providers unavailable and disable actions while scanning.
- Route each graphical button to its own fixed-purpose privileged compiler helper that mutates only its selected file under /System/Filesystems. No .ko, EFI, another FAT variant or APT provider is installed.
- Retain FAT12/FAT16/FAT32 read-only module capability boundaries. FAT32 native .so qualification remains subject to CI.


## 0.5.89 — 2026-10-27

- Repair strict GCC/Clang -Werror compilation of the independent FAT32 read-only regression fixture. Keep the reader and installer unchanged while rerunning qualification.


## 0.5.88 — 2026-10-27

- Add a standalone read-only FAT32.so with real 28-bit FAT and clustered root-directory traversal instead of a renamed FAT16 implementation.
- Add independent FAT32 volume, root directory, file reading and corruption tests; build/install/drag-in smoke tests and on-demand source packaging.
- Keep FAT12/FAT16/OFS unchanged; FAT32 GUI, formatting, writing, nested navigation, EFI and defragmentation remain unqualified.


## 0.5.87 — 2026-10-27

- Build independently selectable FAT12.so from a shared freestanding classic FAT core also used by FAT16.so; correctly decode packed FAT12 table entries and reject incorrect format and reserved FAT data.
- Preserve the original FAT16 read-only ABI using a thin variant wrapper and harden the source trust checks in both standalone compiler helpers.
- Add independent FAT12 read/corruption tests and disposable-runner compiler, drag-in and removal checks; package each variant's full self-contained source set.
- Neither variant automatically installs another FAT format, .ko or EFI. FAT12 GUI action and FAT32.so are still outstanding.


## 0.5.86 — 2026-10-26 (native application resource layout)

- Install the Filesystems application binary under `/Programs/Filesystems/Filesystems` instead of `/usr/bin/filesystems`, with a tiny Linux-only `/usr/bin/filesystems` launch shim for Mint/Debian desktop compatibility.
- Move the FAT16 and OFS on-demand build helpers and canonical source inputs under `/Programs/Filesystems/Helpers/` and `Sources/`. Configure app resource lookup from the one application-root contract, not fixed `/usr/lib/filesystems/*-source` locations. Helpers use source paths relative to their own installation directories and check root-owned trusted source trees before privileged compilation.
- Keep staged EFI reader binaries under `/Programs/Filesystems/Resources/EFI/`, without touching firmware or ESP content. Linux-specific native-kernel, udev and GNOME integration resources stay in existing `/usr/lib/filesystems/` because they are actually required for the Mint/Debian integration workflow; native InfiltratorOS does not inherit those paths.
- Preserve exactly one generated FAT16.so/OFS.so under `/System/Filesystems/` for each clicked Install. Existing Linux `.ko`/EFI driver build pipelines and partitions remain untouched.
- Extend CI package, source-layout and disposable smoke tests. This changes packaging/layout only and **does not** claim complete FAT16/OFS filesystem writing, formatting or ELF Boot-module support.

## 0.5.85 — 2026-10-26 (OFS standalone module candidate)

- Add the freestanding, read-only `OFS.so` module, compiled from existing canonical OFS core primitives, DOSType classifier and freestanding OFS EFI reader source. The new module is NOT a second independent disk parser.
- Wire Filesystems **Install OFS** to compile and atomically place only `/System/Filesystems/OFS.so`; **Remove OFS** affects only that file. Already-compiled `OFS.so` can be placed manually. No APT, DKMS, modprobe, ESP writes or other filesystem modules.
- Keep existing `ofs.ko` Linux driver generation, `OFS_x64.efi` EFI build, current OFS formatter and their test suites completely intact; none is replaced by the new .so.
- Expose only presently validated read-only 512-byte DOS0/DOS2/DOS4 module features (identify, directory enumeration/lookup, file reads). This does not imply complete OFS specification support, 512–32768-byte geometry support in the .so, write or format support in the .so, Defragmenter relocation or new Boot ELF-module loading.
- Add an independent disposable-image ABI fixture and isolated GitHub-hosted select/compile/install/manual-copy smoke test. No physical disks or boot installations are touched.

## 0.5.84 — 2026-10-26 (FAT16 drop-in implementation candidate)

- Replace the FAT family `dosfstools`/kernel-provider selector in Filesystems with an independent FAT16 entry whose Install/Remove button compiles/removes only `/System/Filesystems/FAT16.so`, with no APT, DKMS, modprobe or other FAT variant.
- Add source-packed FAT16 module, adapted from the validated Defragmenter FAT geometry/entry/chain rules and FAT Test Media. The current ABI qualifies read-only detection, 8.3 root file enumeration, raw cluster-chain file reads and root-file fragmentation counting on disposable fixture media only. It does not claim full LFN/nested-directory traversal, formatting, mutation, defrag relocation or EFI Boot compatibility.
- Package the FAT16 C source and fixed-purpose privileged build helper. Only clicking Install compiles it; package installation never activates/installs the module. Existing working kernel modules and InfiltratorFS partition are unchanged.
- Add module and selector-policy tests, including noncontiguous file chains and corruption rejection. No raw block-device writer is exposed.

## 0.5.83 — 2026-10-26 (selector/compiler architecture clarification)

- Specify the exact Filesystems target Install workflow: selecting a filesystem compiles only its existing source to one self-contained `.so` and atomically places only that file in `/System/Filesystems/`.
- Dragging in a precompiled module is explicitly equivalent. The selector does not install APT packages, DKMS, Linux `.ko`, EFI drivers, udev registrations or additional filesystem modules.
- Distinguish target UI install semantics from legacy native-driver/package-management functionality, which remains unchanged while module discovery and Boot ELF loading are developed and qualified.
- Documentation only: no runtime driver, Boot, Defragmenter or InfiltratorFS changes.

## 0.5.82 — 2026-10-26 (architecture clarification)

- Correct the single-file drop-in architecture: FAT12, FAT16 and FAT32 may share implementation source, but they are **three independent, self-contained modules** (`FAT12.so`, `FAT16.so`, `FAT32.so`), not one combined `fat.so` package.
- Specify that Filesystems **Install FAT12** places only `FAT12.so` in `/System/Filesystems/` and introduces no FAT16/FAT32 dependency or installation. FAT16-first refers to development sequence only.
- Documentation only: existing driver binaries, tests, Boot deployments and active InfiltratorFS volumes are untouched.

## 0.5.81 — 2026-10-26 (documentation/architecture contract)

- Document the original drag-and-drop Filesystems design: one self-describing, discoverable filesystem module file directly under `/System/Filesystems/`, with versioned, independently qualified capabilities for Filesystems, Files, Defragmenter, Test Media and later Boot.
- Record a shared-format-code migration plan for FAT12/16/32, exFAT, OFS and FFS; consolidate duplicate readers, allocation logic, formatters and test-media creators without removing working code, format-specific safety, crash recovery or independent tests.
- Define the prospective freestanding ELF `.so` Boot loader and required read-only, firmware-safe ABI/trust/ESP bootstrapping checks. The working Linux `.ko`, EFI `.efi` and InfiltratorFS deployments remain unchanged; the new loader and modules are **not implemented by this documentation update**.

## 0.5.79 — 2026-10-24 (development candidate; DOS0 NOT feature-complete)

- Add `native/filesystems/ofs/DOS0_COMPLETION.md`: explicit authentic DOS0 format, Linux read/write, EFI read, root boot, hardware, recovery and independent interoperability requirements. Do not conflate this checklist with proof of completion.
- Correct Linux OFS's inherited signed-31-bit partition and file-block-key restrictions: unsigned filesystem key fields and block-buffer readers, checked root-option parsing and reporting, checked UINT32_MAX partition geometry, and high-block-key primitive tests.
- **Still gated:** actual >1-TiB Linux mount/data operations on disposable media, Linux filesystem blocks over PAGE_SIZE, EFI blocks over 512 bytes, real OFS Linux-root boot and independent AmigaDOS compatibility. No physical-media or firmware configuration changes.
- Consolidate the complementary, individually green work of PRs #30, #32 and #34 into one checked Linux DOS0 candidate: unsigned BE32 block-key handling from #32; durable static ELF execution after sync/unmount/remount from #30; populated DOS0/2 namespace and data-chain image verification and extended DOS0 read/write QEMU coverage from #34.
- Real QEMU now requires `AMIGA-OFS-MULTIEXT-REMOUNT-PASS` and `AMIGA-OFS-DOS0-EXTENDED-PASS` plus host-side populated image allocation-graph verification. Preserve pre-existing crash, large bitmap, desktop-user, FFS and other filesystem gates.
- Separate canonical DOS0 acceptance contract from current implementation evidence; retain original compatibility and EFI/large-block/root-boot/physical safety restrictions. All combined-code checks must run again before merging.

## 0.5.78 — 2026-10-24 (candidate; not released)

- Canonical OFS specification now covers all four real DOS0, DOS2, DOS4 and **DOS6 LNFS** profiles, with original-source LNFS NaC, overflow-comment and root fields. Migrate explicit FFS variant identifiers to `native/filesystems/ffs/FFS_SPECIFICATION.md`.
- Disposable OFS image formatter now accepts `--variant dos6` and writes/validates the LNFS root DOSType and bitmap-derived used-block count, with seven-size regression and damage tests. Earlier DOS0/2/4 output defaults and physical safety gates stay intact.
- **Not complete DOS6 support:** Linux OFS and EFI classic-name readers remain unable to read LNFS entry names. Physical DOS6 formatting is deliberately refused until those readers, native read/write namespace operations, comment blocks and external compatibility tests are implemented and qualified.

## 0.5.77 — 2026-10-24 (candidate; not yet released)

- Record the OFS specification audit before source changes in `docs/OFS_CORRECTNESS_REMEDIATION_2026-10-24.md`, including unresolved Linux large-block and capacity support.
- Correct the AmigaDOS root key for odd-block OFS volumes using a shared, unsigned overflow-safe primitive in image formatter, Linux driver and EFI reader; add disposable odd-block image creation and all-seven-size regression tests.
- Extend the experimental 512-byte EFI reader to identify DOS\\2 international case folding and read-only DOS\\4 directory-cache media using authoritative directory hashes, while rejecting FFS DOS types.
- Qualify the project's custom OFS folio-backed executable mapping by requiring real ELF execution from disposable OFS in QEMU before removing the former automatic SB_NOEXEC restriction. This does not qualify Linux-root booting.
- Keep physical media formatting restrictions, storage-driver diagnostics and full 8/16/32 KiB EFI/Linux support gates unchanged; do not claim full-specification qualification.
- Extend OFS formatter and image verifier with explicit --variant dos0|dos2|dos4. DOS4 creates a real empty root cache block and reserves its allocation bit; add seven-block-size per-variant and corruption tests. DOS0 remains the default and no physical geometry safety gate is relaxed.
- Add a comprehensive source-independent canonical format specification to native/filesystems/ofs/OFS_SPECIFICATION.md, describing root, boot, bitmap, directory, links, OFS data and variants.

## 0.5.76 — 2026-10-24

- Audit real OFS versus FFS invariants and document the outstanding full-specification qualification gaps, including the artificial signed 31-bit limit and 512-byte hardcoding.
- Extend the **disposable image-only OFS formatter** to the seven documented 512..32768-byte block sizes and unsigned 32-bit on-media block geometry. Preserve 512-byte default output and independently verify layout under each geometry.
- Add sparse image tests beyond 1 TiB and malformed geometry inputs. Keep destructive partition formatting deliberately confined to its previous 512-byte, 31-bit domain until the separate Linux and EFI adapters are qualified; no NVMe device or boot configuration changes.
- Do not claim OFS specification completeness, EFI driver compatibility with new image geometries, or a completed physical boot test.


## 0.5.75 — 2026-10-24

- Use DiskIo for initial uncached 512-byte reads as well as sequential cached reads, matching the existing EFI filesystem framework transport even if the first kernel data sector stalls. A rejected single-sector DiskIo request permanently selects the original BlockIo path; never repeatedly probe DiskIo after an error.

- Investigate the filesystem-driver differential rather than assuming the Dell RTS525A storage controller is defective. The existing EFI filesystem framework (`fsw_efi.c`) successfully supports 128 KiB cached `EFI_DISK_IO.ReadDisk` requests; OFS previously used only 512-byte uncached `EFI_BLOCK_IO.ReadBlocks`.
- Add a bounded, read-only OFS DiskIo cache experiment that activates only for consecutive sector reads. Limit each prefetch to 128 KiB and partition boundaries, allocate a low-address aligned DMA buffer, and use a verified in-memory cache without altering the canonical OFS parser.
- A single failed multi-sector DiskIo call permanently disables prefetch for that controller and falls back to unchanged single-sector BlockIo with the existing retry bound. Never issue repeated speculative bulk reads after firmware rejection.
- Protect with source regression checks and separate OFS EFI/OVMF CI. No claim that XFS's actual EFI driver is the same as the repository's FSW framework, nor that the physical Dell boot is resolved until tested.

## 0.5.74 — 2026-10-23

- Remove the experimentally unsuccessful Dell RTS525A PCIe ASPM mutation from the OFS filesystem driver's EFI entry point. OFS is format-only; no hardware vendor matching or PCI configuration writes occur in production OFS.
- Archive the experimental RTS525A link-control source under `firmware/uefi/storage/experimental/`, outside the production compilation path. Never install it as an unqualified storage provider.
- Keep standard BlockIo read-only access and conservative DMA allocation, fix source contracts and document the independent, optional storage-driver protocol boundary with Boot's `storage_x64/` first-pass loader.
- The physical Dell 30-second firmware reads are still unresolved; 0.5.74 is an architectural safety correction, not a claimed speed fix.

## 0.5.73 — 2026-10-23

- Add an exact-hardware UEFI PCIe link mitigation for the Dell Latitude 5550's 10ec:525a Realtek RTS525A reader (Dell 1028:0cb9) and the verified Intel 8086:7e39 upstream port.
- Locate the parent via PCI segment and secondary-bus relationship rather than assuming Linux's BDF will equal firmware's. Clear only the ASPM Link Control bits in PCI config space, verify readback, restore both partners on partial failure and otherwise leave the firmware's BlockIo/SD driver unchanged.
- Activate the volatile compatibility correction before EFI filesystem probing, with visible status. Unrelated PCI devices, BIOS settings, NVRAM, SD media and OFS filesystem contents are not modified. Preserve 0.5.67's low-DMA bounce buffer.
- Extend the EFI build and regression contract for the added helper. The Dell SD-controller 30-second synchronous firmware read fault was reproduced with 0.5.67; this workaround requires new physical verification and must not be represented as a proven cure.

## 0.5.72 — 2026-10-23

- Promote the actual original UDF implementation as managed native `udf.ko`
  after Debian symbol-resolved builds and successful Linux 6.12 QEMU
  UDF 2.01 write/remount and independent host-kernel readback (run 39932498942).
- Keep the driver independent, installing and removing through existing
  native module source-freshness, signing, selection and rollback logic.
- Remove udftools as a mandatory runtime mount-provider package, while
  leaving formatter, alternative UDF revisions and EFI/root qualification
  explicitly separate and unclaimed.


## 0.5.71 — 2026-10-23

- Promote the independent original exFAT VFS implementation to managed
  native `exfat.ko` after successful Linux 6.12 QEMU load, filesystem
  mount/write/rename/unmount/remount and independent offline `fsck.exfat`
  qualification (run 39931682513).
- Remove the exfatprogs package requirement from the mount-provider path;
  formatter and repair tool qualification remain separate work.
- Retain original Samsung-origin code and attribution, and use the common
  native module installation, source digest, signing and rollback authority.
- No exFAT EFI reader or qualified InfiltratorOS root semantics are claimed.


## 0.5.70 — 2026-10-23

- Promote the original independent NTFS3 implementation as the managed
  `ntfs3.ko` module for the single NTFS format identity, backed by the
  passed Linux 6.12 QEMU mount/write/remount and independent ntfscat
  verification (Actions run 39931921927).
- Switch the catalogue from NTFS-3G userspace runtime to the project-managed
  native VFS module; preserve original source and upstream authorship.
- Use the existing native kernel-version matching, source fingerprinting,
  signing, module selection and safe uninstall/rollback machinery.
- Keep EFI/root qualification and untested NTFS semantics explicitly open.


## 0.5.69 — 2026-10-23

- Restore 55 original CP/M implementation and build files from pinned
  lipro-cpm4l/cpmtools e534e20c15973a9559e981efb498a102020e5db7
  under editable per-format `userspace/`, retaining original code and COPYING.
- Extend the original-userspace source manifest and CI implementation
  presence guard to cover CP/M, keeping native installation disabled
  until a real kernel adapter is implemented and independently qualified.
- Source inventory now covers 37/41 formats: 10 managed native drivers,
  25 other Linux driver sources and two userspace-origin implementations.


## 0.5.68 — 2026-10-23

- Import 35 original Fosfat/Smaky implementation and build files from
  Skywalker13/Fosfat commit 289c7eb838dd5e6305c98f09aa375746a6043d86
  under the directly editable per-format userspace source directory.
- Preserve upstream licence/attribution, add CI that proves actual source
  exists, and keep userspace source distinct from qualified .ko modules.
- Inventory: 10 managed native drivers, 25 other original Linux baselines,
  one userspace original baseline, and five source imports outstanding.


## 0.5.67 — 2026-10-23

- Constrain OFS UEFI `BlockIo.ReadBlocks` transfer pages to the low 4 GiB DMA-addressable physical range using `AllocateMaxAddress`. The Dell's Realtek RTS525A is operated with a 32-bit DMA mask by Linux `rtsx_pci`, whereas the prior `AllocateAnyPages` offered no equivalent physical-address bound.
- Retain the page-backed power-of-two alignment guarantees, check the full 512-byte bounce buffer stays in range and fail closed rather than falling back above 4 GiB. Preserve the read-only OFS parser, single-sector reads, bounded retry limit and all Linux drivers.
- Add regression assertions and require real GNU-EFI/OVMF qualification. Dell Latitude 5550 OFS kernel loading remains unverified until the deployed artifact is physically retested.

## 0.5.66 — 2026-10-23

- Add standalone Linux 6.12 UDF filesystem-module runtime qualification: build original copied `udf.ko` against a kernel without built-in UDF, mount media produced by the independent `udftools` formatter, write and rename files, unmount/remount, and verify persistent contents using the separate host kernel's read-only UDF mount.
- Keep UDF in the original-source-only manifest until native runtime qualification passes; this adds no premature installable driver or filesystem capabilities.


## 0.5.65 — 2026-10-23

- Add an independent Linux 6.12 QEMU native NTFS3 qualification workflow using the original source recovered under the canonical NTFS identity, not the NTFS-3G mount provider.
- Format test media independently with `mkfs.ntfs`, load only our actual `ntfs3.ko` inside a Linux guest, exercise directory/file creation, multi-cluster write, rename, unmount/remount persistence and independent `ntfscat` verification.
- Keep NTFS out of the managed module deployment manifest until real guest runtime and data-integrity checks pass; preserve original implementation and attribution.


## 0.5.64 — 2026-10-23

- Add isolated Linux 6.12 QEMU runtime qualification for the original copied exFAT driver, using `exfatprogs` to format independent disposable media, a kernel without exFAT built into vmlinux, the actual external `exfat.ko`, directory creation, file writes, rename, unmount/remount readback and offline fsck.
- Keep exFAT outside the managed native installation manifest until this runtime workflow completes successfully. Preserve copied source and its original attribution unchanged.
- Do not conflate the earlier Debian exported-symbol build pass with genuine runtime qualification.


## 0.5.63 — 2026-10-23

- Promote original independent SCO BFS to managed `bfs.ko`, following successful Debian symbol-resolved module compilation and Linux 6.12 QEMU mount/write/unmount/remount qualification.
- Register BFS in the one native deployment manifest, enabling the existing kernel-version build, signing, module-selection, source freshness and transaction rollback mechanisms. Build `bfs.o` directly as its own out-of-tree module without rewriting the original filesystem logic.
- Retain BFS's source provenance and separate unqualified EFI/root capability. Add a native catalogue regression asserting the managed BFS identity.


## 0.5.62 — 2026-10-23

- Trial a UEFI-specific SD-controller compatibility correction: allocate the
  OFS EFI driver's 512-byte firmware transfer buffer from contiguous UEFI
  physical pages aligned to at least 4096 bytes while respecting any stricter
  Media.IoAlign up to 64 KiB.
- Keep the canonical OFS core, linked-block validation, 512-byte ReadBlocks
  request size, bounded retry policy and Linux ofs.ko unchanged.
- Extend the EFI source contract and retain full OVMF binding/read regression.
  The Dell Latitude 5550's intermittent high-LBA firmware errors still require
  real-device validation; passing CI is not physical boot qualification.

## 0.5.61 — 2026-10-23

- Promote the original independent Minix filesystem driver to the managed
  `minix.ko` install/remove lifecycle after its Linux 6.12 QEMU mount,
  write, unmount and remount runtime test and Debian symbol-resolved build.
- Allow complete native Linux drivers to retain original format semantics
  in their actual `linux/` source directory without manufacturing empty
  `core/` placeholders; retain per-format source hashing, kernel ABI
  matching, signing, module-selection checks and rollback.
- Preserve explicit separation of source availability, native Linux driver
  qualification and future EFI/root qualification.


## 0.5.60 — 2026-10-23

- Make the original Bcachefs tracepoint re-inclusion resolve through the
  actual out-of-tree Kbuild source directory instead of assuming the source
  resides at `fs/bcachefs` in the Linux kernel tree.
- Preserve the original module's code, semantics and independent identity;
  validate the change through Debian exported-symbol linker probes before
  considering native deployment.
- No new module is added to the installable native manifest without runtime
  qualification.


## 0.5.59 — 2026-10-23

- Repair two concrete original-driver out-of-tree integration failures:
  match upstream Linux 6.12.113 by removing F2FS's obsolete
  `fsverity_prepare_setattr()` call, and restore the JFS Unicode
  declaration header from the pinned upstream Linux source into the
  original JFS adapter with an explicitly local include.
- Run Debian Module.symvers-backed linking checks on all 27 imported
  Linux formats, with separate diagnostics for still-unqualified modules.
- Exercise the imported Minix module's actual VFS behaviour in a QEMU
  boot with Minix absent from the built-in kernel.
- These build and runtime qualification experiments do not silently
  add imported modules to the installable native manifest.


## 0.5.58 — 2026-10-23

- Restore the exact 38-file original Linux APFS out-of-tree module source
  (including Kbuild, DKMS, licences and upstream authorship) under the
  single canonical APFS format identity; independently compare each Git
  blob hash against its original upstream source.
- Expand the actual editable-source baseline from 34 to 35 of the 41
  catalogue formats. Six complete source imports still remain.
- Introduce Linux 6.12 compilation probes for eight original legacy
  filesystem drivers and strict exported-symbol checking against
  Debian 13's packaged kernel headers. Compilation is not claimed
  as mounted runtime or safe write-path qualification.
- Preserve explicit native install gates; no imported module is silently
  promoted to deployable kernel support.


## 0.5.57 — 2026-10-23

- Complete the editable-original-source layout: preserve the copied legacy SFS, SFS2 and PFS3 source snapshots under per-filesystem `original/` trees, retaining their exact Git objects, while keeping the qualified independent `linux/` drivers unchanged.
- Enforce the absence of all obsolete `reference/` filesystem source directories rather than only `reference/linux/`.
- Maintain the separate native driver deployment manifest and the version/tag immutability policy. No new filesystem capability is claimed.


## 0.5.56 — 2026-10-23

- Restore 1,239 original copied Linux source files for 25 in-scope
  filesystem formats from the reference quarantine to editable per-filesystem
  `linux/` working directories without altering their implementation blobs.
- Recover the original NTFS3 Linux implementation from Filesystems Git history
  as the single NTFS format's editable source baseline. Existing source
  attribution and licensing remain intact.
- Remove automatic upstream source replacement/pushing and update CI to require
  actual editable imported code, separated from the eight independently
  qualified and deployable native modules.
- Restore documentation of the original-source development model and report the
  remaining seven unimported complete source baselines honestly.
- This source-recovery release does not claim newly qualified native drivers,
  physical-media capability, Windows drivers or firmware boot behaviour.


## 0.5.55 — 2026-10-23

- Refuse package removals during support installation, native-module build-prerequisite installation and standalone package installation. APT enforces `--no-remove` against its actual transaction, so a conflict cannot silently turn an installation into removal of unrelated applications or bypass the wrapper's earlier simulation.
- Extend the private real-APT transaction regression to capture all three production installation commands. Verify that ordinary installations reach unpack while conflicting plans stop before any dpkg mutation; retain the existing exact-removal approval and package-protection cases.
- Update the maintained installation-authority and validation contracts. This correction does not complete the recorded EXT3/EXT4 implementation migration or qualify physical OFS boot, writable PFS3 or Windows runtime support.

## 0.5.54 — 2026-10-22

- Return OFS/FFS resize failures through the VFS setattr boundary instead of reporting success after restoring the previous size. Stop subsequent attribute updates on failure and preserve last-release resize status.
- Separate SFS/SFS2 allocated filesystem-block counts from Linux's 512-byte-sector accounting across growth, truncation, close and inode reload.
- Bind package-removal approval to installed package names and architectures. Enforce the exact final removal set and protection policy with an installed APT transaction guard under the frontend lock, closing the authentication/concurrent-package-change race without a separate package.
- Add resize fault injection, real-APT tests with an inert dpkg executor and mounted SFS/SFS2 accounting regressions. Update the maintained validation and privileged-operation contracts.
- EXT3/EXT4 inherited migration units remain recorded in the provenance ledger; this defect repair does not claim their implementations have been rewritten.

## 0.5.53 — 2026-10-22

- Remove speculative EFI BlockIo read-ahead: physical Dell 0.5.52 testing demonstrated DEVICE_ERROR at 8, 4, and 2 sectors, with 30-second stalls before falling back to one-sector reads. The failed optimisation also made the measured 55% kernel transfer 386 seconds slower than 0.5.51.
- Restore a single required 512-byte OFS block per firmware call, preserving canonical linked-block traversal, checksum validation, aligned DMA storage, exact failed-sector reporting and the existing one-retry bound.
- Diagnose slow actual-sector transfers: capture EFI RTC times around each BlockIo call, report calls taking at least two seconds with sector, attempt, elapsed seconds and raw status, capped at twelve reports per controller. This observes synchronous firmware latency without pretending to interrupt it.
- No changes to disk format, persistent data, boot configuration or external EFI filesystem interfaces. Physical throughput and full Linux EFI handoff remain unverified.


## 0.5.52 — 2026-10-22

- Avoid permanently disabling eight-sector EFI read-ahead after a single speculative EFI_DEVICE_ERROR. Step down the per-controller batch 8 to 4 to 2 to 1 sectors for successive transient bulk failures, retaining a useful smaller batch when supported.
- Disable batching directly on size/protocol rejections; always retry the failed speculative operation as the exact requested 512-byte sector using the unchanged bounded error path. Emit a diagnostic whenever the bulk window changes.
- Preserve read-only operation, full block validation, MediaId cache checks and no separate filesystem logic in Boot. Record that the Dell successfully read its complete 17 MB kernel in 1,091 seconds but Linux EFI-stub handoff and the 86 MB initramfs read remain unverified.


## 0.5.51 — 2026-10-22

- Opportunistically fetch up to eight adjacent 512-byte EFI BlockIo sectors during OFS file reads, caching by sector and MediaId while preserving canonical OFS checksums and linked-block traversal.
- On firmware device/size rejection of speculative I/O, disable read-ahead for the controller and use the existing precise single-sector read and bounded retry. Preserve hard failure on media changes, access errors and real data-validation faults.
- Extend EFI source-contract checks. Dell Latitude 5550 physical kernel-read throughput and Linux handoff remain unverified.


## 0.5.50 — 2026-10-22

- Record physical Dell Latitude 5550 EFI `BlockIo` `EFI_DEVICE_ERROR` failures on distinct partition-relative sectors 125092067 and 125093772; Boot 0.1.28 reached 1,310,720 bytes before its fallback transfer failed. The status is a transport error, not proof of invalid OFS blocks.
- Implement one bounded, delayed, read-only retry only for `EFI_DEVICE_ERROR` while the device remains present; maintain fail-closed behaviour for persistent errors, media loss, unsupported operations, metadata and checksums, and preserve raw sector diagnostics.
- Extend the source regression contract. Firmware qualification and end-to-end OFS boot remain pending.


## 0.5.49 — 2026-10-21

- Respond to physical Dell Latitude 5550 OFS evidence: Boot 0.1.28 read the first 524,288 bytes of vmlinuz but the following 64 KiB EFI file transfer failed with EFI_DEVICE_ERROR.
- Use aligned partition-relative BlockIo.ReadBlocks instead of unnecessary DiskIo.ReadDisk for canonical 512-byte OFS sectors, with bounded reusable alignment buffer and explicit probe/start/stop cleanup. Preserve DiskIo BY_DRIVER driver-binding ownership from prior OVMF validation.
- Print the failing firmware sector and raw BlockIo status on file-read transport errors; retain strict on-disk validation and format-versus-I/O error attribution.
- Update and run the EFI error contract and OVMF driver regression workflow. Full physical Dell reads and Linux boot remain unverified pending retest.


## 0.5.48 — 2026-10-21

- Guard OFS superblock commits against invalid root buffers following storage-device loss.
- Improve synchronous superblock error propagation and delayed-work failure reporting.
- Build-validated the Linux OFS module against kernel 7.0.0-38-generic.
- Physical unexpected-removal recovery and full CI qualification remain pending.

## 0.5.47 — 2026-10-20

- Correct EFI directory/open/file-read error attribution: firmware DiskIo read failures and media removal now propagate as I/O/media statuses rather than being unconditionally reported as OFS volume corruption. A physical out-of-range metadata pointer or failed canonical OFS validation still reports EFI_VOLUME_CORRUPTED. No implicit retries or relaxed validation.
- Preserve the original firmware read-error status per synchronous OFS protocol operation, retaining read-only access and existing separate filesystem-driver ownership.
- Real Dell Latitude 5550 OFSBOOT (249,733,120 sectors) passed a read-only root and /boot canonical reader traversal under Linux, but firmware had returned EFI_VOLUME_CORRUPTED for both directory enumeration and kernel load. This adjustment improves failure attribution; it is **not** evidence of a successful Linux kernel boot from OFS, and a firmware-specific I/O root cause has not been established.
- Expand CI to verify explicit transport/metadata error distinctions and retain independent EFI driver build and UEFI protocol tests.

### Field diagnostic utility included

- Add a native read-only media probe that exercises the *existing canonical* OFS EFI directory reader against an authentic partition or image without reformatting, repairing or changing boot state. It records directory entries and hash-cursor context on failure.
- Cover the utility with supported and unsupported OFS firmware fixture cases. This supplements but does not replace the real Dell Latitude 5550 qualification of large OFS directories, firmware I/O and Linux kernel handoff.
- Document the real `EFI_VOLUME_CORRUPTED` directory-enumeration observation without assuming the filesystem on disk is damaged or releasing an unproven driver change.


## 0.5.46 — 2026-10-20

- Correct the GNU-EFI `efi_main` C calling convention for the OFS EFI boot-service driver. OVMF reproduced a stall during its first firmware Boot Services call; only exported EFI protocol callbacks retain the Microsoft x64 ABI.
- Correct UEFI filesystem-controller ownership: observe BlockIo without claiming it, and open DiskIo BY_DRIVER. OVMF reproduced EFI_ACCESS_DENIED on the original BlockIo claim. Release DiskIo in Start failure and Stop.
- Qualify a real standalone EFI driver under QEMU/OVMF on a disposable GPT partition: load image, bind, open and read a nested OFS file, disconnect and rebind, and reject non-OFS media. Run the same cases for the standard release-style binary and the opt-in serial diagnostic build.
- Keep OFS experimental and disabled by default. Virtual execution does not qualify physical Dell firmware, removable-media latency, Infiltrator Boot UI scanning, Secure Boot or a full kernel handoff.

## 0.5.45 — 2026-10-20

- Introduce opt-in COM1 firmware-stage tracing and an isolated QEMU/OVMF Driver Binding harness. Initial virtual tests exposed entrypoint and controller-claim defects later repaired in 0.5.46.
- Preserve 0.5.45's published release identity; do not treat its experimental OFS EFI payload as physically qualified.

## 0.5.44 — 2026-10-20

- Restrict experimental OFS EFI probing to logical partitions, preventing whole-disk controller handles from being treated as partition-relative DOS/0.
- Bound OFS EFI directory lookup and enumeration to 512 hash-chain entries; reject cycles and overlong chains without changing the disk format or Linux module.
- Exercise checksum-valid cyclic chains in the reader regressions and execute them in the standalone OFS EFI workflow.
- Keep the UEFI driver experimental and disabled by default. Synchronous firmware disk I/O has no driver-enforceable wall-clock timeout; compilation and host tests do not qualify real Dell firmware.


## 0.5.43 — 2026-10-19

- Take ownership of the firmware filesystem source formerly held by Boot, preserving its implementations, dormant adapters, licences and pinned InfiltratorFS source.
- Build ext4 and OFS EFI drivers independently of Boot and include their separate component payload in the Filesystems Debian release. Keep OFS explicitly experimental.
- Install driver files under `/usr/lib/filesystems/efi/`; selection and deployment to an EFI partition remain separate installer or local-test operations.

## 0.5.42 — 2026-10-19

- Remove the local SD-card boot-payload preparation helper, its CI references
  and the machine-specific setup procedure from the OFS firmware documentation.
- Retain the OFS implementation and independent UEFI driver build unchanged.

## 0.5.39 — 2026-10-18

- Replace OFS 0.5.38's per-512-byte `sb_breadahead()` submission with bounded
  block-layer BIO reads for **contiguous** bitmap extents. A real 119.1 GiB SD
  card showed that 61,451 individual read-ahead submissions took 14.6s,
  leaving mount time unchanged at ~15s; the earlier 2.3s QEMU result was
  not representative of the physical card.
- Each batched extent contains at most 128 OFS bitmap blocks (64 KiB). The
  driver verifies every AmigaDOS bitmap checksum and counts all free bits
  before completing the mount, preserving read-only and fail-safe behavior.
  Noncontiguous/unsupported extents retain the existing buffer-head reader.
- Add per-mount observability for batch count, validated batched blocks,
  fallback blocks, block-I/O latency, read latency, validation and total time.
- Exercise the batched fast path explicitly with a sparse 120 GiB AmigaDOS
  OFS disk under pinned-kernel QEMU; retain normal Amiga RW and crash tests.
- No change to the on-disk OFS format. Physical SD benchmarks still required.

## 0.5.38 — 2026-10-18

- Improve large AmigaDOS OFS mount performance by resolving bitmap keys once
  and issuing a bounded 128-block asynchronous read-ahead window. Continue
  verifying each bitmap checksum and counting every allocation bit before
  exposing the filesystem; reject bitmap pointers outside the partition.
- Emit phase timings for mounts with at least 4096 bitmap blocks: key traversal,
  read-ahead submission, synchronous reads, checksum validation, free-bit
  counting and overall bitmap initialization. No new on-disk metadata.
- Qualify a sparse 120 GiB OFS image in QEMU with the native driver and
  read-only mount, verifying capacity and accounting before running the
  existing small-volume read/write and crash-recovery tests.
- Keep the physical SD card read-only until performance and integrity are
  independently rechecked on the Latitude.

## 0.5.37 — 2026-10-18

- Fix the native Linux kernel-module installer incorrectly hashing relative source
  file paths from the caller's working directory instead of the installed
  source directory. This caused `sha256sum: native/filesystems/...: No such
  file or directory` and prevented `ofs.ko` from building on the Latitude.
- Scope source enumeration and checksum execution to one consistent package
  working directory without changing the caller's directory.
- Add a read-only `native-module-helper source-digest <filesystem-id>`
  diagnostic and CTest regression coverage for unrelated working directories,
  source edits, ignored documentation and missing sources.
- Correct OFS Linux statfs reporting 100% used on a read-only freshly formatted
  SD card. Read-only mounts now validate and count the existing bitmap while
  preserving media bytes; read-only remounts retain the accounting and only
  read-write operations may normalise unused tail bits.
- Extend the QEMU Amiga runtime qualification to require nonzero free blocks
  from a true read-only OFS mount before any media writes. No device contents,
  boot paths or filesystem on-disk structures change.

## 0.5.36 — 2026-10-18

- Fix package configuration on systems where InfiltratorFS already owns the
  GNOME Disks diversion. Filesystems leaves the foreign launcher and diversion
  untouched while retaining its own formatter, native modules, udev rules and
  independently managed Nemo integration.
- Make shared launcher setup, per-filesystem activation and package removal
  respect independent diversion ownership. An incomplete foreign launcher
  remains a clear, non-destructive error.
- Regression-test both foreign GNOME Disks and foreign Nemo ownership,
  per-filesystem activation and safe removal without taking over their binaries.
- Filesystems-specific preload presentation in GNOME Disks is unavailable
  while another package owns that launcher; formatting and native support are
  independent of that presentation layer.

## 0.5.35 — 2026-10-18

- Remove the arbitrary 2 GiB OFS capacity limit for block-partition formatting;
  support legal 512-byte, 31-bit geometries including full 120 GiB volumes.
- Use bitsets for OFS bitmap occupancy verification and image creation, and
  qualify a sparse 120 GiB OFS image in CI.

## 0.5.34 — 2026-10-18

- Add an explicitly confirmed Linux **block-partition** OFS formatter/validator
  mode alongside the non-overwriting image creator. Whole disks, mounted/busy
  partitions, non-512-byte logical sectors and unsupported geometries fail closed.
- Extend OFS geometry test coverage to 2 GiB and add non-partition and
  incorrect-confirmation rejection tests.
- Keep direct SD-card runtime qualification, OFS EFI reading and Linux
  kernel loading separately gated; physical partition formatting remains
  an experimental, opt-in operation rather than a desktop default.

## 0.5.33 — 2026-10-18

- Add the native, image-only AmigaDOS OFS creation and verification tool. It refuses block devices, symlinks and existing files, verifies block metadata and deletes an incomplete newly created image on failure.
- Regression-test bitmap-extension geometry, corrupt media, invalid parameters and non-overwrite safety.
- In release runtime qualification, mount the image created by Filesystems' own tool and compare it byte-for-byte to the pinned independent Defragmenter formatter before exercising the native OFS Linux module.
- This preview is limited to regular test images and does not enable physical-volume formatting or claim a qualified UEFI OFS reader.

## 0.5.32 — 2026-10-18

- Record the shared filesystem-neutral Boot/Setup installation contract and its staged Amiga OFS boot-payload acceptance test.
- Clarify the difference between Boot's EFI readers and kernel-side filesystem modules, and between a readable kernel source and a qualified operating-system root.
- Link the long-term native filesystem design to cross-project integration ownership; use the current Boot product name rather than inherited rEFInd naming.
- No on-disk format, filesystem algorithm, formatter or installed driver behaviour changes.

## 0.5.30 — 2026-10-17

- Pin Libraries 1.19.46 and use canonical shared settings identities.
- Build independent test media with the canonical Defragmenter helper targets.

## 0.5.28

- Move the governing engineering philosophy into `docs/PHILOSOPHY.md` and update maintained documentation references.

## 0.5.27

- Isolate source-contract scratch files so concurrent checks cannot overwrite each other's evidence.
- Pin external CI actions to immutable reviewed commits.
- Stage and compare release drafts before publication; retain all exact-source runtime gates.
- Add the maintained validation entry point.

## 0.5.26

- Rename the application and repository to Filesystems.
- Update desktop and Windows titles, installer text, installed paths, documentation and release filenames.
- Publish one current identity throughout: filesystems for the Debian package and executable, and org.infiltrator.Filesystems for the desktop application.
- Name the planned InfiltratorOS installer Setup and the firmware boot component Boot.
