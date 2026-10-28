# Infiltrator Engineering Philosophy

This document is the canonical engineering and product-quality philosophy for maintained Infiltrator Projects software. Product repositories may add domain-specific rules, but they should not silently weaken or redefine this standard.

## The execution standard

Infiltrator work is judged against a Mercedes-Benz standard of engineering excellence: technically deep, reliable, trustworthy, comfortable to use, deliberately restrained and crafted as a coherent whole. This is a quality benchmark, not a branding claim or statement of affiliation.

The useful idea is not automotive styling. It is disciplined integration: substantial engineering hidden beneath an apparently effortless surface. The product should feel simple because the engineering is complete, not because capability, correctness or depth has been removed.

A useful shorthand is **the best or nothing**. The strongest justified solution wins whether it is new or old, simple or complex, first-party or supplied by a mature external mechanism. Modernity, novelty, abstraction, code reduction and reuse are not virtues by themselves. Refinement is.

## First-principles engineering

Start with the behaviour the product must own. Standards, research, mature implementations, competitors, platform frameworks and external tools are evidence and mechanisms to study; they are not automatic specifications and they do not become hidden owners of product meaning.

A design choice should be preferred when it is stronger in the properties that matter for that product: correctness, safety, recoverability, fidelity, responsiveness, usability, accessibility, performance, resilience, maintainability, inspectability or user confidence.

A replacement must earn its place. "Newer", "shorter", "more modern", "more abstract", "more shared" or "AI-powered" is not sufficient reasoning.

## Elegant engineering

Elegance means the smallest coherent design that completely owns the required behaviour.

Prefer:

- one authority per concern;
- explicit ownership, lifetimes and state transitions;
- narrow interfaces and mechanically defensible boundaries;
- deterministic behaviour and observable failure semantics;
- cohesive modules organised by genuine responsibility;
- shared implementations only where the shared contract is at least as strong as the best local implementation;
- progressive disclosure of complexity rather than permanent exposure of every mechanism; and
- removal of dead paths, duplicated policy, accidental compatibility layers and historical machinery that no longer serves a contract.

Do not split cohesive code merely because a file is large. Do not combine responsibilities merely to reduce file count. Line count is evidence only when it reveals conceptual spread.

## Proportional complexity

A small requirement is not allowed to create a disproportionately large increase in conceptual complexity.

When an apparently modest change requires unrelated modules to know about it, duplicates policy, introduces broad cross-layer state, expands a controller into a god object, adds repeated special cases or creates another source of truth, stop and inspect the architecture before continuing.

Difficult problems may require substantial code. The warning sign is not size by itself; it is unnecessary conceptual spread.

A change should leave the system easier to explain, or make clear why the additional complexity is irreducible and bounded.

## Ownership and dependencies

Project-owned semantics remain project-owned and testable. Dependencies are accepted when their documented contract is the stronger engineering choice; using a dependency must not surrender product policy to opaque behaviour.

Generic reusable mechanisms belong in the appropriate shared foundation only when the abstraction is genuinely broader than one product and at least as strong as the implementation it replaces. Product-specific semantics, manufacturer knowledge, filesystem policy, UI composition and other domain authority remain with the product that owns them.

Duplicate implementations should converge toward one authority, but consolidation must never weaken a stronger implementation merely to make the repository smaller.

## Failure, recovery and trust

Failure behaviour is designed at the same time as success behaviour.

Unknown, unavailable, unsupported, invalid, failed and clean states remain distinct where the distinction matters. The project prefers an explicit refusal or visible uncertainty to plausible but invented success.

Mutating, destructive, privileged or persistence-sensitive operations require preconditions, bounded authority, interruption behaviour, recovery semantics and post-verification proportionate to their risk.

A green common-case test is not proof that the feature is complete.

## Craftsmanship is functional quality

Craftsmanship is not a finishing pass. Typography, spacing, hierarchy, latency, focus behaviour, keyboard behaviour, accessibility, animation, drag/drop, error presentation, cancellation, recovery, logging, packaging, installation, upgrade behaviour and release reliability are all part of product quality.

The interface should be calm and deliberately designed, not merely technically compliant. Reduction removes noise and friction without removing justified capability. Richness may exist beneath the surface without being forced on the user.

Engineering elegance and visual elegance should reinforce each other: deep machinery, clear structure, restrained presentation and predictable behaviour.

## Code quality and comments

Code should be as self-explanatory as reasonably possible. Comments are engineering documentation, not a paraphrase of syntax.

Where explanation is warranted, the expected intellectual standard is postgraduate / university master's-level engineering. Comments should preserve the reasoning that would otherwise be lost, including relevant:

- invariants and authoritative state;
- ownership and lifetime rules;
- concurrency, locking and ordering assumptions;
- units, ranges and representation constraints;
- ABI, protocol, persistence or compatibility requirements;
- security and trust boundaries;
- crash-consistency, durability and recovery rules;
- non-obvious failure semantics; and
- reasons a tempting alternative is incorrect or deliberately rejected.

Do not narrate obvious assignments, loops or conditionals. Concise comments are preferable to verbose comments when they preserve the same engineering meaning.

## Documentation standard

Maintained technical documentation is part of the product and is held to the same postgraduate / university master's-level standard of completeness, precision and internal consistency.

Documentation should make it possible for a competent engineer to understand the current system without reconstructing its architecture from commit history. As applicable, it should describe purpose, ownership, architecture, interfaces, invariants, data and control flow, failure semantics, safety boundaries, dependency decisions, important trade-offs, build and release behaviour, qualification, portability assumptions and deliberately unsupported behaviour.

Documentation must describe the current truth. Historical material belongs in explicit history, changelog or decision records rather than being allowed to contradict maintained design documentation.

Completeness does not mean repetition. Prefer one authoritative explanation with precise references over several slightly different copies.

## Build, test and release quality

Build and release machinery is production engineering. It should be deterministic, auditable, recoverable and as small as the required guarantees permit.

Prefer one authority per build/release concern, exact dependency identities, immutable or explicitly controlled release identities and publication of the exact artifacts that passed qualification wherever practical.

Avoid hidden state, magic commit conventions, duplicated build logic, unnecessary branch choreography, mutable release recovery tricks and cross-workflow assumptions that cannot be verified.

Tests should protect behaviour, contracts, boundaries and failure semantics without needlessly coupling themselves to implementation detail. A green build is necessary; it is not sufficient evidence of product quality.

## Evidence and decision quality

A substantial design change should identify:

1. the user or engineering problem;
2. the owning layer;
3. alternatives considered;
4. evidence, standards or observations supporting the decision;
5. trade-offs and failure modes;
6. compatibility or fallback behaviour where relevant; and
7. how the change will be validated.

If the reason for a design choice cannot be explained clearly, the design is not yet finished.

## Final test

Before accepting meaningful work, ask:

- Is it correct and trustworthy, including when things go wrong?
- Is ownership clearer rather than more distributed?
- Is there one source of truth for each policy?
- Did complexity grow only where the problem genuinely requires it?
- Could the same capability be delivered with less permanent machinery?
- Is the implementation technically deep without making the user carry that complexity?
- Are code, comments and documentation precise enough for another expert to maintain confidently?
- Is the result refined rather than merely newer or larger?
- Does the product feel deliberately engineered as a whole?

If not, the work is not finished simply because it builds or appears to function.
