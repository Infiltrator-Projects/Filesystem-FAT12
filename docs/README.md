# Documentation index — Filesystem-FAT12

- [One-module destination](ONE_MODULE_END_STATE.md): FAT12.so is the entire driver, to be consumed by BOOT, the kernel and applications through generic adapters; existing `.ko`/`.efi` are transitional.

- [Architecture](ARCHITECTURE.md): authority, module boundaries, consumers, dependency direction and filesystem-specific source.
- [Design](DESIGN.md): decisions and failure policy.
- [API](API.md): exported interfaces, ABI and filesystem identity.
- [Validation](VALIDATION.md): automated checks, evidence classes and release gates.
- [Packaging](PACKAGING.md): .deb/.run/GitHub source ZIP, upgrades, installer handover and installed files.
- [Forensic audit](FORENSIC-AUDIT.md): what the standalone green checks prove and what remains unqualified.
- [Decisions](DECISIONS.md): durable architecture and ownership decisions.
- [Source provenance](SOURCE.md): exact extraction source, copied shared code, attribution and external licences.
- [Security](SECURITY.md): trust, permissions, destructive-operation constraints and vulnerability handling.
- [Roadmap](ROADMAP.md): remaining independent kernel/EFI validation, media coverage and consolidation.
- [Release history](../CHANGELOG.md): versioned implementation changes and accepted qualification.
- [Migration provenance](../MIGRATION-PROVENANCE.md): source import details, retained legacy source status.

The upstream combined Filesystems documentation is retained for source traceability but is not authoritative for this standalone module's releases.
