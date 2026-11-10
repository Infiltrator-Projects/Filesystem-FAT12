#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run before any installation: never silently reintroduce filesystem FUSE.
set -euo pipefail
cd "$(dirname "$0")/.."
for forbidden in native/host/filesystem_fuse_host.c native/host/ifs-fuse-host; do
  if [[ -e "$forbidden" ]]; then
    echo "PROHIBITED filesystem transport: $forbidden" >&2
    exit 1
  fi
done
if grep -RinE '(libfuse|fusermount|fuse3|ifs-fuse-host|/dev/fuse|CONFIG_FUSE_FS)' \
    standalone packaging scripts native .github/workflows \
    --include='*.sh' --include='*.c' --include='*.h' --include='*.py' \
    --include='*.yml' --include='*.yaml' --include='Makefile' \
    --exclude='check-package.py' 2>/dev/null; then
  echo 'PROHIBITED FUSE source, package dependency, or installer reference' >&2
  exit 1
fi
echo 'PASS: FAT12 is free of filesystem FUSE dependency and installer paths'
