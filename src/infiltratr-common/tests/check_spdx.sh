#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

status=0
while IFS= read -r path; do
  case "$path" in
    *.c|*.h|*.sh|*.yml|*.yaml|*.cmake|CMakeLists.txt|Makefile)
      if ! grep -Fq 'SPDX-License-Identifier: GPL-3.0-or-later' "$path"; then
        echo "missing GPL-3 SPDX marker: $path" >&2
        status=1
      fi
      ;;
  esac
done < <(git ls-files)

exit "$status"
