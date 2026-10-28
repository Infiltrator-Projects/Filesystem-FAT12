# Changelog

This file records user-visible and contract-relevant changes to Common. Detailed commit history remains in Git; this document is for changes that matter to consumers.

## 1.19.46 - 2026-10-17

- Use the RunnerScope identity in Windows workflow process checks.

## 1.19.45 - 2026-10-17

- Use the canonical System Settings provider identity and require canonical repository links.

## 1.19.43 — 2026-10-16

- Make `VERSION` the release-intent authority and retire the parallel commit-subject switch.
- Archive the exact qualified source without repeating the complete CI build/test graph in the publisher.
- Verify draft and public asset bytes and exact tag identity; retain immutable publication and repeatable recovery.
- Keep current capability/ownership/build guidance in the README and release history in this changelog.

## 1.19.42 — 2026-10-14

- Add Common-owned validators for the versioned theme palette, design metrics and typography records so native consumers no longer duplicate Common's ABI-prefix rules.
- Reject wrong-version and truncated design records, reject palette roles outside 24-bit RGB, require the version-1 typography identities, and retain forward-compatible acceptance of trailing structure storage.
- Correct the ownership ledger to reflect System Monitor's active binary-quantity parser, signed POSIX i64 reader and checked unsigned multiplication use, and record its GTK/Win32 design consumption accurately.
- Restore repository-wide SPDX coverage to the philosophy-adoption workflow and qualify the complete change through the normal GCC, Clang, sanitiser, Windows, Apple and design-contract gates.

## 1.19.41 — 2026-10-10

- Render both Edo clock models with traditional Japanese bell numerals and localized Japanese branch names/wording, removing English animal teaching labels from timestamps. Native compatibility output uses forms such as `巳の刻 · 四つ`; English output uses `Mi · 四 bells`.
- Make explicit 12-hour clocks use locale-specific AM/PM markers and ordering, including Chinese, Japanese, Korean and Spanish forms. Preserve conventional English output and the locale-free compatibility API.
- Cover midnight/noon, negative instants, UTC offsets, vertical layout, bounded output and repeated daytime/nighttime Edo bell counts without changing the underlying time calculations.
- Run the complete MSVC Windows test gate on a clean hosted Windows runner after the self-hosted runner rejected every contract executable. Select the runner's native target architecture; no test or release gate is skipped.

## 1.19.40 — 2026-10-10

- Add an explicit-locale clock formatter using Common's existing allocation-free i18n catalogues. Chinese double-hour/hundred-kè and Roman clock wording follows the selected UI language while native units, Roman numerals, astronomy and phase boundaries remain unchanged.
- Keep native-notation compatibility rendering available; no process locale, temporal policy format or timestamp is changed.
- Cover English/Chinese script selection, other translated wording, locale-tag variants, fallback, vertical layout, bounded output and the exact Chinese midnight half-shí transition.

## 1.19.39 — 2026-10-10

- Render Traditional Chinese double-hours as document-style Earthly-Branch time (`子時`) and use the attested `初`/`正` half-shí distinction when finer precision is requested, instead of embedding pinyin and English zodiac teaching labels in the timestamp.
- Give Roman seasonal daylight time a native finer presentation using twelfths of the current unequal hora (`unciae`) while retaining the profile's four-watch night convention.
- Advertise native finer-precision support for the Chinese double-hour and Roman seasonal profiles so system-wide presentation controls match formatter behaviour.
- Add a bounded, toolkit-neutral date-renderer ABI and a lazy POSIX bridge to Calendar's native chronology engine. Common owns shared provider discovery and UTF-8 validation; Calendar retains date conversion and calendar-specific presentation.
- Keep caller output unchanged on missing providers, unsupported dates, invalid UTF-8 and insufficient capacity; qualify the bridge with both Make and CMake contract tests.

## 1.19.38 — 2026-09-26

