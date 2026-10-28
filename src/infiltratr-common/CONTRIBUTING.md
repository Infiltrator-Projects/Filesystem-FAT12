# Contributing

## Engineering standard

Changes to Common have project-family impact. Start from the contract, not from the implementation shortcut. Preserve product/domain ownership boundaries and do not move code into Common merely because it is reusable in one repository.

## Before changing code

1. Identify the public or internal contract being changed.
2. Check whether the behaviour is genuinely product-neutral.
3. Search for existing Common capability before adding a parallel helper.
4. Add or update regression tests for success, boundary and failure behaviour.
5. Update the relevant architecture/design/usage documentation when ownership or semantics change.

## Build and test

Use the repository's Make/CMake verification paths and ensure the complete CI surface remains green. Warnings, sanitizer failures, ABI/install regressions and consumer-test failures are defects, not release exceptions.

## Dependency policy

Prefer C/C++ and first-party implementation where that gives a clearer and more stable contract. External dependencies are acceptable when their documented interface is the stronger engineering choice, but Common must not surrender semantic ownership to a tool whose output or behaviour can change independently.

## Repository policy

main is the working branch. Published tags/releases are immutable. Release changes must correspond to the exact tested source revision.
