# Validation

## Maintained entry points

[Architecture](ARCHITECTURE.md) owns subsystem boundaries. [Filesystem design index](FILESYSTEM_DESIGN_INDEX.md) maps each format's contract and provenance. The source, native deployment and desktop integration manifests own their separate membership sets; CI consumes those manifests.

From a recursive checkout with the required C/C++ toolchain and platform development packages:

```sh
bash tools/validate-source-contract.sh
python3 tests/repository_architecture_test.py .
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Source-contract checks use isolated temporary directories and may run concurrently. Build configuration reports platform/tool availability; compilation alone does not qualify mounted runtime behaviour.

The image-only OFS creator is exercised by `filesystems-ofs-image`
CTest and by the Amiga runtime workflow. The latter compares a newly
created project-native OFS image byte-for-byte against the pinned
Defragmenter formatter, then runs the native OFS Linux driver against
that image in QEMU. The tool refuses block devices and overwrites; its
success does **not** qualify physical-media formatting or booting
an EFI kernel from OFS.

A separate developer-preview partition mode requires an exact device-path
confirmation, Linux kernel partition identification, exclusive open,
512-byte logical blocks and a size from 1 MiB to the shared 31-bit
block-count boundary (just under 1 TiB). Unit tests verify refusal
of non-partition character devices and incorrect confirmation, while
regular-file stress tests include a sparse 120 GiB image. These guards do not
replace real-device testing: do not claim physical SD-card qualification
until controlled formatting, native driver mount, copy, remount, independent
validation and recovery have been demonstrated on expendable hardware.

## Qualification boundaries

The OFS/FFS resize fault-injection test compiles the production VFS resize
and setattr bodies with minimal host stubs. It requires exact propagation
of allocation, corruption and I/O errors and prevents later attribute changes
after a failed resize. This checks the caller contract; it does not qualify
rollback of partially modified media.

The Amiga runtime lane checks SFS/SFS2 sector accounting for a 1 MiB file
on both 512-byte and 4096-byte-block volumes after allocation, remount and
truncate. Allocator/mapping counts remain in
filesystem-block units; Linux `i_blocks` is always in 512-byte sectors.

The package transaction test uses the real installed-removal-guard target
and real APT against a private dpkg database, package lists and archives.
Its inert dpkg recorder refuses mutation. It checks that a dependency added
after approval or newly protected package cannot reach a removal command,
while an exact approved set can. It also captures installation argv from
the production C++ support-installation body, shell build-prerequisite
function and standalone wrapper's final command, stubbing only the privilege
launcher, allowlist lookup and dependency download during capture. Real APT then resolves those exact requests against
independently manufactured packages: ordinary installations reach unpack,
while conflicting packages cannot trigger any dpkg mutation. The fixture adds
no removal restriction to either captured request. No host package state is
used as a fixture, and this test does not claim full real-package installation
or kernel-module build qualification.

The main build qualifies core/catalogue/action behaviour, native install membership, desktop APT install/reinstall/purge, the Windows manager and EXT2 driver, and managed module compilation against explicit kernel APIs. Release candidates also require reusable exact-SHA sanitizer/adversarial, EXT runtime and Amiga runtime workflows.

Those checks exercise different claims. A format's presence in the catalogue or a successful kernel compilation does not make it an installation target. Format-specific design and runtime evidence define the supported operations.

## Artifact ownership

VERSION owns release identity. The desktop qualification job creates the Debian package, verifies actual installation, embeds that exact package in the native wrapper, and seals both with SHA-256. The publisher consumes that bundle after all required lanes succeed.

Publication is owned by packaging/publish-release.sh. Only drafts may be repaired. Downloaded remote assets are compared with the qualified bundle before and after promotion. Public tags and bytes remain immutable. The central repository discovers releases independently; APT scheduling does not rebuild product artifacts.

## Environment evidence

Kernel loading, real media, firmware and physical-desktop behaviour require the appropriate isolated environment. Preserve test image identity, kernel/tool versions, exact source SHA and logs. Destructive tests use disposable media; release packaging never formats a developer's storage.
