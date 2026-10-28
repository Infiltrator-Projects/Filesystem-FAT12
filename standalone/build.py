#!/usr/bin/env python3
"""Build precisely one freestanding Linux FAT shared object."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WIDTH = int((ROOT / "standalone" / "WIDTH").read_text().strip())
NAME = f"FAT{WIDTH}.so"
BASE = ROOT / "native/filesystems/fat"
DEF = BASE / "defrag"
MOD = BASE / "module"
PORT = BASE / "portable"
SOURCES = [MOD / f"fat{WIDTH}_module.c"] + [
    PORT / name for name in ("fatfs_module.c", "ff.c", "ffunicode.c", "fatfs_libc.c")
] + [DEF / name for name in ("writer.c", "fat_analysis.c", "fat_directory.c",
    "fat_relayout.c", "fat_io.c", "fat_journal.c", "fat_relocation.c",
    "fat_volume.c", "fat_defrag_module.c")]
INCLUDES = [MOD, PORT, DEF, DEF / "host", ROOT / "src/infiltratr-common/include"]
EXPORTS = ("infiltrator_filesystem_module_v1", "infiltrator_filesystem_module_v3",
           "infiltrator_filesystem_module_v4", "infiltrator_filesystem_defrag_v1")

def run(args):
    subprocess.run([str(a) for a in args], check=True)

def inspect(module):
    if module.name != NAME or not module.is_file():
        raise RuntimeError("Missing correctly named filesystem module")
    elf = subprocess.check_output(["readelf", "-h", str(module)], text=True)
    if "DYN (Shared object file)" not in elf or "Advanced Micro Devices X86-64" not in elf:
        raise RuntimeError("Module is not an x86-64 ELF shared object")
    exports = subprocess.check_output(["readelf", "-Ws", str(module)], text=True)
    for symbol in EXPORTS:
        if symbol not in exports:
            raise RuntimeError("Missing ABI export: " + symbol)
    meta = subprocess.check_output(["readelf", "-p", ".ifsmeta", str(module)], text=True)
    for api in (1, 3, 4):
        if f"IFS-MODULE:fat{WIDTH}:{api}" not in meta:
            raise RuntimeError("Missing exact FAT format identity")
    if "(NEEDED)" in subprocess.check_output(["readelf", "-d", str(module)], text=True):
        raise RuntimeError("Unexpected runtime shared-library dependency")

def build(profile, module):
    module.parent.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-fPIC", "-fno-stack-protector", "-fno-builtin",
             "-ffreestanding", "-fvisibility=hidden", "-D_GNU_SOURCE",
             "-D_FILE_OFFSET_BITS=64", f"-DIFS_FATFS_ONLY={WIDTH}",
             f"-DIFS_FAT_DEFRAG_BITS={WIDTH}"]
    if profile == "portable":
        flags += ["-O2", "-march=x86-64", "-mtune=generic"]
    else:
        flags += ["-O3", "-march=native", "-mtune=native", "-flto=auto"]
    for directory in INCLUDES:
        if not directory.is_dir():
            raise RuntimeError("Required copied source missing: " + str(directory))
    cmd = [os.environ.get("CC", "cc"), *flags]
    for directory in INCLUDES:
        cmd += ["-I", directory]
    cmd += ["-nostdlib", "-shared", "-Wl,-z,defs", "-Wl,-z,relro",
            "-Wl,-z,now", "-o", module, *SOURCES]
    run(cmd)
    inspect(module)
    return flags

def test(module):
    exe = module.parent / f"fat{WIDTH}-module-test"
    run([os.environ.get("CC", "cc"), "-std=c11", "-O2",
         "-Wall", "-Wextra", "-Werror", "-I", MOD,
         ROOT / "tests" / f"fat{WIDTH}_module_test.c",
         module, "-Wl,-rpath,$ORIGIN", "-o", exe])
    run([exe])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=("portable", "aggressive"), default="portable")
    parser.add_argument("--output", type=Path, default=ROOT / "build" / NAME)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    module = args.output.resolve()
    flags = build(args.profile, module)
    if args.check:
        test(module)
    print("QUALIFIED", NAME, args.profile, " ".join(flags))

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as exc:
        sys.exit(f"Build failed: {exc}")
