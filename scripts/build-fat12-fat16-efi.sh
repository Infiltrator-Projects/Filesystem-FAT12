#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source-owned TianoCore FAT12/16 read/write UEFI driver build.
# Builds only; never changes an ESP, NVRAM or physical partition.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
test "$(uname -m)" = x86_64
pin=447c415c2678a2bab107d5d529b879e3d7713830
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
git clone --quiet --filter=blob:none https://github.com/tianocore/edk2.git "$work/edk2"
git -C "$work/edk2" checkout --quiet "$pin"
# EDK2 provides platform headers/build libraries. The FAT ENGINE and
# FAT12/FAT16 drivers are OWNED by our checked-out Filesystems source.
git -C "$work/edk2" submodule update --init --depth 1 -- BaseTools/Source/C/BrotliCompress MdePkg/Library/MipiSysTLib/mipisyst MdePkg/Library/BaseFdtLib/libfdt
rm -rf "$work/edk2/FatPkg"
cp -a "$root/native/filesystems/fat/efi/FatPkg" "$work/edk2/FatPkg"
(
  cd "$work/edk2"
  export WORKSPACE="$PWD" PYTHON_COMMAND=python3
  make -C BaseTools -j2
  # EDK2's upstream setup script reads optional variables without guarding
  # unset status. Initialise tools path and suspend nounset only while sourcing.
  export EDK_TOOLS_PATH="$PWD/BaseTools"
  set +u
  source ./edksetup.sh --reconfig
  set -u
  build -a X64 -t GCC -b RELEASE -p FatPkg/FatPkg.dsc
)
out="${1:-$root/build-fat-efi}"
mkdir -p "$out"
for type in 12 16; do
  # EDK2 uses upper-case BASE_NAME and also emits staging duplicates.
  # Copy the one canonical top-level release image, not a wildcard match.
  image="$work/edk2/Build/Fat/RELEASE_GCC/X64/FAT$type.efi"
  test -s "$image" || { echo "Missing FAT$type UEFI driver" >&2; exit 1; }
  install -m 0644 "$image" "$out/FAT$type.efi"
  python3 - "$out/FAT$type.efi" <<'PY'
import struct, sys
from pathlib import Path
data=Path(sys.argv[1]).read_bytes()
assert data[:2]==b'MZ'
pe=struct.unpack_from("<I",data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0'
assert struct.unpack_from("<H",data,pe+4)[0]==0x8664
assert struct.unpack_from("<H",data,pe+24)[0]==0x20b
assert struct.unpack_from("<H",data,pe+24+68)[0]==11
print(sys.argv[1], "PE32+ UEFI boot-services driver")
PY
done
sha256sum "$out/FAT12.efi" "$out/FAT16.efi"
echo "EFI compile evidence only: writable UEFI runtime interoperability NOT yet certified"
