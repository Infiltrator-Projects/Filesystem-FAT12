# Validation

## Validation philosophy

Common is infrastructure for multiple products, so validation is contract-oriented. A passing compile is necessary but insufficient: shared behaviour must be exercised directly and through representative consumers.

## Automated gates

The repository currently uses:

- .github/workflows/ci.yml
- .github/workflows/release.yml

The tests/ tree covers arithmetic, encoding, escaping, formatting, graphics, localisation, timing, token parsing, POSIX I/O/path behaviour, installed CMake consumption and ABI-oriented contracts. The design contract suite additionally verifies canonical palette/metrics/typography records, rejects NULL, wrong-version and truncated ABI records, rejects out-of-range RGB roles, requires typography identities, and accepts future trailing structure storage without making the newest compiler `sizeof()` an ABI requirement.

CI is expected to reject warnings, portability regressions, ABI/installation mistakes and behavioural changes that are not accompanied by an intentional contract update.

## Evidence levels

1. Unit/contract evidence — direct regression tests for a primitive.
2. Platform evidence — successful build and execution on the supported host/toolchain.
3. Consumer evidence — an external or in-tree consumer resolves and uses the installed interface correctly.
4. Release evidence — the exact tested commit is the one tagged and published.

A lower evidence level must not be described as proving a higher one.

## Manual validation

Manual testing remains appropriate where a result depends on a real graphical environment, platform integration or a downstream consumer that CI cannot reproduce. Such evidence supplements rather than replaces automated contract tests.

## Release criterion

A release is acceptable when the exact source revision passes the required automated gates, the published package/source identity matches that revision, and no known failing contract is represented as supported.

## Source publication

The complete Common CI run qualifies one immutable source revision. Publication requires a successful push-to-`main` run and the exact current branch head. `VERSION` identifies an unpublished release; commit-subject wording is irrelevant. The publisher creates a deterministic Git source archive and checksums, verifies the complete remote asset set and bytes before publishing its draft, then verifies the public bytes and tag again. It does not repeat compilation or testing. A rerun of the original qualified source verifies public artifacts without rewriting them.
