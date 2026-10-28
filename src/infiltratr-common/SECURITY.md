# Security

## Scope

Security issues include memory-safety defects, integer/size handling errors, unsafe path or file-publication behaviour, malformed-input handling that can cross a trust boundary, dynamic-library loading problems, and ABI mistakes that can cause unsafe memory interpretation.

## Reporting

Do not publish exploit details or sensitive reproduction material in a public issue. Use GitHub's private vulnerability-reporting/security-advisory mechanism for this repository when available.

A useful report includes the affected revision, platform/toolchain, minimal reproduction, expected versus observed behaviour and any known impact boundary.

## Response principles

Security fixes are treated as correctness fixes. The preferred response is to reproduce the problem, add a regression test where practical, fix the underlying contract and verify all supported platforms/consumers affected by the change.

## Supported source

The current released line and current main are the primary security-maintained sources. Historical development snapshots are not independently maintained once superseded unless a release explicitly states otherwise.

## Disclosure

Public disclosure should occur only after a fix or clear mitigation is available and the affected release identity is known.
