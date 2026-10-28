<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Libraries usage ledger

This ledger records why public Common operations exist and where ownership belongs. Tests alone do not justify public API.

Status meanings: **ACTIVE** is consumed by production code, **FOUNDATION** is the canonical implementation beneath active API, and **READY** completes an already-active capability or directly replaces production-private mechanics when the consumer next advances its Common pin.

## Portable core

| Capability | Principal consumers / reason | Status |
| --- | --- | --- |
| Project identity, metadata and canonical build-profile labels | System Monitor, Calendar, Link, InfiltratorFS | ACTIVE |
| Bounded strings, trimming and deterministic ASCII classification/case matching/ordering | System Monitor, Calendar, Defragger, Link | ACTIVE |
| Stable non-cryptographic FNV-1a identity/signature mixing | System Monitor runtime identities/change signatures; reusable byte-stream hashing | ACTIVE |
| Monotonic unsigned-counter delta/rate mechanics | System Monitor CPU, process, storage, network and hardware accounting | ACTIVE |
| Strict signed/unsigned whole-value parsing and ranges | System Monitor, Defragger, Link | ACTIVE |
| Unsigned cursor token parsing | System Monitor procfs/sysfs parsing | ACTIVE |
| Signed cursor token parsing | Completes the active cursor-parser family | READY |
| Locale-independent finite decimal token parsing | Calculator expression-token conversion; completes the active cursor-parser family | ACTIVE |
| Locale-independent finite decimal parsing | Calendar, POSIX typed readers | ACTIVE |
| Locale-independent fixed-point ASCII formatting | System Monitor preferences/history/CSV persistence | ACTIVE |
| Strict binary quantity parsing | System Monitor CPU-cache size input | ACTIVE |
| Checked and saturating arithmetic | System Monitor, Defragger, Link, InfiltratorFS | ACTIVE |
| Checked unsigned multiplication | System Monitor sector/cache/topology conversions | ACTIVE |
| Checked unsigned subtraction | Completes the active checked-u64 family | READY |
| Checked signed add/multiply and saturating add/subtract/multiply | Completes the active signed arithmetic family; Calendar uses the shared saturating multiply contract for wide chronology/navigation intermediates | ACTIVE |
| Checked `size_t` arithmetic / array reservation | System Monitor | ACTIVE |
| Little-endian conversion | InfiltratorFS, Defragger | ACTIVE |
| Big-endian conversion and byte load/store | Replaces repeated filesystem BE16/BE32 decoding in Defragger; completes endian family | READY |
| Strict UTF-8 validation | InfiltratorFS | ACTIVE |
| Exact timing / periodic cadence | Calendar, System Monitor, Link, Backyard Racer | ACTIVE |
| System-wide temporal presentation authority, full clock/calendar catalogues, location policy and explicit 12h/24h/decimal-10 formatting | System Settings and Calendar; future Common-aware applications | ACTIVE |
| Software surfaces, alpha compositing and nearest/bilinear scaling | Backyard Racer; shared framebuffer foundation | ACTIVE |
| Generic quantity scaling | Formatting foundation | FOUNDATION |
| Shared metric/duration formatting, including canonical GHz rendering | System Monitor | ACTIVE |
| Canonical System/Day/Night persistence keys and parsing | System Monitor, InfiltratorFS | ACTIVE |
| Canonical design ABI validation | System Monitor native theme adapters | ACTIVE |
| Allocation-free i18n lookup/interpolation | Link family | ACTIVE |
| HTML text, JSON string and URI-component output encoding | ssmithnet.net, Infiltrator Repository | READY |
| Spreadsheet-safe quoted CSV field encoding | System Monitor process export | ACTIVE |

## ABI contract

Public ABI-bearing structures use `struct_size` and `abi_version`. The implementation validates the minimum prefix required by the declared ABI version rather than the newest compiler `sizeof(struct)`. This prevents harmless trailing padding or future appended fields from breaking older ABI-compatible callers while still rejecting incomplete structures.

The shared design structures follow that rule through Common-owned validators. Theme palettes additionally reject values outside 24-bit RGB, and typography validation requires every version-1 family, filename and platform-fallback identity to be present. Platform adapters may layer narrower host-range requirements on top of a valid Common record, but they must not duplicate Common's ABI-prefix authority.

## Dynamic-library adapter

`dynlib.c` owns portable module lifetime, individual symbol lookup and atomic required/optional symbol-table binding over POSIX `dlopen`/`dlsym`/`dlclose` and Win32 `LoadLibrary`/`GetProcAddress`/`FreeLibrary`. Library names and version-probing policy remain application-owned. Consumers may declare which table entries are required without reimplementing the binding loop. Windows is compiled and exercised directly in Common CI.

## POSIX provider

Common owns generic file/path/clock mechanics used by Linux applications:

- detailed bounded and dynamically allocated complete text readers plus u64/i64/double readers;
- readable-path selection;
- current-user home and XDG config/data-home resolution plus recursive directory creation;
- durable atomic replacement and durable namespace removal;
- monotonic nanosecond/second clocks;
- normalized absolute POSIX-clock deadline creation and upward-rounded remaining-millisecond conversion;
- exact EINTR-safe sequential descriptor reads/writes;
- exact EINTR-safe positioned pread/pwrite.

Native generators and publishers must use these primitives instead of carrying private whole-file writers when the Common contract fits. Product-specific transaction policy remains local.

The signed i64 reader is ACTIVE in System Monitor CPU topology/cache discovery. Filesystem status translation, block-device discovery, locking, randomness and durability policy specific to a storage engine remain application-owned.

