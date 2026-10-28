#!/usr/bin/env python3
"""Reject incomplete or substituted SO/KO/EFI FAT release payloads."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

WIDTH = int((Path(__file__).resolve().parent / "WIDTH").read_text().strip())
name = f"Filesystem-FAT{WIDTH}"
lower = f"fat{WIDTH}"
sources = Path(__file__).resolve().parent / "qualified-drivers"

p = argparse.ArgumentParser()
p.add_argument("package", type=Path)
p.add_argument("--drivers", type=Path, default=sources)
args = p.parse_args()

expected = {
    f"System/Filesystems/FAT{WIDTH}.so": None,
    f"System/Kernel/Modules/6.12.107-qemu-tested-only/{lower}.ko": args.drivers / f"{lower}.ko",
    f"System/Kernel/Firmware/EFI/FAT{WIDTH}.efi": args.drivers / f"FAT{WIDTH}.efi"
}
with tempfile.TemporaryDirectory(prefix=lower+"-payload-test-") as d:
    root = Path(d)
    subprocess.run(["dpkg-deb", "-x", str(args.package.resolve()), d], check=True)
    for installed, reference in expected.items():
        file = root / installed
        assert file.is_file() and not file.is_symlink() and file.stat().st_size > 5000, installed
        if reference:
            assert file.read_bytes() == reference.read_bytes(), f"Tested payload altered: {installed}"
    other = 16 if WIDTH == 12 else 12
    assert not list(root.rglob(f"*FAT{other}*")), "Foreign filesystem in package"
    assert not list(root.rglob(f"*fat{other}.ko")), "Foreign kernel module in package"
    record = (root / "System/Resources/Documentation" / name / "BUILD-INFO").read_text()
    for installed, key in (
        (f"System/Filesystems/FAT{WIDTH}.so", "SHA256"),
        (f"System/Kernel/Modules/6.12.107-qemu-tested-only/{lower}.ko", "Kernel-SHA256"),
        (f"System/Kernel/Firmware/EFI/FAT{WIDTH}.efi", "EFI-SHA256"),
    ):
        digest = hashlib.sha256((root / installed).read_bytes()).hexdigest()
        assert f"{key}: {digest}\n" in record, key
    assert f"Kernel-Tested-Version: 6.12.107\n" in record
print("SO/KO/EFI release payload identity and exact tested bytes PASS for FAT", WIDTH)
