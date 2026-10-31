#!/usr/bin/env python3
"""Compile one FAT kernel module against a prepared, matching Linux build tree.

The installed reference .ko is usable only with its tested kernel ABI.
Build the active host version before attempting to load or mount via fat12/fat16.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
if (ROOT / "standalone/WIDTH").is_file():
    WIDTH = int((ROOT / "standalone/WIDTH").read_text().strip())
elif (HERE / "WIDTH").is_file():
    WIDTH = int((HERE / "WIDTH").read_text().strip())
else:
    raise SystemExit("Missing standalone FAT width manifest")
SOURCE = ROOT / "native/filesystems/fat/linux"
if not SOURCE.is_dir():
    SOURCE = HERE / "linux"
MODULE = f"fat{WIDTH}.ko"
OTHER = 16 if WIDTH == 12 else 12

def run(cmd):
    subprocess.run([str(x) for x in cmd], check=True)

def main():
    parser = argparse.ArgumentParser(description="Build a single matching FAT kernel module")
    parser.add_argument("--kernel-tree", type=Path, default=Path("/lib/modules") / os.uname().release / "build")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    kernel = args.kernel_tree.resolve(strict=True)
    if not (kernel / "Makefile").is_file() or not (kernel / "Module.symvers").is_file():
        raise RuntimeError("Kernel headers and prepared Module.symvers are required")
    # Distribution headers can have an upstream Makefile version (6.8.12)
    # while the prepared module ABI uses a distro release (6.8.0-147-generic).
    # The generated release is what Kbuild actually puts into vermagic.
    generated_release = kernel / "include/config/kernel.release"
    generated_uts = kernel / "include/generated/utsrelease.h"
    if generated_release.is_file():
        release = generated_release.read_text().strip()
    elif generated_uts.is_file():
        import re
        match = re.search(r'^#define UTS_RELEASE "([^"]+)"$', generated_uts.read_text(), re.M)
        if not match:
            raise RuntimeError("Prepared kernel has invalid UTS_RELEASE")
        release = match.group(1)
    else:
        release = subprocess.check_output(
            ["make", "-s", "-C", str(kernel), "kernelrelease"], text=True
        ).strip()
    if not release or "/" in release:
        raise RuntimeError("Prepared kernel has invalid kernelrelease")
    if not SOURCE.is_dir() or not (SOURCE / f"namei_fat{WIDTH}.c").is_file():
        raise RuntimeError("The selected FAT kernel driver sources were not packaged")
    # Linux FAT core APIs changed between 6.8 and 6.12. Never link a 6.12
    # namespace driver against a 6.8 core with undefined fat_* functions.
    compat68 = release.startswith("6.8.")
    if not compat68 and not release.startswith("6.12."):
        raise RuntimeError("Unsupported Linux FAT-core ABI: " + release +
                           " (qualified source variants: 6.8 and 6.12)")
    target_dir = args.output.resolve()
    target_dir.mkdir(parents=True, exist_ok=True)
    destination = target_dir / MODULE
    if destination.exists() or destination.is_symlink():
        raise RuntimeError("Refusing to overwrite existing driver: " + str(destination))
    with tempfile.TemporaryDirectory(prefix=f"fat{WIDTH}-kernel-") as tmp:
        work = Path(tmp)
        shutil.copytree(SOURCE, work / "src")
        src = work / "src"
        if compat68:
            legacy = SOURCE / "compat" / "linux-6.8"
            for filename in ("fat.h", "namei_vfat.c"):
                if not (legacy / filename).is_file():
                    raise RuntimeError("Missing locally copied Linux 6.8 FAT compatibility source")
                shutil.copyfile(legacy / filename, src / filename)
        # Build exactly this repository's selected filesystem, not both copies.
        (src / "Makefile").write_text(
            "# SPDX-License-Identifier: GPL-2.0\n"
            f"obj-m += fat{WIDTH}.o\nfat{WIDTH}-y := namei_fat{WIDTH}.o\n")
        run(["make", "-C", kernel, f"M={src}", "modules"])
        built = src / MODULE
        if not built.is_file():
            raise RuntimeError("Kernel build produced no " + MODULE)
        name = subprocess.check_output(["modinfo", "-F", "name", str(built)], text=True).strip()
        vermagic = subprocess.check_output(["modinfo", "-F", "vermagic", str(built)], text=True).strip()
        if name != f"fat{WIDTH}" or vermagic.split(" ")[0] != release:
            raise RuntimeError(
                f"Compiled FAT driver does not match the kernel ABI: "
                f"expected name=fat{WIDTH}, release={release!r}; "
                f"actual name={name!r}, vermagic={vermagic!r}"
            )
        if (src / f"fat{OTHER}.ko").exists():
            raise RuntimeError("Unexpected other-format module built")
        shutil.copyfile(built, destination)
        destination.chmod(0o644)
        print("Verified", destination, "for kernel", release)

if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        sys.exit("FAT kernel build failed: " + str(exc))