- Close the final forensic contract gaps found after the 1.19.37 hardening pass.
- Preserve nearest-neighbour edge pixels during exact right-angle rotation by accepting each source pixel's half-pixel sampling footprint.
- Enforce physical latitude/longitude ranges consistently for every location-aware clock and elapsed-duration mode, and make duration formatting use the same catalogue-driven location contract as clock formatting.
- Make Apple portable-source completeness CI derive its required module list from CMake's canonical portable source inventory so future additions cannot silently drift out of the Xcode target.
- Add temporal regressions for missing and out-of-range location data while retaining the existing non-square rotation regression.

## 1.19.37 — 2026-09-26

- Restore Apple native package parity with the canonical 13-source portable Common target and extend CI so the complete source set is enforced.
- Correct rotated software-surface clipping for non-square and oblique transforms and reject non-finite rotation angles before coordinate conversion.
- Validate localisation catalogue structure at initialisation, including locale syntax, entry storage and non-NULL keys/values, while leaving failed contexts unusable.
- Make recursive directory creation descriptor-relative while preserving existing symlink-to-directory behaviour and permission-mode semantics.
- Define previously implicit non-overlap contracts for escaping and path concatenation helpers.
- Align consumer documentation with Common-owned historical/astronomical clock formatting and the current Calendar product name.

## 1.19.36 — 2026-09-26

- Complete a second forensic clarity pass over every shared clock/calendar selector name, with special attention to ancient and reconstructed systems.
- Make Roman seasonal time say explicitly that its twelve daylight hours are unequal and its four night divisions are military watches.
- Make ancient Babylonian time expose the sunset-start civil day and fixed 12-bēru/30-UŠ hierarchy, while naming the later sunrise-origin convention as Renaissance European “Babylonian” hours.
- Replace Nuremberg `Wendetage` jargon in the selector with the behaviour it means and label the coordinate-driven variant as a reconstruction rather than the historical civic schedule.
- Expand model-sensitive calendar labels: Ptolemaic Nabonassar-era Egyptian civil, traditional Armenian wandering calendar, Goodman–Martínez–Thompson 584283 Maya correlation, French Republican 1792 epoch/Romme continuation, and Positivist structural rules.
- Label local sidereal time as Common's compact J2000 approximation so the UI does not imply ephemeris-grade accuracy.
- Preserve all clock/calendar algorithms and persisted IDs; this release changes explanatory presentation and regression expectations only.

## 1.19.35 — 2026-09-26

- Normalize Chinese double-hour elapsed presentation to native `日 · 時辰` instead of denominator notation.
- Normalize complete Edo unequal-hour cycles as `日` plus residual `刻`/half-`刻` rather than unbounded toki totals.
- Normalize ancient Babylonian elapsed presentation as `ūmu · bēru · UŠ`, with twelve bēru per ūmu/day and thirty UŠ per bēru.
- Clarify that the default traditional Chinese double-hour mode uses the common Zǐ-at-23:00 convention and that finer hundred-kè subdivisions varied historically.
- Make Roman and Nuremberg selector wording expose their native day/night structure directly.
- Clarify broad calendar labels where the implementation is a specific era/epoch model: Buddhist Era, Coptic, Ethiopian Amete Mihret/Alem, Minguo, Varronian A.U.C. Roman dating, Constantinopolitan Byzantine era, Nabonassar Egyptian epoch, traditional Armenian epoch, International Fixed and World Calendar structure.

## 1.19.34 — 2026-09-26

- Split Edo Japanese unequal time into the existing late-Edo 1797 twilight model and a distinct early-Edo sunrise/sunset model; both retain six daylight and six night periods and the historical zodiacal labels.
- State explicitly that apparent solar time uses Common's compact Sun-position approximation rather than presenting the calculation as exact ephemeris output.
- Clarify the Chinese hundred-kè historical variant, ancient Babylonian equal bēru/UŠ hierarchy and Indian ghaṭī unit size in the selector.
- Clarify calendar model/range assumptions for Hebrew arithmetic, Umm al-Qura table coverage, Persian arithmetic, modern Badíʿ Naw-Rúz, computational Chinese/Dangi calendars and the modern Meiji–Reiwa Japanese-era implementation.

