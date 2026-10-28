#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# BFS selector/compiler only: no APT, modprobe, DKMS, EFI, or other formats.
set -euo pipefail
export LC_ALL=C

# Locate source files relative to the installed application helper itself.
# Linux packaging has a /usr/bin launcher, but no separate .so compiler tree.
self_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
app_dir=$(cd -- "$self_dir/.." && pwd -P)
source_dir="$self_dir/../Sources/bfs"
destination=/System/Filesystems
module="$destination/BFS.so"
compiler=/usr/bin/cc
elf_checker=/usr/bin/readelf

fatal() { echo "BFS: $*" >&2; exit 1; }
[[ $# -eq 1 ]] || fatal "usage: bfs-module-helper install|remove|status"

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
        fatal "BFS.so is not a regular file"
    [[ "$(stat -c %u "$module")" == 0 ]] ||
        fatal "BFS.so is not root owned"
    mode=$(stat -c %a "$module")
    (( (8#$mode & 8#022) == 0 )) ||
        fatal "BFS.so must not be group/world writable"
}
check_module() {
    local file="$1"
    [[ -x "$elf_checker" ]] || fatal "readelf is unavailable"
    "$elf_checker" -h "$file" | grep 'Type:.*DYN' >/dev/null ||
        fatal "BFS.so is not an ELF shared object"
    "$elf_checker" -Ws "$file" |
        grep -F 'infiltrator_filesystem_module_v1' >/dev/null ||
        fatal "BFS module ABI entry point missing"
    # Check exact on-disk format before trusting a renamed .so.
    "$elf_checker" -p .ifsmeta "$file" 2>/dev/null |
        grep -F 'IFS-MODULE:bfs:1' >/dev/null ||
        fatal "BFS embedded format identity missing or incorrect"
}

# The privileged compiler must never consume untrusted application source.
# Validate each real path component; do not silently trust /Programs permissions.
safe_source_tree() {
    for dir in "$(dirname -- "$app_dir")" "$app_dir" "$self_dir" \
               "$app_dir/Sources" "$app_dir/Sources/bfs" \
               "$app_dir/Sources/bfs/module"; do
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
        for source in "$source_dir/module/bfs_module.c" \
                      "$source_dir/module/bfs_module.h"; do
            [[ -f "$source" && ! -L "$source" ]] ||
                fatal "packaged BFS source missing or symlinked: $source"
            [[ "$(stat -c %u -- "$source")" == 0 ]] ||
                fatal "BFS source must be root-owned: $source"
            mode=$(stat -c %a -- "$source")
            (( (8#$mode & 8#022) == 0 )) ||
                fatal "BFS source must not be group/world writable: $source"
        done
        [[ ! -L /System && ! -L "$destination" ]] ||
            fatal "symlinked module directory refused"
        if [[ ! -d /System ]]; then mkdir -m 0755 /System; fi
        if [[ ! -d "$destination" ]]; then mkdir -m 0755 "$destination"; fi
        safe_directory
        [[ ! -L "$module" ]] || fatal "existing BFS module is a symlink"
        temporary=$(mktemp "$destination/.BFS.so.XXXXXXXX") ||
            fatal "cannot stage BFS.so"
        trap 'rm -f -- "$temporary"' EXIT
        "$compiler" -std=c11 -O2 -fPIC -fno-stack-protector \
            -fno-builtin -ffreestanding -fvisibility=hidden \
            -nostdlib -shared -Wl,-z,defs -Wl,-z,relro -Wl,-z,now \
            -I "$source_dir/module" \
            -o "$temporary" "$source_dir/module/bfs_module.c" ||
            fatal "BFS compilation failed; old module unchanged"
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
            { printf 'BFS already absent\n'; exit 0; }
        safe_file
        rm -f -- "$module"
        printf 'Removed %s\n' "$module"
        ;;
    *)
        fatal "usage: bfs-module-helper install|remove|status"
        ;;
esac
