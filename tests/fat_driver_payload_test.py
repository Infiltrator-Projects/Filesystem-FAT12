#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the release package contains the exact qualified FAT driver bytes."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--package', required=True, type=Path)
parser.add_argument('--efi-dir', type=Path)
parser.add_argument('--ko-dir', type=Path)
args = parser.parse_args()

with tempfile.TemporaryDirectory() as name:
    root = Path(name)
    subprocess.run(['dpkg-deb', '-x', str(args.package), str(root)], check=True)
    app = root / 'Programs/Filesystems'
    source = app / 'Sources/fat/kernel'
    for filename in ('Makefile.variants', 'fat.h', 'fat_variant_guard.h',
                     'namei_vfat.c', 'namei_fat12.c', 'namei_fat16.c'):
        path = source / filename
        assert path.is_file() and not path.is_symlink(), path
    helper = app / 'Helpers/fat-kernel-build'
    assert helper.is_file() and not helper.is_symlink() and helper.stat().st_mode & 0o111
    for bits in (12, 16):
        efi = app / f'Resources/EFI/drivers_x64/FAT{bits}.efi'
        ko = app / f'Resources/Kernel/6.12.107-qemu-test-only/fat{bits}.ko'
        assert efi.is_file() and not efi.is_symlink()
        assert ko.is_file() and not ko.is_symlink()
        data = ko.read_bytes()
        assert data[:6] == b'\x7fELF\x02\x01', ko
        assert struct.unpack_from('<H', data, 16)[0] == 1, ko  # relocatable
        assert struct.unpack_from('<H', data, 18)[0] == 62, ko  # x86-64
        if args.efi_dir:
            assert efi.read_bytes() == (args.efi_dir / efi.name).read_bytes(), efi
        if args.ko_dir:
            assert data == (args.ko_dir / ko.name).read_bytes(), ko
print('FAT12/FAT16 qualified driver package: PASS')
