#!/usr/bin/env python3
"""Release exactly one filesystem's DEB and source-embedded native RUN."""
import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

import build as fsbuild

ROOT = Path(__file__).resolve().parents[1]
WIDTH = int((ROOT / "standalone/WIDTH").read_text().strip())
REPO = f"Filesystem-FAT{WIDTH}"
SLUG = f"filesystem-fat{WIDTH}"
PACKAGE = f"infiltrator-filesystem-fat{WIDTH}"
MODULE = f"FAT{WIDTH}.so"
VERSION = (ROOT / "VERSION").read_text().strip()
if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", VERSION):
    raise SystemExit("Invalid standalone VERSION")
DIST = ROOT / "dist"
DRIVERS = ROOT / "standalone/qualified-drivers"
KERNEL_TEST_VERSION = "6.12.107"
KERNEL_TEST_LABEL = "6.12.107-qemu-tested-only"

def qualified_driver(name):
    source = DRIVERS / name
    if not source.is_file() or source.is_symlink() or source.stat().st_size < 5000:
        raise RuntimeError("Missing tested driver payload: " + str(source))
    data = source.read_bytes()
    if name.endswith(".ko"):
        if data[:4] != b"\x7fELF" or data[4] != 2:
            raise RuntimeError("Not an ELF64 kernel module")
        identity = subprocess.check_output(["modinfo", "-F", "name", source], text=True).strip()
        vermagic = subprocess.check_output(["modinfo", "-F", "vermagic", source], text=True)
        if identity != f"fat{WIDTH}" or KERNEL_TEST_VERSION not in vermagic:
            raise RuntimeError("Kernel module name or kernel ABI mismatch")
    elif name.endswith(".efi"):
        import struct
        if data[:2] != b"MZ":
            raise RuntimeError("Not a PE/COFF UEFI driver")
        off = struct.unpack_from("<I", data, 0x3c)[0]
        if data[off:off+4] != b"PE\x00\x00" or struct.unpack_from("<H", data, off+4)[0] != 0x8664 or struct.unpack_from("<H", data, off+24)[0] != 0x20b or struct.unpack_from("<H", data, off+24+68)[0] != 11:
            raise RuntimeError("Not an x64 EFI boot-services driver")
    return source

def stage_drivers(stage):
    ksource = qualified_driver(f"fat{WIDTH}.ko")
    esource = qualified_driver(f"FAT{WIDTH}.efi")
    ktarget = stage / "System/Kernel/Modules" / KERNEL_TEST_LABEL / ksource.name
    etarget = stage / "System/Kernel/Firmware/EFI" / esource.name
    for src, target in ((ksource, ktarget), (esource, etarget)):
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, target)
    return hashlib.sha256(ktarget.read_bytes()).hexdigest(), hashlib.sha256(etarget.read_bytes()).hexdigest()


def run(args):
    subprocess.run([str(a) for a in args], check=True)

