#!/usr/bin/env python3
"""Disposable classic FAT images at the FAT12/FAT16 cluster boundary.

FatFs and the Linux/UEFI drivers disagree at exactly 4085 clusters. The
ambiguous middle fixture is for rejection, never for destructive testing.
"""
import struct
import sys
from pathlib import Path

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for clusters, width, fat_sectors in ((4084, 12, 12), (4085, 16, 16), (4086, 16, 16)):
    sectors = 1 + 2 * fat_sectors + 32 + clusters
    image = bytearray(sectors * 512)
    image[0:3] = b'\xeb\x3c\x90'
    image[3:11] = b'MSDOS5.0'
    struct.pack_into('<HBHBHHBHHHII', image, 11, 512, 1, 1, 2, 512,
                     sectors, 0xf8, fat_sectors, 32, 64, 0, 0)
    image[36] = 0x80
    image[38] = 0x29
    struct.pack_into('<I', image, 39, 0x12345678)
    image[43:54] = b'NO NAME    '
    image[54:62] = f'FAT{width}   '.encode()
    image[510:512] = b'\x55\xaa'
    for copy in range(2):
        offset = (1 + copy * fat_sectors) * 512
        image[offset:offset + (3 if width == 12 else 4)] = (
            b'\xf8\xff\xff' if width == 12 else b'\xf8\xff\xff\xff'
        )
    (out / f'boundary-{clusters}.img').write_bytes(image)

dos1 = bytearray(512)
dos1[0:3] = b'\xeb\x00\x90'
(out / 'dos1-boot-sector.img').write_bytes(dos1)

# DOS 1.x 360 KiB floppy: no BPB, two 2-sector FATs, 112 root entries,
# 2 sectors/cluster and media byte 0xfd (the Linux static geometry table).
floppy = bytearray(720 * 512)
floppy[0:3] = b'\xeb\x00\x90'
for offset in (512, 3 * 512):
    floppy[offset:offset + 3] = b'\xfd\xff\xff'
(out / 'dos1-floppy.img').write_bytes(floppy)
