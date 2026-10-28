#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Compile a FAT namespace module against an explicitly selected kernel tree.
# This helper writes one output file; it does not install or load it.
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 || ( $1 != 12 && $1 != 16 ) ]]; then
    echo "Usage: fat-kernel-build 12|16 OUTPUT_DIRECTORY [KERNEL_BUILD_DIRECTORY]" >&2
    exit 2
fi
bits=$1
output=$(cd -- "$2" && pwd -P)
kernel=${3:-/lib/modules/$(uname -r)/build}
kernel=$(cd -- "$kernel" && pwd -P)
self_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
source_dir="$self_dir/../Sources/fat/kernel"
# Also support running the same helper directly from the source checkout.
if [[ ! -f "$source_dir/Makefile.variants" ]]; then
    source_dir="$self_dir/../../native/filesystems/fat/linux"
fi
test -f "$source_dir/Makefile.variants"
test -f "$kernel/Makefile"
test -s "$kernel/Module.symvers" || {
    echo "The prepared kernel build tree must include Module.symvers." >&2
    exit 1
}
target="$output/fat$bits.ko"
if [[ -e "$target" || -L "$target" ]]; then
    echo "Refusing to overwrite $target" >&2
    exit 1
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
cp -a "$source_dir/." "$work/"
cp -- "$work/Makefile.variants" "$work/Makefile"
make -C "$kernel" M="$work" modules
test -s "$work/fat$bits.ko"
install -m 0644 -- "$work/fat$bits.ko" "$target"
printf 'Built %s against %s\n' "$target" "$kernel"
