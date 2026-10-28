# Filesystem-FAT12 security and safety

Report vulnerabilities privately through GitHub Security Advisories or directly to the project maintainers; do not publish unpatched exploit details in an issue.

## Trust and installation

Only stable GitHub releases from the exact `Infiltrator-Projects/Filesystem-FAT12` repository may be selected by InfiltratorOS. Verify release asset digest, size, tag identity and final installed-module SHA-256 before reporting success. Installation of a `.run` executes native code and modifies system files; it therefore requires local administrator authorisation. No arbitrary URL or branch head is a substitute for a published release.

The module loader requires root-owned, non-group/world-writable `/System` and `/System/Filesystems`; modules must be regular non-symlink files. Never relax those checks merely to make an install pass. Select and revalidate the actual FAT width before mounting or modifying a volume.

## Destructive operations

Require explicit user selection, unmounted target, identity revalidation and exclusive access for mutation. Do not run write/recovery qualification on a system disk or irreplaceable media. Keep journal state durable and verify recovered output independently; interrupted or corrupted media is an error unless recovery validators accept it.
