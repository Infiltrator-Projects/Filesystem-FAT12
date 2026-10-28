#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Independent util-linux mkfs.bfs image, never a physical device or mount.
set -euo pipefail
command -v mkfs.bfs >/dev/null || {
  echo 'mkfs.bfs missing; install util-linux-extra on this disposable runner' >&2
  exit 1
}
[[ "${GITHUB_ACTIONS:-}" == true &&
   "${RUNNER_ENVIRONMENT:-}" == github-hosted ]] || {
  echo 'Refusing BFS interoperability test outside hosted CI' >&2
  exit 1
}
test_binary=$(find build-desktop -type f -name 'filesystems-bfs-real-mkfs-test' -print -quit)
[[ -n "$test_binary" && -x "$test_binary" ]] || {
  echo 'Missing compiled BFS interoperability test' >&2
  exit 1
}
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
for inodes in 48 49; do
  # 48 inode entries are block aligned; 49 entries are intentionally not.
  image="$tmp/bfs-${inodes}.img"
  truncate -s 1048576 "$image"
  mkfs.bfs -N "$inodes" "$image" >/dev/null
  "$test_binary" "$image"
done
echo 'BFS-INDEPENDENT-REAL-MKFS-ALIGNED-AND-UNALIGNED-PASS'
