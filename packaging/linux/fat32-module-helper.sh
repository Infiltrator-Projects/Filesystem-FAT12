#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# FAT32 selector/compiler only: no APT, modprobe, DKMS, EFI, or other formats.
set -euo pipefail
export LC_ALL=C

# Locate source files relative to the installed application helper itself.
# Linux packaging has a /usr/bin launcher, but no separate .so compiler tree.
self_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
app_dir=$(cd -- "$self_dir/.." && pwd -P)
source_dir="$self_dir/../Sources/fat32"
engine_dir="$app_dir/Sources/fat-defrag"
destination=/System/Filesystems
module="$destination/FAT32.so"
compiler=/usr/bin/cc
elf_checker=/usr/bin/readelf

fatal() { echo "FAT32: $*" >&2; exit 1; }
[[ $# -eq 1 ]] || fatal "usage: fat32-module-helper install|remove|status"

safe_directory() {
    for directory in /System "$destination"; do
        [[ -d "$directory" && ! -L "$directory" ]] ||
            fatal "expected a real module directory"
        [[ "$(stat -c %u "$directory")" == 0 ]] ||
            fatal "module directory must be root owned"
        mode=$(stat -c %a "$directory")
        (( (8#$mode & 8#022) == 0 )) ||
            fatal "module directory must not be group/world writable"
    done
}
safe_file() {
    [[ -f "$module" && ! -L "$module" ]] ||
        fatal "FAT32.so is not a regular file"
    [[ "$(stat -c %u "$module")" == 0 ]] ||
        fatal "FAT32.so is not root owned"
    mode=$(stat -c %a "$module")
    (( (8#$mode & 8#022) == 0 )) ||
        fatal "FAT32.so must not be group/world writable"
}
check_module() {
    local file="$1"
    [[ -x "$elf_checker" ]] || fatal "readelf is unavailable"
    "$elf_checker" -h "$file" | grep 'Type:.*DYN' >/dev/null ||
        fatal "FAT32.so is not an ELF shared object"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_module_v1' >/dev/null ||
        fatal "FAT32 module ABI entry point missing"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_defrag_v1' >/dev/null ||
        fatal "FAT32 defragmentation ABI missing"
    # Check exact on-disk format before trusting a renamed .so.
    "$elf_checker" -p .ifsmeta "$file" 2>/dev/null |
        grep -F 'IFS-MODULE:fat32:1' >/dev/null ||
        fatal "FAT32 embedded format identity missing or incorrect"
}

# The privileged compiler must never consume untrusted application source.
# Validate each real path component; do not silently trust /Programs permissions.
safe_source_tree() {
    for dir in "$(dirname -- "$app_dir")" "$app_dir" "$self_dir" \
               "$app_dir/Sources" "$app_dir/Sources/fat32" \
               "$engine_dir" "$engine_dir/host" "$engine_dir/include" \
               "$engine_dir/include/infiltratr"; do
        [[ -d "$dir" && ! -L "$dir" ]] ||
            fatal "application resource directory is missing or a symlink: $dir"
        [[ "$(stat -c %u -- "$dir")" == 0 ]] ||
            fatal "application resources must be root-owned: $dir"
        mode=$(stat -c %a -- "$dir")
        (( (8#$mode & 8#022) == 0 )) ||
            fatal "application resources are writable by another user: $dir"
    done
}

case "$1" in
    status)
        [[ -d "$destination" && ! -L "$destination" && ! -L /System ]] ||
            exit 3
        safe_directory
        [[ -e "$module" ]] || exit 3
        safe_file
        check_module "$module"
        printf 'installed\n'
        ;;
    install)
        [[ $(id -u) -eq 0 ]] || fatal "requires authorisation"
        safe_source_tree
        [[ -x "$compiler" ]] || fatal "C compiler missing; no packages installed"
        [[ -x "$elf_checker" ]] || fatal "readelf missing; no packages installed"
        # Every compilation input is root-owned and cannot be changed by other users.
        for path in "$engine_dir"/*.c "$engine_dir"/*.h "$engine_dir/host"/*.h \
                    "$engine_dir/include/infiltratr"/*.h; do
            [[ -f "$path" && ! -L "$path" ]] || fatal "unsafe engine input: $path"
            [[ "$(stat -c %u -- "$path")" == 0 ]] || fatal "engine input is not root-owned: $path"
            mode=$(stat -c %a -- "$path")
            (( (8#$mode & 8#022) == 0 )) || fatal "engine input is writable by another user: $path"
        done
        engine_sources=("$engine_dir/writer.c" "$engine_dir/fat_analysis.c"
            "$engine_dir/fat_directory.c" "$engine_dir/fat_relayout.c"
            "$engine_dir/fat_io.c" "$engine_dir/fat_journal.c"
            "$engine_dir/fat_relocation.c" "$engine_dir/fat_volume.c"
            "$engine_dir/fat_defrag_module.c")
        for name in fat32_module.c fat32_module.h fatfs_libc.c; do
            path="$source_dir/$name"
            [[ -f "$path" && ! -L "$path" ]] || fatal "unsafe FAT32 source: $path"
            [[ "$(stat -c %u -- "$path")" == 0 ]] || fatal "FAT32 source is not root-owned: $path"
            mode=$(stat -c %a -- "$path")
            (( (8#$mode & 8#022) == 0 )) || fatal "FAT32 source is writable by another user: $path"
        done
        [[ ! -L /System && ! -L "$destination" ]] ||
            fatal "symlinked module directory refused"
        if [[ ! -d /System ]]; then mkdir -m 0755 /System; fi
        if [[ ! -d "$destination" ]]; then mkdir -m 0755 "$destination"; fi
        safe_directory
        [[ ! -L "$module" ]] || fatal "existing FAT32 module is a symlink"
        temporary=$(mktemp "$destination/.FAT32.so.XXXXXXXX") ||
            fatal "cannot stage FAT32.so"
        trap 'rm -f -- "$temporary"' EXIT
        "$compiler" -std=c11 -O2 -fPIC -fno-stack-protector \
            -fno-builtin -ffreestanding -fvisibility=hidden \
            -nostdlib -shared -Wl,-z,defs -Wl,-z,relro -Wl,-z,now \
            -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DIFS_FAT_DEFRAG_BITS=32 \
            -I "$source_dir" -I "$engine_dir" -I "$engine_dir/host" \
            -I "$engine_dir/include" -o "$temporary" \
            "$source_dir/fat32_module.c" "$source_dir/fatfs_libc.c" \
            "${engine_sources[@]}" ||
            fatal "FAT32 compilation failed; old module unchanged"
        check_module "$temporary"
        chmod 0644 "$temporary"
        chown root:root "$temporary"
        mv -fT -- "$temporary" "$module"
        trap - EXIT
        printf 'Compiled and installed %s\n' "$module"
        ;;
    remove)
        [[ $(id -u) -eq 0 ]] || fatal "requires authorisation"
        safe_directory
        [[ -e "$module" ]] ||
            { printf 'FAT32 already absent\n'; exit 0; }
        safe_file
        rm -f -- "$module"
        printf 'Removed %s\n' "$module"
        ;;
    *)
        fatal "usage: fat32-module-helper install|remove|status"
        ;;
esac
