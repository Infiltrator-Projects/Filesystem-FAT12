#!/usr/bin/env python3
"""Check native module source hashing through the installed CLI from any cwd.

The 0.5.36 bug ran find from /usr/lib/filesystems but sha256sum from the
caller's working directory, causing legitimate source paths to be missing.
This fixture uses the real helper scripts and module source tree without root,
kernel mutations, or installing development packages.
"""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

source = Path(sys.argv[1]).resolve()
base_manifest = source / "data/native-filesystems.tsv"

with tempfile.TemporaryDirectory(prefix="filesystems-native-digest-") as tmp:
    temp = Path(tmp)
    package = temp / "usr/lib/filesystems"
    (package / "native/filesystems/ofs").mkdir(parents=True)
    (package / "native/platform/linux_kernel").mkdir(parents=True)
    (temp / "unrelated/working-directory").mkdir(parents=True)
    shutil.copy2(base_manifest, package / "native-filesystems.tsv")

    for helper in ("kernel-module-helper", "native-module-helper"):
        shutil.copy2(source / "packaging/linux" / (helper + ".sh"),
                     package / helper)
        (package / helper).chmod(0o755)

    native_source = source / "native/filesystems/ofs"
    deployed_source = package / "native/filesystems/ofs"
    for kind in ("core", "linux"):
        shutil.copytree(native_source / kind, deployed_source / kind)
    shutil.copy2(source / "native/platform/linux_kernel/vfs_compat.h",
                 package / "native/platform/linux_kernel/vfs_compat.h")

    def expected_digest() -> str:
        paths = []
        for kind in ("core", "linux"):
            root = deployed_source / kind
            paths.extend(p.relative_to(package) for p in root.rglob("*")
                         if p.is_file() and
                         (p.suffix in (".c", ".h") or p.name == "Makefile"))
        consumer = any(
            b"platform/linux_kernel/vfs_compat.h" in
            (package / rel).read_bytes()
            for rel in paths if rel.suffix in (".c", ".h")
        )
        if consumer:
            paths.append(Path("native/platform/linux_kernel/vfs_compat.h"))
        payload = b"".join(
            hashlib.sha256((package / rel).read_bytes()).hexdigest().encode()
            + b"  " + rel.as_posix().encode() + b"\n"
            for rel in sorted(paths, key=lambda x: os.fsencode(x.as_posix()))
        )
        return hashlib.sha256(payload).hexdigest()

    def query(cwd: Path, fs: str = "ofs", coordinator: bool = True):
        binary = "native-module-helper" if coordinator else "kernel-module-helper"
        return subprocess.run(
            [str(package / binary), "source-digest", fs],
            cwd=cwd, text=True, capture_output=True
        )

    expected = expected_digest()
    for cwd in (temp / "unrelated/working-directory", temp, package):
        for coordinator in (True, False):
            result = query(cwd, coordinator=coordinator)
            assert result.returncode == 0, (cwd, coordinator, result.stderr)
            assert result.stdout.strip() == expected, (
                cwd, result.stdout.strip(), expected
            )
            assert "No such file" not in result.stderr

    # A documentation change must not trigger recompilation.
    (deployed_source / "core/README.md").write_text("unrelated description\n")
    assert query(temp).stdout.strip() == expected

    # A real input mutation must change the digest regardless of caller cwd.
    changed_file = deployed_source / "core/ofs_primitives.c"
    original = changed_file.read_bytes()
    changed_file.write_bytes(original + b"\n/* digest regression */\n")
    updated = expected_digest()
    assert updated != expected
    for cwd in (temp, temp / "unrelated/working-directory"):
        result = query(cwd)
        assert result.returncode == 0, result.stderr
        assert result.stdout.strip() == updated
    changed_file.write_bytes(original)
    assert query(temp).stdout.strip() == expected

    # Reject unknown filesystem identities and incomplete packaged sources.
    unknown = query(temp, "unmanaged")
    assert unknown.returncode == 2, unknown
    assert "refusing unmanaged native filesystem" in unknown.stderr
    (deployed_source / "linux/Makefile").unlink()
    missing = query(temp)
    assert missing.returncode != 0, missing
    assert "packaged native source is incomplete" in missing.stderr

print("Native module digest: independent CWD, exact content, source mutations and safe failures: PASS")
