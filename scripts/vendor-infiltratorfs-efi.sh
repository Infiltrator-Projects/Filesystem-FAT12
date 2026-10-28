#!/usr/bin/env bash
set -euo pipefail

# Reproduce the exact portable source set used by the read-only InfiltratorFS
# UEFI integration. This script is the sole authority for constructing the
# committed vendor tree. CI invokes --check so verification never authors or
# pushes source state as a side effect.

readonly INFILFS_REPOSITORY="https://github.com/Infiltrator-Projects/InfiltratorFS.git"
readonly INFILFS_COMMIT="0451f95cadf57484640d87eea2037e067bc7ed6f"
readonly INFILFS_VERSION="0.18.94"
readonly INFILFS_FORMAT="0.18"

readonly COMMON_REPOSITORY="https://github.com/Infiltrator-Projects/Libraries.git"
readonly COMMON_COMMIT="7cc5de3de0e94ed2cfcff0840bbb5346eb5c9c9f"
readonly COMMON_VERSION="1.19.35"

readonly LZ4_REPOSITORY="https://github.com/lz4/lz4.git"
readonly LZ4_COMMIT="ebb370ca83af193212df4dcbadcc5d87bc0de2f0"
readonly LZ4_VERSION="1.10.0"

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPOSITORY_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
readonly DESTINATION="${REPOSITORY_ROOT}/firmware/uefi/filesystems/infiltratorfs/vendor"
readonly MODE="${1:-update}"

case "${MODE}" in
    update|--check)
        ;;
    *)
        printf 'usage: %s [--check]\n' "$0" >&2
        exit 2
        ;;
esac

work_root="$(mktemp -d "${TMPDIR:-/tmp}/infiltratorfs-uefi-vendor.XXXXXXXX")"
trap 'rm -rf -- "${work_root}"' EXIT HUP INT TERM

fetch_exact_commit() {
    local repository="$1"
    local commit="$2"
    local destination="$3"
    local actual

    git init -q "${destination}"
    git -C "${destination}" remote add origin "${repository}"
    git -C "${destination}" -c protocol.version=2 fetch --quiet --no-tags --depth=1 origin "${commit}"
    git -C "${destination}" checkout --quiet --detach FETCH_HEAD

    actual="$(git -C "${destination}" rev-parse HEAD)"
    if [[ "${actual}" != "${commit}" ]]; then
        printf 'requested commit %s but fetched %s from %s\n' "${commit}" "${actual}" "${repository}" >&2
        exit 1
    fi
}

infilfs_checkout="${work_root}/infiltratorfs"
common_checkout="${work_root}/common"
lz4_checkout="${work_root}/lz4"
staged="${work_root}/vendor"

fetch_exact_commit "${INFILFS_REPOSITORY}" "${INFILFS_COMMIT}" "${infilfs_checkout}"
fetch_exact_commit "${COMMON_REPOSITORY}" "${COMMON_COMMIT}" "${common_checkout}"
fetch_exact_commit "${LZ4_REPOSITORY}" "${LZ4_COMMIT}" "${lz4_checkout}"

mkdir -p \
    "${staged}/include" \
    "${staged}/src" \
    "${staged}/common/src" \
    "${staged}/lz4"

cp -a "${infilfs_checkout}/include/infilfs" "${staged}/include/"
# Normalize the shared library product label in the retained wrapper comment.
# The pinned implementation and required copyright notices are unchanged.
sed -i '/^ \* The implementation lives in /c\ * The implementation lives in Libraries 1.11+ so byte-order handling' \
    "${staged}/include/infilfs/endian.h"
cp "${infilfs_checkout}/src/checksum.c" "${staged}/src/"
cp "${infilfs_checkout}/src/status.c" "${staged}/src/"
cp "${infilfs_checkout}/src/storage.c" "${staged}/src/"
cp "${infilfs_checkout}/src/security.c" "${staged}/src/"
cp "${infilfs_checkout}/src/fs.c" "${staged}/src/"
cp "${infilfs_checkout}/src/volume.c" "${staged}/src/"
cp -a "${infilfs_checkout}/src/volume" "${staged}/src/"
cp "${infilfs_checkout}/LICENSE" "${staged}/LICENSE"

# The pinned InfiltratorFS version owns the portable Common revision used by
# this firmware adapter. Keep that dependency identity explicit here rather
# than discovering a moving branch or tag during a build.
cp -a "${common_checkout}/include/infiltratr" "${staged}/include/"
cp "${common_checkout}/src/arithmetic.c" "${staged}/common/src/"
cp "${common_checkout}/src/core.c" "${staged}/common/src/"
cp "${common_checkout}/src/ascii_internal.h" "${staged}/common/src/"
cp "${common_checkout}/LICENSE" "${staged}/common/LICENSE"

# Compressed InfiltratorFS extents must remain readable before an operating
# system exists, so the EFI integration carries a pinned freestanding LZ4
# decoder instead of acquiring a host runtime dependency.
cp "${lz4_checkout}/lib/lz4.c" "${staged}/lz4/"
cp "${lz4_checkout}/lib/lz4.h" "${staged}/lz4/"
cp "${lz4_checkout}/LICENSE" "${staged}/lz4/LICENSE"

cat > "${staged}/UPSTREAM" <<EOF
Repository: Infiltrator-Projects/InfiltratorFS
Commit: ${INFILFS_COMMIT}
Version: ${INFILFS_VERSION}
On-disk format: ${INFILFS_FORMAT}
Libraries repository: Infiltrator-Projects/Libraries
Libraries commit: ${COMMON_COMMIT}
Libraries version: ${COMMON_VERSION}
LZ4 repository: lz4/lz4
LZ4 commit: ${LZ4_COMMIT}
LZ4 version: ${LZ4_VERSION}
Purpose: read-only UEFI/FSW port for Boot
EOF

if [[ "${MODE}" == "--check" ]]; then
    if ! diff -ruN -- "${DESTINATION}" "${staged}"; then
        printf '\nVendored InfiltratorFS UEFI source does not match the pinned generator.\n' >&2
        printf 'Run ./scripts/vendor-infiltratorfs-efi.sh, inspect the diff, and commit the intentional update.\n' >&2
        exit 1
    fi

    printf 'Vendored InfiltratorFS UEFI source matches all pinned identities.\n'
    exit 0
fi

parent="$(dirname -- "${DESTINATION}")"
mkdir -p "${parent}"
rm -rf -- "${DESTINATION}"
mv -- "${staged}" "${DESTINATION}"
printf 'Updated %s from exact pinned source identities.\n' "${DESTINATION}"
