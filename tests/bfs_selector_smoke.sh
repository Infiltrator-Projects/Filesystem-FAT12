#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Only run on disposable GitHub-hosted CI runners; does not touch block devices.
set -euo pipefail

if [[ "${GITHUB_ACTIONS:-}" != true ||
      "${RUNNER_ENVIRONMENT:-}" != github-hosted ]]; then
    echo 'Refusing BFS /System smoke test outside disposable GitHub-hosted CI.' >&2
    exit 1
fi

app_dir=/Programs/Filesystems
helper="$app_dir/Helpers/bfs-module-helper"
source_dir="$app_dir/Sources/bfs"
module_dir="$source_dir/module"
module=/System/Filesystems/BFS.so
[[ ! -e "$helper" && ! -e "$module" && ! -d "$app_dir" ]] ||
  { echo "Pre-existing BFS test path; refusing replacement" >&2; exit 1; }

cleanup() {
    sudo "$helper" remove >/dev/null 2>&1 || :
    sudo rm -f "$helper" "$module_dir/bfs_module.c" "$module_dir/bfs_module.h"
    sudo rmdir "$module_dir" "$source_dir" >/dev/null 2>&1 || :
    sudo rmdir "$app_dir/Sources" "$app_dir/Helpers" "$app_dir" /Programs >/dev/null 2>&1 || :
}
trap cleanup EXIT

sudo install -d -m 0755 "$app_dir/Helpers" "$module_dir"
for file in bfs_module.c bfs_module.h; do
    sudo install -m 0644 "native/filesystems/bfs/module/$file" "$module_dir/"
done
sudo install -m 0755 packaging/linux/bfs-module-helper.sh "$helper"

sudo "$helper" install
[[ -f "$module" ]] || { echo "missing compiled BFS.so" >&2; exit 1; }
[[ "$(sudo "$helper" status)" == installed ]] ||
  { echo "installed module not detected" >&2; exit 1; }
if readelf -d "$module" | grep 'NEEDED' >/dev/null; then
    echo "BFS.so has host-linked library dependencies" >&2
    exit 1
fi
readelf -Ws "$module" | grep 'infiltrator_filesystem_module_v1$' >/dev/null

sudo "$helper" remove
[[ ! -e "$module" ]] || { echo "BFS removal failed" >&2; exit 1; }

# A binary for another format is never an acceptable replacement.
foreign=$(find build-desktop -name "FAT16.so" -type f -print -quit)
[[ -n "$foreign" ]] || { echo "missing foreign module for identity test" >&2; exit 1; }
sudo install -m 0644 "$foreign" "$module"
if sudo "$helper" status >/dev/null 2>&1; then
    echo "BFS accepted a mismatched module identity" >&2
    exit 1
fi
sudo "$helper" remove


# The copied prebuilt module and a module compiled by Install are discovered
# through the exact same directory; neither installs a second filesystem.
prebuilt=$(find build-desktop -name BFS.so -type f -print -quit)
[[ -n "$prebuilt" ]] || { echo "prebuilt BFS.so missing" >&2; exit 1; }
sudo install -m 0644 "$prebuilt" "$module"
[[ "$(sudo "$helper" status)" == installed ]] ||
  { echo "precompiled BFS.so was not recognised" >&2; exit 1; }
sudo "$helper" remove

echo 'BFS-SELECT-COMPILE-COPY-AND-DRAG-PASS'
