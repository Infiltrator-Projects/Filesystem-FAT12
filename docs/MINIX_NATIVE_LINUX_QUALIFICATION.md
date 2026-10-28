# Minix independent Linux driver qualification

Original implementation: `native/filesystems/minix/linux/` copied from
the Linux 6.12 filesystem driver, including its existing licensing and
copyright. No competing generic Minix implementation or fake `core/`
source has been added.

## Evidence obtained

- Linux 6.12.107 out-of-tree compilation of the original driver:
  [imported Kbuild workflow](https://github.com/Infiltrator-Projects/Filesystems/actions/runs/39917484079)
- Debian 13 actual exported-symbol table compilation and linking:
  [symbol-qualified workflow](https://github.com/Infiltrator-Projects/Filesystems/actions/runs/39918073230)
- Booted Linux 6.12.107 with in-kernel Minix disabled; loaded the separate
  `minix.ko`, formatted Minix V3 with an independent `mkfs.minix`,
  mounted and wrote data, created directories, renamed, unmounted,
  remounted and verified data and payload size:
  [QEMU runtime qualification](https://github.com/Infiltrator-Projects/Filesystems/actions/runs/39918893890)

## Deployment boundary

The Filesystems native module helper continues to own compilation for the
running kernel, signature policy, rollback, depmod, and module-selection
verification. It must fail safely when the system's built-in or loaded
`minix` cannot be replaced.

This does **not** qualify Minix EFI reading, boot-payload storage,
InfiltratorOS root semantics, true power-loss recovery, or all variants
and adversarial media behaviours. Those are separately gated.
