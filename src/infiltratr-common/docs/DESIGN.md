# Design

## First-principles position

Common exists to reduce duplicated engineering without turning the project family into a dependency on opaque or externally controlled behaviour. The design starts with contracts: what a reusable operation means, how it fails, what it owns and what callers may rely on.

## Design goals

- one strong implementation for genuinely shared behaviour;
- deterministic, testable semantics with explicit failure handling;
- minimal and deliberate dependencies;
- C interfaces suitable for use from C, C++ and platform bridges;
- stable ownership boundaries between Common and product repositories;
- exact version pinning so consumers are insulated from later upstream change.

## Non-goals

Common is not a place for product-specific policy, manufacturer knowledge, filesystem semantics, UI composition or convenience wrappers with only one real caller. It is also not a reason to replace a superior product-local implementation with a weaker generic abstraction.

## Decision rule

"Newer" is not synonymous with "better". Existing algorithms and interfaces remain when they are the strongest justified option. A replacement must improve correctness, portability, resilience, performance, maintainability or the clarity of the contract.

External libraries and operating-system APIs are mechanisms, not sources of product meaning. They may be used behind a stable boundary where their documented contract is stronger than reimplementation. The behaviour promised to consumers remains owned and regression-tested here.

## Quality bar

A shared primitive is complete only when normal cases, boundary cases, malformed input, allocation or I/O failure where applicable, portability assumptions and consumer integration are all defined. The project prefers fail-closed behaviour to ambiguous success and treats silent truncation or undefined ownership as defects.

## Relationship to consumers

Common should make consumers smaller and more consistent without making them less capable. When a consumer has the better implementation, that is evidence for improving Common before consolidation, not evidence that the consumer should regress.
