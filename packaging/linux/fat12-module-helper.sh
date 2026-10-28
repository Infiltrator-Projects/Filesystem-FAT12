#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# FAT12 selector/compiler only: no APT, modprobe, DKMS, EFI, or other formats.
set -euo pipefail
export LC_ALL=C

# Locate source files relative to the installed application helper itself.
# Linux packaging has a /usr/bin launcher, but no separate .so compiler tree.
self_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
app_dir=$(cd -- "$self_dir/.." && pwd -P)
source_dir="$self_dir/../Sources/fat12"
engine_dir="$app_dir/Sources/fat-defrag"
destination=/System/Filesystems
module="$destination/FAT12.so"
compiler=/usr/bin/cc
elf_checker=/usr/bin/readelf

fatal() { echo "FAT12: $*" >&2; exit 1; }
[[ $# -eq 1 ]] || fatal "usage: fat12-module-helper install|remove|status"

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
        fatal "FAT12.so is not a regular file"
    [[ "$(stat -c %u "$module")" == 0 ]] ||
        fatal "FAT12.so is not root owned"
    mode=$(stat -c %a "$module")
    (( (8#$mode & 8#022) == 0 )) ||
        fatal "FAT12.so must not be group/world writable"
}
check_module() {
    local file="$1"
    [[ -x "$elf_checker" ]] || fatal "readelf is unavailable"
    "$elf_checker" -h "$file" | grep 'Type:.*DYN' >/dev/null ||
        fatal "FAT12.so is not an ELF shared object"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_module_v1' >/dev/null ||
        fatal "FAT12 module ABI entry point missing"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_defrag_v1' >/dev/null ||
        fatal "FAT12 defragmentation ABI missing"
    # Check exact on-disk format before trusting a renamed .so.
    "$elf_checker" -p .ifsmeta "$file" 2>/dev/null |
        grep -F 'IFS-MODULE:fat12:1' >/dev/null ||
        fatal "FAT12 embedded format identity missing or incorrect"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_module_v3' >/dev/null ||
        fatal "FAT12 writable V3 ABI missing"
    "$elf_checker" -p .ifsmeta "$file" 2>/dev/null |
        grep -F 'IFS-MODULE:fat12:3' >/dev/null ||
        fatal "FAT12 embedded writable format identity missing"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_module_v4' >/dev/null ||
        fatal "FAT12 format and metadata V4 ABI missing"
    "$elf_checker" -p .ifsmeta "$file" 2>/dev/null |
        grep -F 'IFS-MODULE:fat12:4' >/dev/null ||
        fatal "FAT12 embedded V4 format identity missing"
}

# The privileged compiler must never consume untrusted application source.
# Validate each real path component; do not silently trust /Programs permissions.
safe_source_tree() {
    for dir in "$(dirname -- "$app_dir")" "$app_dir" "$self_dir" \
               "$app_dir/Sources" "$app_dir/Sources/fat12" \
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
        # The directory may be root-owned while a source file inside it is
        # writable by another user. Never compile such a file as root.
        for name in fat12_module.c fat12_module.h \
                    classic_fat_module.c classic_fat_module.h \
                    ff.c ff.h ffconf.h diskio.h ffunicode.c \
                    fatfs_libc.c fatfs_module.c fatfs_module.h; do
            path="$source_dir/$name"
            [[ -f "$path" && ! -L "$path" ]] ||
                fatal "missing or unsafe FAT12 source: $name"
            [[ "$(stat -c %u -- "$path")" == 0 ]] ||
                fatal "FAT12 source is not root-owned: $name"
            mode=$(stat -c %a -- "$path")
            (( (8#$mode & 8#022) == 0 )) ||
                fatal "FAT12 source is writable by another user: $name"
        done
        [[ ! -L /System && ! -L "$destination" ]] ||
            fatal "symlinked module directory refused"
        if [[ ! -d /System ]]; then mkdir -m 0755 /System; fi
        if [[ ! -d "$destination" ]]; then mkdir -m 0755 "$destination"; fi
        safe_directory
        [[ ! -L "$module" ]] || fatal "existing FAT12 module is a symlink"
        temporary=$(mktemp "$destination/.FAT12.so.XXXXXXXX") ||
            fatal "cannot stage FAT12.so"
        trap 'rm -f -- "$temporary"' EXIT
        "$compiler" -std=c11 -O2 -fPIC -fno-stack-protector \
            -fno-builtin -ffreestanding -fvisibility=hidden \
            -nostdlib -shared -Wl,-z,defs -Wl,-z,relro -Wl,-z,now \
            -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DIFS_FAT_DEFRAG_BITS=12 \
            -DIFS_FATFS_ONLY=12 -I "$source_dir" -I "$engine_dir" \
            -I "$engine_dir/host" -I "$engine_dir/include" -o "$temporary" \
            "$source_dir/fat12_module.c" "$source_dir/ff.c" \
            "$source_dir/ffunicode.c" "$source_dir/fatfs_libc.c" \
            "$source_dir/fatfs_module.c" "${engine_sources[@]}" ||
            fatal "FAT12 compilation failed; old module unchanged"
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
            { printf 'FAT12 already absent\n'; exit 0; }
        safe_file
        rm -f -- "$module"
        printf 'Removed %s\n' "$module"
        ;;
    *)
        fatal "usage: fat12-module-helper install|remove|status"
        ;;
esac