## 1.19.33 — 2026-09-26

- Audit every clock/calendar catalogue label for comprehensibility and expose important model assumptions directly in the selector instead of relying on specialist knowledge.
- Clarify Roman seasonal, Edo unequal-hour, historical Italian, Renaissance European “Babylonian”, ancient Babylonian, Chinese, Nuremberg, solar/sidereal/scientific and Unix clock labels without changing stable persisted IDs.
- Clarify model-sensitive historical calendars including Roman civil dating, Byzantine Anno Mundi, Egyptian and Armenian wandering years, Mayan GMT correlation, French Republican continuation, Swedish reform chronology and Positivist epoch.
- Replace cryptic Indian `GH hh:mm` presentation with native `ghaṭī · pala` units; elapsed durations normalize complete 60-ghaṭī days as `dina`.

## 1.19.32 — 2026-09-26

- Remove redundant `italian-modern`: modern Italian civil time is the existing standard 24-hour civil clock, not a separate time system.
- Keep `italian-hours` exclusively for the historically distinct sunset-origin Italian-hours system.
- Add a regression check preventing the duplicate modern-Italian mode from re-entering the catalogue.

## 1.19.31 — 2026-09-26

- Add `italian-modern` so modern Italian 24-hour civil presentation is explicitly separate from historical sunset-origin Italian hours.
- Replace the provisional ancient-Babylonian seasonal presentation with the better-attested fixed system: twelve equal `bēru` per sunset-to-sunset civil day, each subdivided into 30 UŠ.
- Make `nuremberg-hours` the historical Nürnberg Great Clock: equal 60-minute hours, separate day/night counts and the fixed 1488 Wendetag schedule (mapped from its Julian dates to the corresponding proleptic-Gregorian seasonal dates).
- Preserve the previous location-aware reconstructed Wendetag behaviour under the new `nuremberg-solar` mode instead of conflating it with Nürnberg's documented civic schedule.
- Keep historical persisted IDs stable where their meaning is unchanged and add explicit catalogue regression coverage for the new distinctions.

## 1.19.30 — 2026-09-26

- Rename the presentation labels for historical Italian hours and Renaissance European "Babylonian hours" so neither can be mistaken for modern Italian civil time or ancient Babylonian timekeeping; keep their stable persisted IDs unchanged.
- Add `babylonian-ancient`, a distinct ancient seasonal-hour mode dividing daylight and night into twelve simānu each from astronomical sunrise/sunset.
- Correct `nuremberg-hours`: retain equal 60-minute hours but assign a discrete integer number to daylight (8–16) and the remainder to night, changing allocation only when local solar day length crosses the next half-hour threshold—the Wendetag principle—instead of resetting continuously at each real sunrise and sunset.
- Preserve location-aware behaviour for the Nuremberg method because neighbouring cities historically used slightly different Wendetage; the configured location determines the reconstructed transition days.
- Document that modern Italian time is ordinary civil time under the Europe/Rome time zone, not a distinct clock system.
- Add regression coverage for the new catalogue names, ancient Babylonian seasonal formatting and corrected Nuremberg clock path.

## 1.19.29 — 2026-09-26

- Normalize Roman seasonal durations into complete `dies` plus residual horae/vigiliae/unciae instead of presenting unbounded aggregate daylight and night totals.
- Present Chinese hundred-kè durations with native 日/刻 hierarchy; remove hybrid modern day prefixes and redundant `/100` denominator notation.
- Keep generic Chinese hundred-kè precision honest: finer fēn/miǎo subdivisions varied by historical calendar, so this mode does not fabricate one dynasty's sub-kè convention.
- Add regression coverage proving a full Roman civil cycle renders as `1 dies` and one hundredth of a Chinese day renders as `1刻`.

