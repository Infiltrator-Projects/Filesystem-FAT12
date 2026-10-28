#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify independently packaged EFI driver identity and installation boundaries."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--package', required=True, type=Path)
args = parser.parse_args()
depends = subprocess.check_output(
    ['dpkg-deb', '-f', str(args.package), 'Depends'], text=True)
assert 'infiltrator-boot' not in depends, 'filesystem drivers depend on Boot package lifecycle'
with tempfile.TemporaryDirectory() as name:
    root = Path(name)
    subprocess.run(['dpkg-deb', '-x', str(args.package), str(root)], check=True)
    expected = {
        'Programs/Filesystems/Resources/EFI/drivers_x64/ext4_x64.efi',
        'Programs/Filesystems/Resources/EFI/drivers_x64/FAT12.efi',
        'Programs/Filesystems/Resources/EFI/drivers_x64/FAT16.efi',
        'Programs/Filesystems/Resources/EFI/experimental/OFS_x64.efi',
    }
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*')
              if p.is_file() and p.suffix.lower() == '.efi'}
    assert actual == expected, f'unexpected EFI package inventory: {actual ^ expected}'
    assert not (root / 'boot').exists(), 'filesystem package writes directly to boot storage'
    for relative in sorted(expected):
        p = root / relative
        assert not p.is_symlink(), relative
        b = p.read_bytes()
        assert len(b) >= 0x100 and b[:2] == b'MZ', relative
        pe = struct.unpack_from('<I', b, 0x3c)[0]
        assert pe + 96 <= len(b) and b[pe:pe + 4] == b'PE\0\0', relative
        assert struct.unpack_from('<H', b, pe + 4)[0] == 0x8664, relative
        assert struct.unpack_from('<H', b, pe + 24)[0] == 0x20b, relative
        assert struct.unpack_from('<H', b, pe + 92)[0] == 11, relative
print('Independent EFI filesystem package: PASS')
