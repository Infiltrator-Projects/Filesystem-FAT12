#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Only run on disposable GitHub-hosted CI runners; does not touch block devices.
set -euo pipefail

if [[ "${GITHUB_ACTIONS:-}" != true ||
      "${RUNNER_ENVIRONMENT:-}" != github-hosted ]]; then
    echo 'Refusing FAT16 /System smoke test outside disposable GitHub-hosted CI.' >&2
    exit 1
fi

app_dir=/Programs/Filesystems
helper="$app_dir/Helpers/fat16-module-helper"
source_dir="$app_dir/Sources/fat16"
engine_dir="$app_dir/Sources/fat-defrag"
module=/System/Filesystems/FAT16.so
[[ ! -e "$helper" && ! -e "$module" && ! -d "$app_dir" ]] ||
  { echo "Pre-existing FAT16 test path; refusing replacement" >&2; exit 1; }

cleanup() {
    sudo "$helper" remove >/dev/null 2>&1 || :
    sudo rm -f "$helper" "$source_dir/fat16_module.c" "$source_dir/fat16_module.h" "$source_dir/classic_fat_module.c" "$source_dir/classic_fat_module.h" \
      "$source_dir/ff.c" "$source_dir/ff.h" "$source_dir/ffconf.h" "$source_dir/diskio.h" \
      "$source_dir/ffunicode.c" "$source_dir/fatfs_libc.c" "$source_dir/fatfs_module.c" "$source_dir/fatfs_module.h"
    sudo rmdir "$source_dir" >/dev/null 2>&1 || :
    sudo rm -rf "$engine_dir"
    sudo rmdir "$app_dir/Sources" "$app_dir/Helpers" "$app_dir" /Programs >/dev/null 2>&1 || :
}
trap cleanup EXIT

sudo install -d -m 0755 "$app_dir/Helpers" "$source_dir"
sudo install -d -m 0755 "$engine_dir/host" "$engine_dir/include/infiltratr"
sudo install -m 0644 native/filesystems/fat/defrag/*.c native/filesystems/fat/defrag/*.h "$engine_dir"
sudo install -m 0644 native/filesystems/fat/defrag/host/*.h "$engine_dir/host"
sudo install -m 0644 src/infiltratr-common/include/infiltratr/*.h "$engine_dir/include/infiltratr"
sudo install -m 0644 native/filesystems/fat/module/fat16_module.c "$source_dir"
sudo install -m 0644 native/filesystems/fat/module/fat16_module.h "$source_dir"
sudo install -m 0644 native/filesystems/fat/module/classic_fat_module.c "$source_dir"
sudo install -m 0644 native/filesystems/fat/module/classic_fat_module.h "$source_dir"
sudo install -m 0644 native/filesystems/fat/portable/{ff.c,ff.h,ffconf.h,diskio.h,ffunicode.c,fatfs_libc.c,fatfs_module.c,fatfs_module.h} "$source_dir"
sudo install -m 0755 packaging/linux/fat16-module-helper.sh "$helper"

sudo "$helper" install
[[ -f "$module" ]] || { echo "missing compiled FAT16.so" >&2; exit 1; }
[[ "$(sudo "$helper" status)" == installed ]] ||
  { echo "installed module not detected" >&2; exit 1; }
if readelf -d "$module" | grep 'NEEDED' >/dev/null; then
    echo "FAT16.so has host-linked library dependencies" >&2
    exit 1
fi
readelf -Ws "$module" | grep 'infiltrator_filesystem_module_v1$' >/dev/null
readelf -Ws "$module" | grep 'infiltrator_filesystem_defrag_v1$' >/dev/null
readelf -Ws "$module" | grep 'infiltrator_filesystem_module_v3$' >/dev/null
readelf -Ws "$module" | grep 'infiltrator_filesystem_module_v4$' >/dev/null

# Refuse unsafe privileged build input and retain the previously installed SO.
original_hash=$(sha256sum "$module")
sudo chmod 0666 "$source_dir/ff.c"
if sudo "$helper" install >/dev/null 2>&1; then
    echo "FAT16 accepted world-writable source" >&2
    exit 1
fi
sudo chmod 0644 "$source_dir/ff.c"
[[ "$(sha256sum "$module")" == "$original_hash" ]] ||
  { echo "FAT16 replaced the module after refusing unsafe source" >&2; exit 1; }

sudo "$helper" remove
[[ ! -e "$module" ]] || { echo "FAT16 removal failed" >&2; exit 1; }

# A binary for another FAT format is never an acceptable replacement.
foreign=$(find build-desktop -name "FAT12.so" -type f -print -quit)
[[ -n "$foreign" ]] || { echo "missing foreign module for identity test" >&2; exit 1; }
sudo install -m 0644 "$foreign" "$module"
if sudo "$helper" status >/dev/null 2>&1; then
    echo "FAT16 accepted a mismatched module identity" >&2
    exit 1
fi
sudo "$helper" remove


# The copied prebuilt module and a module compiled by Install are discovered
# through the exact same directory; neither installs a second filesystem.
prebuilt=$(find build-desktop -name FAT16.so -type f -print -quit)
[[ -n "$prebuilt" ]] || { echo "prebuilt FAT16.so missing" >&2; exit 1; }
sudo install -m 0644 "$prebuilt" "$module"
[[ "$(sudo "$helper" status)" == installed ]] ||
  { echo "precompiled FAT16.so was not recognised" >&2; exit 1; }
sudo "$helper" remove

echo 'FAT16-SELECT-COMPILE-COPY-AND-DRAG-PASS'
