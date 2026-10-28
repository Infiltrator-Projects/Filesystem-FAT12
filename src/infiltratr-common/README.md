<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Libraries

**Project copyright:** © 1993-2026 Shannon Smith  
**Release identity:** [`VERSION`](VERSION)  
**Language:** C11 · **Licence:** GPL-3.0-or-later

[![Common CI](https://github.com/Infiltrator-Projects/Libraries/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/Libraries/actions/workflows/ci.yml)

Common is the shared first-party foundation for portable arithmetic, parsing, encoding, formatting, graphics, localisation, time presentation, design records and native I/O mechanisms used across the Infiltrator software family.

## Ownership

Common owns product-neutral contracts that are genuinely shared and at least as strong as the implementations they replace. Calendar owns chronology; filesystem projects own filesystem meaning and mutation policy; Link owns shared automotive diagnostics; manufacturer products own vehicle knowledge. Application composition and product identity remain local.

The portable core has no operating-system or toolkit dependency. POSIX and Windows adapters isolate native mechanisms. Consumers select an exact reviewed Git revision; a normal build never advances a dependency to a mutable branch. [Usage and consumer boundaries](USAGE.md) identify the public mechanisms and their owners.

The governing [engineering philosophy](docs/PHILOSOPHY.md) requires complete behaviour, explicit ownership, deliberate failure semantics and proportionate complexity. Reuse and smaller files are useful only when they strengthen those contracts.

## Build and test

```sh
make check
make portable-check
make shared

cmake -S . -B cmake-build -DINFILTRATR_COMMON_BUILD_TESTS=ON
cmake --build cmake-build
ctest --test-dir cmake-build --output-on-failure
```

CMake exports `InfiltratrCommon::Portable`, `InfiltratrCommon::Common` and `InfiltratrCommon::Shared`. Make exposes the portable/full static libraries and the versioned shared library. Windows and Apple build the same portable source set through their native toolchains.

CI qualifies strict GCC/Clang builds, portable and full contracts, sanitizers, installed CMake consumers, shared linkage, Windows MSVC and Apple Debug/Release builds. Those gates provide distinct evidence; an unavailable runtime is not a successful test.

## Compatibility

Published APIs remain supported even when a current consumer stops using them. The shared-library major SONAME is 1. ABI-bearing records start with `struct_size` and `abi_version`; version-1 validation requires the defined ABI prefix rather than the newest compiler's complete structure size. Future trailing storage is accepted. Incompatible record semantics require a new ABI version; an incompatible shared-library contract requires a new library major.

## Releases

Development stays on `main`. `VERSION` is the release-intent authority. A successful complete push-to-main CI run can publish an unpublished version from that exact current commit; commit-message wording is not a second release switch.

The publisher archives the qualified Git tree and publishes the source ZIP with SHA-256 checksums. It does not repeat the build/test graph or rebuild binaries. Draft assets are compared byte-for-byte before publication; public assets and tags remain immutable. Re-running the original publication verifies the existing bytes. Advancing `VERSION` is required to publish a subsequent source revision.

## Technical documentation

- [Architecture](docs/ARCHITECTURE.md) — dependency direction and ownership.
- [Usage](USAGE.md) — capability contracts and consumer boundaries.
- [Design](design/README.md) — shared presentation records and adapters.
- [Validation](docs/VALIDATION.md) — qualification and evidence.
- [Contributing](CONTRIBUTING.md) and [security](SECURITY.md).
- [Changelog](CHANGELOG.md) — release history, including superseded behaviour.

## Licence

Copyright © 1993-2026 Shannon Smith. Common is licensed under GNU GPL-3.0-or-later; see [LICENSE](LICENSE).
