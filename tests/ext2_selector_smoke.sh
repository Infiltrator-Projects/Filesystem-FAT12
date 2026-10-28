#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Disposable GitHub-hosted runner only. Never touches a disk or EFI partition.
set -euo pipefail

if [[ "${GITHUB_ACTIONS:-}" != true ||
      "${RUNNER_ENVIRONMENT:-}" != github-hosted ]]; then
    echo 'Refusing EXT2 /System smoke test outside disposable CI.' >&2
    exit 1
fi

app_dir=/Programs/Filesystems
helper="$app_dir/Helpers/ext2-module-helper"
source_dir="$app_dir/Sources/ext2"
module=/System/Filesystems/EXT2.so
[[ ! -e "$helper" && ! -e "$module" && ! -d "$app_dir" ]] ||
  { echo "Pre-existing EXT2 test path; refusing replacement" >&2; exit 1; }

cleanup() {
    sudo "$helper" remove >/dev/null 2>&1 || :
    sudo rm -f "$helper"
    for part in module core; do
        sudo rm -f "$source_dir/$part/"*
        sudo rmdir "$source_dir/$part" >/dev/null 2>&1 || :
    done
    sudo rmdir "$source_dir" >/dev/null 2>&1 || :
    sudo rmdir "$app_dir/Sources" "$app_dir/Helpers" "$app_dir" /Programs >/dev/null 2>&1 || :
}
trap cleanup EXIT

sudo install -d -m 0755 "$app_dir/Helpers" "$source_dir/module" "$source_dir/core"
for src in ext2_module.c ext2_module.h; do
    sudo install -m 0644 "native/filesystems/ext2/module/$src" "$source_dir/module/$src"
done
for src in ext2_core.c ext2_core.h ext2_engine.c ext2_engine.h; do
    sudo install -m 0644 "native/filesystems/ext2/core/$src" "$source_dir/core/$src"
done
sudo install -m 0755 packaging/linux/ext2-module-helper.sh "$helper"
sudo "$helper" install
[[ -f "$module" ]] || { echo "compiled EXT2.so missing" >&2; exit 1; }
[[ "$(sudo "$helper" status)" == installed ]] || { echo "status failed" >&2; exit 1; }
if readelf -d "$module" | grep 'NEEDED' >/dev/null; then
    echo 'EXT2.so has host-linked library dependencies' >&2
    exit 1
fi
readelf -Ws "$module" | grep -F 'infiltrator_filesystem_module_v1' >/dev/null
readelf -p .ifsmeta "$module" | grep -F 'IFS-MODULE:ext2:1' >/dev/null

sudo "$helper" remove
[[ ! -e "$module" ]] || { echo "EXT2 removal failed" >&2; exit 1; }

foreign=$(find build-desktop -name OFS.so -type f -print -quit)
[[ -n "$foreign" ]] || { echo "foreign module absent" >&2; exit 1; }
sudo install -m 0644 "$foreign" "$module"
if sudo "$helper" status >/dev/null 2>&1; then
    echo 'EXT2 accepted foreign .so identity' >&2
    exit 1
fi
sudo "$helper" remove

prebuilt=$(find build-desktop -name EXT2.so -type f -print -quit)
[[ -n "$prebuilt" ]] || { echo "prebuilt EXT2.so absent" >&2; exit 1; }
sudo install -m 0644 "$prebuilt" "$module"
[[ "$(sudo "$helper" status)" == installed ]] || { echo "prebuilt status failed" >&2; exit 1; }
sudo "$helper" remove
echo 'EXT2-SELECT-COMPILE-COPY-AND-DRAG-PASS'
