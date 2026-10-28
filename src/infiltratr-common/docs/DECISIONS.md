# Decisions

This file records durable architectural decisions for Libraries.

## ADR-001 — Common owns only product-neutral contracts

**Decision.** A capability enters Common only when its semantics are genuinely reusable across products.

**Rationale.** Centralising domain policy would couple unrelated applications and weaken ownership.

**Consequence.** Filesystem semantics, calendar rules, manufacturer knowledge and product UI policy remain in their owning repositories.

## ADR-002 — Shared code must be at least as strong as the best caller

**Decision.** A consumer is not required to replace a superior local implementation with a weaker generic helper.

**Rationale.** Deduplication is useful only when it preserves or improves technical quality.

**Consequence.** When local generic code is better, Common is improved first; the duplicate is removed only after the shared contract reaches reference quality.

## ADR-003 — C and C++ are both preferred first-party languages

**Decision.** Across the project family, C and C++ are preferred over other language ecosystems for first-party native/shared implementation; Common's current portable ABI remains C.

**Rationale.** The language choice should serve correctness, control, portability and interoperability rather than ideology.

**Consequence.** Common may expose stable C contracts even when a consumer or tooling component uses C++.

## ADR-004 — Published Common APIs are durable contracts

**Decision.** Once an API is admitted and published, loss of a current caller is not sufficient reason to remove it.

**Rationale.** Shared-library consumers need stable contracts independent of the current repository graph.

**Consequence.** Incompatible semantic/layout changes require explicit ABI/version transitions.

## ADR-005 — Exact pinning is the normal consumer model

**Decision.** Consumers use exact reviewed Common releases/commits.

**Rationale.** A later Common change must not silently redefine a previously reviewed application.

**Consequence.** Consumer builds verify the intended Common identity and update deliberately.

## ADR-006 — External dependencies are mechanisms, not semantic owners

**Decision.** Common may use an operating-system API or external library when its documented contract is stronger than reimplementation, but the Common contract remains explicit.

**Rationale.** Avoiding every dependency is not the goal; owning observable semantics is.

**Consequence.** Dependencies sit behind narrow boundaries and are replaceable without changing consumer meaning where practical.