## 1.19.28 — 2026-09-26

- Strengthen the existing elapsed-duration contract for Roman temporal and Edo Japanese seasonal modes instead of leaving their anchored intervals in disguised modern H:M:S.
- Integrate seasonal duration directly between the real civil interval endpoints: Roman daylight accumulates twelve variable horae, Roman night accumulates four variable vigiliae, and Edo day/night each accumulate six variable koku/toki.
- Express Roman fractional seasonal units as unciae (twelfths) and Edo fractional precision with the historically attested half-period marker rather than fabricating modern minutes or seconds.
- Keep unanchored accumulated quantities explicitly labelled SI because no honest seasonal conversion exists without a civil interval.
- Preserve canonical Unix/SI storage, monotonic accounting and all existing public API signatures; this is a presentation-contract correction.
- Add regression coverage for anchored Roman/Edo output, explicit-SI unanchored fallback and full-day seasonal intervals.

## 1.19.27 — 2026-09-25

- Add a strict allocation-free Unicode scalar-to-UTF-8 encoder beside the existing validator.
- Reject UTF-16 surrogate code points and values above U+10FFFF while preserving exact one-through-four-byte canonical UTF-8 output.
- Keep filesystem, JSON and application-specific malformed-input/replacement policy in consumers; Common owns only the product-neutral scalar encoding step.
- Add contract coverage for ASCII, three-byte and maximum four-byte scalars plus invalid-scalar and insufficient-capacity unchanged-output failures.

## 1.19.26 — 2026-09-24

- Add one canonical elapsed-duration formatter keyed by the same system-wide clock-mode IDs used for civil-clock presentation.
- Give fixed-unit systems their actual elapsed representation: French decimal 10/100/100 time, Internet beats, binary H:M:S, hexadecimal 65,536-tick days, Julian/MJD fractional days, sidereal rate, Chinese double-hours/hundred-kè and Indian ghaṭī/vighaṭī.
- Anchor apparent-solar elapsed presentation to the interval end instant so equation-of-time drift is represented instead of silently treating apparent solar time as a fixed-rate clock.
- Keep equal-hour origin systems on their real 60/60 units, and deliberately retain conventional SI H:M:S for Roman and Edo seasonal period labels because their unequal day/night periods do not define one context-free duration unit.
- Preserve canonical SI/monotonic accounting; this API changes presentation only.

## 1.19.25 — 2026-09-24

- Add one portable formatter for every explicit clock mode in the shared temporal catalogue, including decimal, Internet, binary/hexadecimal, astronomical, solar-origin, Roman/Japanese seasonal and other historical systems.
- Keep the `standard` OS-locale mode deliberately platform-owned while making explicit 12/24-hour and extended modes deterministic across consumers.
- Require configured geographic context for location-dependent modes so applications cannot silently substitute Greenwich or another fabricated location.
- Add catalogue-wide regression coverage so System Settings cannot publish a selectable explicit mode that Common cannot render.

## 1.19.24 — 2026-09-22

- Harden the existing graphics rectangle/copy/blit paths so signed caller coordinates are clipped before endpoint arithmetic, eliminating overflow-prone `origin + length` calculations at extreme inputs.
- Make self-copy an explicit no-op success and snapshot aliased sources for in-place region copy, blitting, scaling and rotation so overlapping writes cannot destroy pixels that are still required as input.
- Reuse Common's existing exact cycle-partition arithmetic for nearest-neighbour scale mapping instead of forming a potentially overflowing `index * source_span` product.
- Add regression coverage for `INT_MIN`/`INT_MAX` clipping and overlapping in-place surface operations.
- No public functions, types or APIs were added or removed.

## 1.19.23 — 2026-09-22

- Expose Common's existing exact local-civil microsecond-of-day calculation as a public temporal primitive so Calendar and future consumers do not maintain a duplicate negative-safe day-phase algorithm.
- Preserve the formatter's existing behaviour by routing it through the promoted helper and add boundary coverage for negative instants, positive/negative UTC offsets and signed integer extremes.