## Endian ownership

Common owns fixed-width byte-order mechanics, not filesystem formats. The BE/LE load/store helpers intentionally operate on byte storage without alignment assumptions, allowing Defragger and filesystem code to remove private `be16`, `be32`, `le16`, and similar decoder variants while preserving filesystem-specific structural validation in those projects.

## Localisation ownership

Common owns catalogue lookup, locale normalisation, language/fallback selection and named placeholder interpolation. Product language strings, language-pack storage/discovery, UI direction policy and manufacturer-specific presentation remain in the product or Link layer.

Locale normalisation is deterministic ASCII logic. Malformed tags, duplicate separators and overlong tags fail instead of being silently shortened. POSIX `_` separators and `.encoding`/`@modifier` suffixes are accepted at the platform boundary.

## Build-package contract

CMake consumers link one authoritative target rather than enumerating Common source files:

- `InfiltratrCommon::Portable`
- `InfiltratrCommon::Common`
- `InfiltratrCommon::Shared`

The Windows build separates the static-library output from the DLL import library and exports the shared C API. Apple owns the complete portable source set through its Xcode project. CI verifies GCC, Clang, sanitizers, Windows MSVC and Apple Debug/Release.

## Consumer boundaries

- System Monitor: Common owns general C primitives, deterministic ASCII matching/ordering, stable non-cryptographic hash mixing, monotonic counter delta/rate mechanics, formatting, timing, dynamic loading, POSIX mechanics and the shared design data/ABI validation contract, including toolkit-neutral user/XDG path discovery, recursive directory creation and absolute clock-deadline conversion; process/group signature composition, hardware semantics, host appearance detection, native palette projection and product UI composition remain System Monitor-owned.
- Calendar: Common owns generic strings/parsing/arithmetic/timing, the system-wide temporal presentation policy, explicit conventional/decimal plus shared historical/astronomical clock formatting, canonical build-profile labels and dynamic-library binding; chronology, ICU version probing and calendar/event semantics remain Calendar-owned.
- Defragger: Common owns general arithmetic, byte order, exact I/O and generic durable file publication/removal; filesystem safety, on-disk validation, recovery record contents and relocation transactions remain Defragger-owned.
- InfiltratorFS: Common owns endian/UTF-8/checked arithmetic/exact POSIX I/O; allocation, CoW, checkpoints, recovery and filesystem semantics remain InfiltratorFS-owned.
- Link: Common owns portable primitives/localisation engine/timing; OBD/UDS/ISO-TP and vehicle-diagnostic policy remain Link-owned.
- Backyard Racer: Common owns software-surface mechanics, alpha composition, image scaling, exact fixed-step scheduling and generic POSIX durability/clock primitives; game simulation, vehicle data, authored assets, screen composition and presentation identity remain Backyard Racer-owned.
- MBLink/JAGLink consume Common transitively through Link where appropriate.
- ssmithnet.net: Common owns neutral design tokens, the web-token adapter, generic output escaping, generic durable publication and Pages deployment plumbing; personal content, page composition, local material/colour treatment, graphics and site information architecture remain ssmithnet.net-owned.
- Infiltrator Repository: Common owns neutral design tokens, the web-token adapter, generic output escaping, generic durable publication and Pages deployment plumbing; APT/Debian semantics, GitHub release discovery, retention, signing, mirroring, catalogue semantics and repository status presentation remain repository-owned.

## Public API permanence

Once an operation is published as part of Common's public API, it remains part
of the reference library. Loss of a current production caller is not grounds
for removal: published operations may represent historical requirements and
are maintained as stable reference implementations. Existing public operations
must therefore continue to receive correctness fixes, complete contracts and
regression coverage even when their current consumer count is zero.

This permanence rule applies to accepted public API. It does not relax the
admission standard for new operations.

## Rule for adding public API

A new public operation must satisfy at least one of these conditions:

1. two consumers already contain substantially the same capability;
2. one consumer needs a capability another already has;
3. it robustly completes an already-active shared capability family; or
4. application-private production code can be replaced immediately.

Once Common accepts an algorithmic responsibility, it must define complete input, boundary and failure semantics. Speculative utility APIs with no production justification stay out of Common.

## Shared design ownership

Common owns the canonical System/Day/Night semantic palette, structural radii/spacing metrics, MB Corpo family/role identity, validation of those shared ABI-bearing records and immutable first-party font-asset provenance. Native consumers use `infiltratr/design.h`; build systems that package the canonical verified MB faces use `cmake/InfiltratrTypographyAssets.cmake`.

The general design contract permits platform fallbacks when MB Corpo is unavailable, but a product may adopt a stricter no-fallback contract when it bundles and verifies the canonical faces itself. Product geometry, accents, icons, host-theme detection, native toolkit projection and domain-specific presentation remain local.

## Shared web and Pages ownership

The canonical neutral web adapter is `design/infiltrator-web-v1.css`. It is derived from the named roles in `design/infiltrator-design-v1.json`; consumers may layer local CSS after it, but they must not silently redefine shared palette, typography-role, radius or spacing tokens under different values.

The repository also provides `.github/actions/deploy-pages/action.yml` as the canonical product-neutral Pages publication sequence. It accepts a caller-built static directory and owns only configuration, artifact upload and deployment. Build inputs, generators, tests, scheduling, release policy and the contents of the static directory remain consumer-owned.

These non-C assets are versioned with Common but are not part of the C ABI or SONAME contract. Consumers pin an exact reviewed Common revision/release just as native consumers do.