def build_deb(profile, dest):
    with tempfile.TemporaryDirectory(prefix=SLUG + "-package-") as tmp:
        root = Path(tmp) / "root"
        module = root / "System/Filesystems" / MODULE
        module.parent.mkdir(parents=True)
        flags = fsbuild.build(profile, module)
        fsbuild.test(module)
        # The fixture test is an ephemeral QA executable, never a filesystem driver.
        (module.parent / f"fat{WIDTH}-module-test").unlink()
        ko_sha, efi_sha = stage_drivers(root)
        # Ship the selected VFS driver source and independent host-kernel
        # builder, not only a binary for the reference QEMU kernel.
        host_source = root / "System/Kernel/Sources" / REPO
        host_source.mkdir(parents=True)
        shutil.copytree(ROOT / "native/filesystems/fat/linux", host_source / "linux")
        shutil.copy2(ROOT / "standalone/kernel.py", host_source / "kernel.py")
        (host_source / "WIDTH").write_text(str(WIDTH) + "\n")
        info_dir = root / "System/Resources/Documentation" / REPO
        info_dir.mkdir(parents=True)
        digest = hashlib.sha256(module.read_bytes()).hexdigest()
        (info_dir / "BUILD-INFO").write_text(
            f"Version: {VERSION}\nModule: /System/Filesystems/{MODULE}\n"
            f"Package: {PACKAGE}\nProfile: {profile}\n"
            f"C flags: {' '.join(flags)}\nSHA256: {digest}\n"
            f"Kernel-SHA256: {ko_sha}\nEFI-SHA256: {efi_sha}\n"
            f"Kernel-Tested-Version: {KERNEL_TEST_VERSION}\n"
            f"Kernel-Compatibility: QEMU-tested reference only; rebuild for host kernel\n")
        for item in ("README.md", "MIGRATION-PROVENANCE.md"):
            shutil.copy2(ROOT / item, info_dir / item)
        control = root / "DEBIAN"
        control.mkdir()
        deb_version = VERSION + ("+native1" if profile == "aggressive" else "")
        (control / "control").write_text(
            f"Package: {PACKAGE}\nVersion: {deb_version}\nArchitecture: amd64\n"
            "Section: admin\nPriority: optional\n"
            "Maintainer: Infiltrator Projects <noreply@infiltrator-projects.invalid>\n"
            "Depends: libc6\n"
            f"Description: independent FAT{WIDTH} SO, kernel and UEFI filesystem driver\n"
            f" Ships FAT{WIDTH}.so, OVMF-tested FAT{WIDTH}.efi and QEMU-tested fat{WIDTH}.ko.\n"
            f" Kernel module is restricted to tested Linux {KERNEL_TEST_VERSION}; rebuild for host kernel.\n")
        dest.parent.mkdir(parents=True, exist_ok=True)
        run(["dpkg-deb", "--root-owner-group", "--build", root, dest])
        content = subprocess.check_output(["dpkg-deb", "--contents", str(dest)], text=True)
        if f"./System/Filesystems/{MODULE}" not in content:
            raise RuntimeError("Missing selected module in package")
        foreign = 16 if WIDTH == 12 else 12
        if f"./System/Filesystems/FAT{foreign}.so" in content:
            raise RuntimeError("Package owns the opposite filesystem")
        for path in (f"./System/Kernel/Firmware/EFI/FAT{WIDTH}.efi",
                     f"./System/Kernel/Modules/{KERNEL_TEST_LABEL}/fat{WIDTH}.ko"):
            if path not in content:
                raise RuntimeError("Package missing qualified kernel or EFI driver: " + path)
    return dest

def source_files():
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
            q.startswith("standalone/") or q.startswith("docs/") or
            q.startswith("scripts/build-fat12-fat16-efi.sh") or
            q.startswith("packaging/linux/fat-kernel-build.sh") or
            q in ("README.md", "VERSION", "MIGRATION-PROVENANCE.md",
                  "src/infiltratr-common/LICENSE", f"tests/fat{WIDTH}_module_test.c")
        )
        if selected and (ROOT / p).is_file() and not (ROOT / p).is_symlink():
            chosen.append(p)
    if len(chosen) < 35:
        raise RuntimeError("Insufficient source files for full independent build")
    # Include only the actual previously qualified binaries, not an
    # unchecked build or a source-only placeholder. The standalone RUN
    # must carry the exact KO/EFI tested by QEMU and OVMF.
    for filename in (f"fat{WIDTH}.ko", f"FAT{WIDTH}.efi"):
        qualified_driver(filename)
        chosen.append(Path("standalone/qualified-drivers") / filename)
    return sorted(set(chosen))

