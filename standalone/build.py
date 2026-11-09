#!/usr/bin/env python3
"""Build precisely one freestanding Linux FAT shared object."""
import argparse
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WIDTH = int((ROOT / "standalone" / "WIDTH").read_text().strip())
NAME = f"FAT{WIDTH}.so"
BASE = ROOT / "native/filesystems/fat"
DEF = BASE / "defrag"
MOD = BASE / "module"
PORT = BASE / "portable"
SOURCES = [MOD / "classic_fat_module.c"] + [
    PORT / name for name in ("fatfs_module.c", "fatfs_boot.c", "ff.c", "ffunicode.c", "fatfs_libc.c")
] + [DEF / name for name in ("writer.c", "fat_analysis.c", "fat_directory.c",
    "fat_relayout.c", "fat_io.c", "fat_journal.c", "fat_relocation.c",
    "fat_volume.c", "fat_defrag_module.c")]
INCLUDES = [MOD, PORT, DEF, DEF / "host", ROOT / "native/include", ROOT / "src/infiltratr-common/include"]
EXPORTS = ("infiltrator_filesystem_api_v1", "infiltrator_filesystem_defrag_v1",
           "infiltrator_filesystem_boot_v1")

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
    if f"IFS-MODULE:fat{WIDTH}:common1" not in meta:
        raise RuntimeError("Missing canonical FAT filesystem identity")
    for obsolete in ("infiltrator_filesystem_module_v1",
                     "infiltrator_filesystem_module_v3",
                     "infiltrator_filesystem_module_v4"):
        if obsolete in exports:
            raise RuntimeError("Historical filesystem ABI still exported: " + obsolete)
    if "(NEEDED)" in subprocess.check_output(["readelf", "-d", str(module)], text=True):
        raise RuntimeError("Unexpected runtime shared-library dependency")
    manifest = struct.pack("<8sIIII16s32s", b"IFSBOOT1", 72, 1, 62, 1,
                           f"fat{WIDTH}".encode(), f"FAT{WIDTH}".encode())
    if manifest not in module.read_bytes():
        raise RuntimeError("Missing exact freestanding Boot manifest")
    relocations = subprocess.check_output(["readelf", "-Wr", str(module)], text=True)
    for line in relocations.splitlines():
        if "R_X86_64_" in line and "R_X86_64_RELATIVE" not in line:
            raise RuntimeError("Unsupported Boot relocation: " + line)
    for line in subprocess.check_output(["readelf", "--dyn-syms", "-W", str(module)], text=True).splitlines():
        fields = line.split()
        if "UND" in fields and fields[0] != "0:":
            raise RuntimeError("Undefined Boot import: " + line)

def compile_module(module, flags, training=False):
    """Use host profiling libraries only for the throwaway instrumented pass."""
    args = [os.environ.get("CC", "cc"), *flags]
    for directory in INCLUDES:
        args += ["-I", directory]
    if not training:
        args += ["-nostdlib"]
    args += ["-shared", "-Wl,-Bsymbolic", "-Wl,-z,defs", "-Wl,-z,relro",
             "-Wl,-z,now", "-o", module, *SOURCES]
    run(args)
    if not training:
        inspect(module)


def train_profile(module, workspace):
    """Exercise the one canonical ABI with disposable media for PGO."""
    test(module)


def build(profile, module):
    module.parent.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-fPIC", "-mno-red-zone", "-fno-stack-protector", "-fno-builtin",
             "-fno-tree-loop-distribute-patterns", "-U_FORTIFY_SOURCE", "-D_FORTIFY_SOURCE=0",
             "-ffreestanding", "-fvisibility=hidden", "-D_GNU_SOURCE",
             "-D_FILE_OFFSET_BITS=64", f"-DIFS_FATFS_ONLY={WIDTH}",
             f"-DIFS_FAT_DEFRAG_BITS={WIDTH}", f"-DIFS_CLASSIC_FAT_BITS={WIDTH}"]
    if profile == "portable":
        flags += ["-O2", "-march=x86-64", "-mtune=generic"]
    else:
        flags += ["-O3", "-march=native", "-mtune=native", "-flto=auto",
                  "-mno-avx", "-mno-avx2", "-mno-avx512f", "-mno-amx-tile"]
    for directory in INCLUDES:
        if not directory.is_dir():
            raise RuntimeError("Required copied source missing: " + str(directory))
    if profile == "portable":
        compile_module(module, flags)
    else:
        # Identical module/source paths across generate/use passes are required.
        with tempfile.TemporaryDirectory(prefix=f"fat{WIDTH}-pgo-", dir=module.parent) as tmp:
            workspace = Path(tmp)
            profiles = workspace / "profiles"
            profiles.mkdir()
            compile_module(module, flags + ["-fprofile-generate=" + str(profiles)],
                           training=True)
            train_profile(module, workspace)
            if not any(p.is_file() and p.stat().st_size for p in profiles.rglob("*.gcda")):
                raise RuntimeError("FAT PGO training did not produce measured profile data")
            compile_module(module, flags + ["-fprofile-use=" + str(profiles),
                                          "-fprofile-correction", "-fprofile-partial-training"])
        inspect(module)  # final release must have zero runtime library dependencies
    return flags