## 1.19.22 — 2026-09-22

- Harden the existing bounded POSIX text reader to reject embedded NUL bytes in its retained byte range and trim trailing CR/LF directly from the known byte count instead of rediscovering length with `strlen()`.
- Reuse that bounded-reader contract for temporal provider and policy documents, removing duplicated open/read/probe/close loops while retaining the 1024-byte schema limit and existing rich I/O results.
- Anchor atomic file replacement and durable unlink to one opened parent-directory descriptor for target inspection, namespace mutation and final directory `fsync()`, closing the pathname re-resolution identity gap.
- Create atomic-writer temporary descriptors with `O_CLOEXEC` atomically on Linux via `mkostemp()`, retain the portable `mkstemp()+fcntl()` fallback elsewhere, and verify the temporary entry belongs to the anchored parent before publication.
- Add bounded embedded-NUL and symlink-target replacement regressions; the symlink test proves atomic replacement changes the link entry without modifying its victim.
- Leave `infiltratr_mkdir_parents()` unchanged; descriptor-relative traversal remains deferred until its existing generic pathname, symlink and permission semantics can be preserved exactly.
- No public functions, types or APIs were added or removed.

## 1.19.21 — 2026-09-22

- Harden the existing temporal-policy v3 parser so current authority documents must contain one unambiguous value for every existing v3 field; duplicate or incomplete v3 documents are rejected while legacy v1/v2 migration remains unchanged.
- Validate the existing fixed-size clock/calendar ID fields are NUL-terminated before serialisation performs catalogue lookups.
- Bound the existing POSIX temporal policy and provider-marker reads to the schema's 1024-byte document capacity while preserving embedded-NUL rejection and rich I/O outcomes.
- Accelerate the existing exact cycle-partition implementation with a checked direct-multiply path when the product fits in uint64_t, retaining the overflow-safe wide fallback unchanged.
- Strengthen tests for existing clock/calendar catalogue count/at/find invariants, bounded temporal files, embedded NULs and both cycle-partition arithmetic paths.
- No public functions, types or APIs were added or removed.

## 1.19.20 — 2026-09-21

- Validate the temporal provider marker's provider identity, policy version and contract instead of treating any readable file at the marker path as authority.
- Permit an explicit absolute provider-marker path for staged/custom-prefix installs and deterministic qualification.
- Reject non-finite and out-of-range latitude/longitude before serialising temporal policy, keeping save/load validation symmetric.
- Add allocation-backed XDG configuration-path resolution and remove the temporal persistence layer's private 4096-byte path ceiling.
- Extend POSIX and temporal contract tests for long XDG paths, provider marker contents and coordinate bounds.

## 1.19.19 — 2026-09-21

- Added the shared POSIX temporal-policy persistence adapter so Linux consumers no longer duplicate the XDG `presentation.conf` path, parsing, validation, directory creation or atomic write mechanics.
- Added the canonical installed temporal-provider capability marker `/usr/share/infiltrator/policy-providers/temporal-v3`, allowing consumers to distinguish an active System Settings authority from stale per-user policy files without probing executable names or PATH.
- Added contract tests for missing-policy defaults, canonical paths, atomic save/load round trips and provider-marker identity.

## 1.19.18 — 2026-09-20

- Removed the retired public temporal-policy v2 ABI instead of carrying compatibility baggage.
- Made temporal policy v3 the sole current cross-application authority.
- Removed the secondary-calendar entry and the non-calendar `none` choice from the shared calendar catalogue.
- Retained only a private one-time parser for already-written v2 `presentation.conf` data; the old primary calendar migrates to the single v3 calendar and the retired secondary value is discarded.
- Added regression coverage proving v3 serialisation cannot emit secondary-calendar state and the shared calendar catalogue contains only the 30 real calendar systems.