def create_run(path):
    with tempfile.TemporaryFile() as temp:
        with tarfile.open(fileobj=temp, mode="w:gz", format=tarfile.PAX_FORMAT) as tar:
            for p in source_files():
                data = (ROOT / p).read_bytes()
                entry = tarfile.TarInfo(p.as_posix())
                entry.size, entry.mode, entry.uid, entry.gid, entry.mtime = len(data), 0o644, 0, 0, 0
                tar.addfile(entry, io.BytesIO(data))
        temp.seek(0)
        payload = temp.read()
    digest = hashlib.sha256(payload).hexdigest()
    header = """#!/bin/sh
set -eu
if [ "$#" -gt 0 ] && [ "$1" = "--help" ]; then
    echo "Usage: __NAME__ [--profile aggressive] [--system-package-mode] [--build-only --output DIRECTORY]"
    exit 0
fi
temp=$(mktemp -d)
trap 'rm -rf -- "$temp"' EXIT HUP INT TERM
line=$(awk '/^__FAT_SOURCE_PAYLOAD_BELOW__$/ {print NR + 1; exit}' "$0")
test -n "$line"
tail -n +"$line" "$0" > "$temp/source.tar.gz"
printf '%s  %s\n' '__SHA256__' "$temp/source.tar.gz" | sha256sum -c - >/dev/null
mkdir "$temp/source"
tar -xzf "$temp/source.tar.gz" -C "$temp/source" --no-same-owner --no-same-permissions
python3 "$temp/source/standalone/release.py" install "$@"
exit $?
__FAT_SOURCE_PAYLOAD_BELOW__
""".replace("__NAME__", path.name).replace("__SHA256__", digest)
    path.write_bytes(header.encode("utf-8") + payload)
    path.chmod(0o755)

def release():
    DIST.mkdir(exist_ok=True)
    if list(DIST.iterdir()):
        raise RuntimeError("Refusing to mix release artifacts; clean dist first")
    deb = build_deb("portable", DIST / f"{PACKAGE}_{VERSION}_amd64.deb")
    runfile = DIST / f"{SLUG}-{VERSION}-native-installer.run"
    create_run(runfile)
    for artifact in (deb, runfile):
        print(artifact.name, hashlib.sha256(artifact.read_bytes()).hexdigest())
    if len(list(DIST.iterdir())) != 2:
        raise RuntimeError("Release must contain exactly the DEB and RUN")

def install(rest):
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=("portable", "aggressive"), default="aggressive")
    parser.add_argument("--system-package-mode", action="store_true")
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--output", type=Path)
    opt = parser.parse_args(rest)
    with tempfile.TemporaryDirectory(prefix=SLUG + "-install-") as tmp:
        output = (opt.output if opt.build_only and opt.output else
                  Path(tmp) / f"{PACKAGE}_{VERSION}+native1_amd64.deb").resolve()
        build_deb(opt.profile, output)
        if opt.build_only:
            print(output)
            return
        args = ["dpkg", "-i", output]
        if os.geteuid() != 0:
            args.insert(0, "sudo")
        run(args)
        installed = Path("/System/Filesystems") / MODULE
        fsbuild.inspect(installed)
        for filename, dest in ((f"fat{WIDTH}.ko", Path("/System/Kernel/Modules") / KERNEL_TEST_LABEL / f"fat{WIDTH}.ko"),
                               (f"FAT{WIDTH}.efi", Path("/System/Kernel/Firmware/EFI") / f"FAT{WIDTH}.efi")):
            reference = qualified_driver(filename)
            if dest.read_bytes() != reference.read_bytes():
                raise RuntimeError("Installed driver differs from the tested binary: " + filename)
        record = (Path("/System/Resources/Documentation") / REPO / "BUILD-INFO").read_text()
        sha = hashlib.sha256(installed.read_bytes()).hexdigest()
        if f"SHA256: {sha}\n" not in record:
            raise RuntimeError("Installed shared object does not match BUILD-INFO")
        for label, filename in (("Kernel-SHA256", f"fat{WIDTH}.ko"),
                                ("EFI-SHA256", f"FAT{WIDTH}.efi")):
            data = qualified_driver(filename).read_bytes()
            if f"{label}: {hashlib.sha256(data).hexdigest()}\n" not in record:
                raise RuntimeError(label + " did not match BUILD-INFO")
        print("Installed and verified", installed)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("command", choices=("release", "install"))
    args, rest = p.parse_known_args()
    if args.command == "release":
        if rest:
            p.error("Unrecognised release options")
        release()
    else:
        install(rest)

if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        sys.exit(f"ERROR: {exc}")
