# Roadmap

This roadmap describes direction rather than a promise of dates. The exact released capability is defined by the tagged source and tests.

## Current foundation

- maintain the published ABI and exact consumer contract;
- keep shared primitives at least as strong as the best consumer implementation;
- qualify Linux, Windows and Apple consumers through CI;
- keep toolkit-neutral POSIX user paths and directory creation in Common rather than duplicating them in graphical consumers.

## Near-term direction

- promote reusable behaviour only after it has proved useful in more than one consumer;
- continue removing private duplicate helpers from consumers when the Common contract is stronger;
- expand regression coverage whenever a shared primitive gains a new caller.

## Longer-term direction

- add new shared capability families only where a stable product-neutral contract exists;
- preserve backward compatibility or introduce explicit versioned ABI transitions when incompatibility is unavoidable.

## Admission rule

New work enters Common only when it has a stable product-neutral contract and centralising it is at least as correct and maintainable as the best existing implementation. Project-specific behaviour stays with its owner.

## Completion rule

A roadmap item is complete only when implementation, automated tests, documentation and consumer integration agree. Publication of a Common release is not used to conceal unqualified behaviour.