## 1.19.17 — 2026-09-20

- Added a generic monotonic unsigned-counter delta contract with unchanged-output rollback semantics and made the existing counter-rate operation consume it.
- Added errno-style POSIX absolute-deadline construction for caller-selected clocks and upward-rounded remaining-millisecond conversion.
- Kept pthread wait policy, hardware-counter interpretation and product-specific baseline/reset semantics in consumers while removing their repeated generic arithmetic.

## 1.19.16 — 2026-09-20

- Expanded temporal policy to a version-2 system authority containing all 21 Calendar clock systems.
- Added the complete shared calendar-system catalogue: 30 primary calendars plus None for the optional secondary date.
- Added primary calendar, secondary calendar, seconds and optional geographic latitude/longitude to the persisted temporal policy.
- Removed the conceptual global "Follow system" choice: the authority uses Standard time (OS locale), while applications may separately offer Follow System Settings.
- Added automatic in-memory migration of version-1 system/12h/24h/decimal policy documents to v2.

## 1.19.15 — 2026-09-20

- Completed the deterministic ASCII family with locale-independent case-insensitive lexical ordering, allowing consumers to remove residual `strcasecmp` sorting.
- Added stable non-cryptographic 64-bit FNV-1a byte, text and explicit little-endian uint64 mixing for consumer runtime identities/change signatures.
- Kept signature composition and domain identity policy in consumers; Common owns only the portable hash mechanics.

## 1.19.14 — 2026-09-20

- Added the portable system-wide temporal presentation policy contract with stable System, 12-hour, 24-hour and decimal-10 clock profiles.
- Added deterministic versioned policy parsing/serialization so System Settings and consumers can exchange one user-wide temporal preference without changing canonical data.
- Promoted Calendar's exact rational decimal-day partition semantics into Common and added boundary/extreme-instant regression coverage.

## 1.19.13 — 2026-09-20

- Added toolkit-neutral current-user HOME, XDG_CONFIG_HOME and XDG_DATA_HOME resolution to the POSIX provider, with specification-correct fallback for relative XDG overrides.
- Added recursive POSIX directory creation with errno-style failure reporting, replacing graphical-toolkit filesystem helpers in consumers without moving application policy into Common.
- Completed the public deterministic ASCII family with whitespace/alpha/digit/alnum/hex classification and case-insensitive substring matching, replacing locale/toolkit helpers in kernel, protocol and identifier grammars.
- Extended POSIX and core contract tests to cover environment overrides, XDG fallback behaviour, truncation, recursive directory creation and the complete ASCII contracts.

## 1.19.12 — 2026-09-20

- Promoted deterministic ASCII byte case conversion, complete case-insensitive equality and case-insensitive prefix matching into the portable public contract for protocol and hardware identifiers.
- Added a bounded POSIX lexical dirname contract alongside basename, allowing consumers to remove toolkit allocation from simple parent-directory extraction.

## 1.19.11 — 2026-09-20

- Added canonical lowercase System/Day/Night persistence keys and case-insensitive parsing to the shared design contract, replacing duplicated consumer serialization logic.
- Added the shared two-decimal GHz formatter used by System Monitor, completing the active frequency-formatting family without a consumer-local scalar wrapper.
- Standardised the documentation baseline across the Infiltrator project family.

## 1.19.7 — 2026-09-19

- Added exact locale-independent finite-decimal cursor-token parsing with unchanged-output failure semantics.
- Exposed canonical native structural design metrics and typography identity through the C adapter.
- Centralised immutable MB Corpo font-asset provenance in reusable CMake metadata without moving font binaries into Common.

## Release history

The repository already records release-specific detail in README.md, release tags and GitHub Releases. Those immutable release records remain authoritative for exact historical source identity.

## Changelog policy

Entries should describe behaviour, not internal activity. Refactoring with no observable contract change needs an entry only when it materially changes maintenance, portability or dependency requirements.
