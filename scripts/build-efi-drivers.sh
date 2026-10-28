#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build independently installed filesystem drivers without a Boot checkout.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
out="$(realpath -m -- "${1:?output directory required}")"
test "$(uname -m)" = x86_64
version="$(tr -d '\r\n' < "$root/VERSION")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
source "$root/scripts/gnu-efi-toolchain.sh"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT

# Preserve the migrated implementation and compile in an isolated copy. The
# imported makefiles share per-filesystem object names and cannot safely build
# different drivers in one source directory concurrently.
cp -a "$root/firmware/uefi/." "$work/"
cat > "$work/filesystems-sbat.csv" <<EOF
sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md
filesystems,1,Infiltrator Projects,filesystems,$version,https://github.com/Infiltrator-Projects/Filesystems
EOF
make -C "$work/filesystems" ext4_gnuefi ARCH=x86_64 \
    "EFIINC=$gnu_efi_include" "EFICRT0=$(dirname "$crt")" \
    "EFILIB=$(dirname "$libefi")" "GNUEFILIB=$(dirname "$libgnuefi")" \
    REFIND_SBAT_CSV=filesystems-sbat.csv
test -s "$work/filesystems/ext4_x64.efi"
test -z "$(nm -u "$work/filesystems/ext4_x64.so")"
grep -q 'return EFI_WRITE_PROTECTED;' "$work/filesystems/fsw_efi.c"
mkdir -p "$out/drivers_x64" "$out/experimental"
install -m 0644 "$work/filesystems/ext4_x64.efi" "$out/drivers_x64/ext4_x64.efi"

# OFS keeps its canonical core and EFI adapter in native/filesystems/ofs. Its
# qualification state is independent of the established ext4 reader.
bash "$root/scripts/build-ofs-efi.sh" "$work/ofs"
install -m 0644 "$work/ofs/OFS_x64.efi" "$out/experimental/OFS_x64.efi"
python3 - "$out/drivers_x64/ext4_x64.efi" "$out/experimental/OFS_x64.efi" <<'PY'
from pathlib import Path
import struct
import sys
for name in sys.argv[1:]:
    p = Path(name)
    b = p.read_bytes()
    assert len(b) >= 0x100 and b[:2] == b'MZ', name
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    assert pe + 96 <= len(b) and b[pe:pe + 4] == b'PE\0\0', name
    assert struct.unpack_from('<H', b, pe + 4)[0] == 0x8664, name
    assert struct.unpack_from('<H', b, pe + 24)[0] == 0x20b, name
    assert struct.unpack_from('<H', b, pe + 92)[0] == 11, name
    print(f'{p}: x86-64 PE32+ EFI boot-service driver OK')
PY