def test(module):
    exe = module.parent / f"fat{WIDTH}-module-test"
    run([os.environ.get("CC", "cc"), "-std=c11", "-O2",
         "-Wall", "-Wextra", "-Werror", "-I", ROOT / "src/infiltratr-common/include",
         ROOT / "tests" / f"fat{WIDTH}_module_test.c",
         module, "-Wl,-rpath,$ORIGIN", "-o", exe])
    run([exe])
    format_exe = module.parent / f"fat{WIDTH}-common-format-test"
    run([os.environ.get("CC", "cc"), "-std=c11", "-O2",
         "-Wall", "-Wextra", "-Werror", "-I", ROOT / "src/infiltratr-common/include",
         ROOT / "tests/fatfs_common_format_test.c", "-ldl", "-o", format_exe])
    with tempfile.TemporaryDirectory(prefix=f"fat{WIDTH}-common-media-") as temp:
        for size in (1440 * 1024, 255 * 1024 * 1024):
            image = Path(temp) / f"fat12-{size}.img"
            with image.open("wb") as target:
                target.truncate(size)
            run([format_exe, module, image, "512"])


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

# Shared standalone release.py adapter. Keep native compilation filesystem-specific,
# but use the identical verified Python source-embedded RUN/DEB packager everywhere.
REPO = "Filesystem-FAT12"
SLUG = REPO.lower()
PACKAGE = "infiltrator-" + SLUG

PDF = Path("docs/FAT12.pdf")

def install_deps():
    # Resolve only build-time tools; installed .so and PDF have no Python dependency.
    packages = ("build-essential", "binutils", "python3", "dpkg-dev")
    missing = [p for p in packages if subprocess.run(
        ["dpkg-query", "-W", "-f=$" + "{Status}", p],
        capture_output=True, text=True).stdout.strip() != "install ok installed"]
    tools = ("cc", "readelf", "dpkg-deb", "dpkg-query")
    if missing or any(__import__("shutil").which(cmd) is None for cmd in tools):
        prefix = [] if os.geteuid() == 0 else ["sudo"]
        apt = [*prefix, "apt-get", "-o", "DPkg::Lock::Timeout=-1"]
        run([*apt, "update"])
        run([*apt, "install", "-y", "--no-install-recommends", *packages])

def dependencies(module):
    inspect(module)
    return "libc6"

def prepare_release_source():
    pass

def embedded_source_files():
    return []

def cleanup_test_artifacts(module):
    (module.parent / f"fat{WIDTH}-module-test").unlink(missing_ok=True)
    (module.parent / f"fat{WIDTH}-common-format-test").unlink(missing_ok=True)

def release_source_files():
    if (ROOT / ".git").exists():
        names = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "-z"])
        paths = [Path(os.fsdecode(p)) for p in names.split(b"\0") if p]
    else:
        paths = [p.relative_to(ROOT) for p in ROOT.rglob("*") if p.is_file()]
    chosen = []
    for p in paths:
        q = p.as_posix()
        selected = (
            q.startswith("native/filesystems/fat/") or
            q.startswith("src/infiltratr-common/include/") or
            q.startswith("native/include/") or
            q.startswith("standalone/") or q.startswith("docs/") or
            q in ("README.md", "VERSION", "MIGRATION-PROVENANCE.md",
                  "src/infiltratr-common/LICENSE", f"tests/fat{WIDTH}_module_test.c",
                  "tests/fatfs_common_format_test.c")
        )
        if selected and (ROOT / p).is_file() and not (ROOT / p).is_symlink():
            chosen.append(p)
    if len(chosen) < 35:
        raise RuntimeError("Insufficient source files for full independent build")
    return sorted(set(chosen))
