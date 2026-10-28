#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise production package commands through APT with a private database.

The dpkg executor is an inert recorder which always refuses mutation. Neither
APT nor dpkg can reach the host status database, package lists or archives.
Installation argv is captured from the production C++ and shell function bodies;
the fixture adds only private APT paths, never a removal restriction of its own.
"""

import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

guard = Path(sys.argv[1]).resolve()
repo = Path(sys.argv[2]).resolve()
compiler = sys.argv[3]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def installation_commands(root):
    # Keep the production validation/argv/execution sequence intact. Only the
    # allowlist lookup (covered separately) and privilege launcher are stubbed.
    # No public API or production command-building abstraction exists for tests.
    source = (repo / "src/installer.cpp").read_text(encoding="utf-8")
    harness = root / "capture-install.cpp"
    harness.write_text(r'''
#include "installer.hpp"
#include <cstdlib>
#include <iostream>
#include <utility>
namespace filesystems {
constexpr const char* kAptGet = "/usr/bin/apt-get";
bool packages_are_catalogued(const std::vector<std::string>&, std::string*)
{ return true; }
void run_privileged_async(const std::vector<std::string>& arguments,
                          std::string, ActionCompletion completion)
{
    for (const auto& argument : arguments)
        std::cout << argument << '\0';
    completion(true, "captured");
}
''' + function(source, "void install_packages_async(") + r'''
}
int main()
{
    filesystems::install_packages_async({"hfsprogs"},
        [](bool success, const std::string&) { if (!success) std::exit(1); });
}
''', encoding="utf-8")
    binary = root / "capture-install"
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(repo / "src"), str(harness), "-o", str(binary)],
                   check=True)
    desktop = subprocess.check_output([str(binary)], timeout=10)

    kernel_build = root / "fixture-kernel-build"
    captured = root / "prerequisite-argv"
    source = (repo / "packaging/linux/kernel-module-helper.sh").read_text(
        encoding="utf-8")
    harness = ("set -euo pipefail\n"
               f"kernel_build={shlex.quote(str(kernel_build))}\n"
               "kernel=fixture\n"
               "apt-get() {\n"
               "    if [[ $1 == update ]]; then return 0; fi\n"
               f"    printf '%s\\0' /usr/bin/apt-get \"$@\" > {shlex.quote(str(captured))}\n"
               "    mkdir -p \"$kernel_build\"\n"
               "    : > \"$kernel_build/Makefile\"\n"
               "}\n" + function(source, "ensure_build_environment() {") +
               "ensure_build_environment\n")
    subprocess.run(["bash", "-c", harness], check=True, timeout=10)
    prerequisites = captured.read_bytes()

    # Exercise the wrapper's final command after a formerly acceptable plan
    # has changed. Only its absolute executable is stubbed during argv capture;
    # real APT later sees the local Debian payload and the conflicting state.
    source = (repo / "packaging/build-run.sh").read_text(encoding="utf-8")
    wrapper_commands = re.findall(r"^/usr/bin/apt-get install .+$", source, re.M)
    assert len(wrapper_commands) == 1, wrapper_commands
    harness = (f"work={shlex.quote(str(root / 'repo'))}\n"
               "capture_apt() { printf '%s\\0' /usr/bin/apt-get \"$@\"; }\n" +
               wrapper_commands[0].replace("/usr/bin/apt-get", "capture_apt", 1))
    wrapper = subprocess.check_output(["bash", "-c", harness], timeout=10)
    commands = {}
    for name, data in (("desktop support", desktop),
                       ("native build prerequisites", prerequisites),
                       ("standalone installer", wrapper)):
        arguments = data.decode().split("\0")
        assert arguments.pop() == "", (name, arguments)
        assert arguments[0] == "/usr/bin/apt-get", (name, arguments)
        commands[name] = arguments
    return commands

with tempfile.TemporaryDirectory(prefix="filesystems-packages-") as temporary:
    root = Path(temporary)
    for directory in ("dpkg/updates", "lists/partial", "archives/partial",
                      "apt.conf.d", "sources.list.d", "repo"):
        (root / directory).mkdir(parents=True)
    status = root / "dpkg/status"
    marker = root / "dpkg-called"
    executor = root / "dpkg-recorder"
    executor.write_text(
        "#!/usr/bin/python3\n"
        "import pathlib, sys\n"
        "if '--version' in sys.argv:\n"
        "    print('Debian dpkg version 1.22.0 (amd64).')\n"
        "    raise SystemExit(0)\n"
        "if '--print-foreign-architectures' in sys.argv:\n"
        "    raise SystemExit(0)\n"
        "if '--print-architecture' in sys.argv:\n"
        "    print('amd64')\n"
        "    raise SystemExit(0)\n"
        "if any(a.startswith('--assert-') for a in sys.argv[1:]):\n"
        "    raise SystemExit(0)\n"
        "if '--set-selections' in sys.argv:\n"
        "    sys.stdin.read()\n"
        "    raise SystemExit(0)\n"
        f"pathlib.Path({str(marker)!r}).write_text(' '.join(sys.argv[1:]))\n"
        "raise SystemExit(42)\n",
        encoding="utf-8")
    executor.chmod(0o755)

    environment = os.environ.copy()
    environment.update(LC_ALL="C", DPKG_ADMINDIR=str(status.parent))
    # APT reads this before its normal configuration directories. Setting
    # Dir::Etc only on argv is too late to exclude host-defined dpkg hooks.
    configuration = root / "apt.conf"
    configuration.write_text(
        f'Dir::Etc::parts "{root / "apt.conf.d"}";\n'
        'Dir::Etc::main "/dev/null";\n', encoding="utf-8")
    environment["APT_CONFIG"] = str(configuration)
    sources = root / "sources.list"
    sources.touch()

    def installed(name, extra="", architecture="amd64"):
        return (f"Package: {name}\nStatus: install ok installed\n"
                f"Architecture: {architecture}\nVersion: 1\n"
                f"Description: disposable transaction fixture\n{extra}\n")

    def apt(arguments, approved=None):
        command = ["/usr/bin/apt-get"]
        options = {
            "Dir::State::status": str(status),
            "Dir::State::extended_states": str(root / "extended_states"),
            "Dir::State::lists": str(root / "lists"),
            "Dir::Cache::archives": str(root / "archives"),
            "Dir::Cache::pkgcache": "",
            "Dir::Cache::srcpkgcache": "",
            "Dir::Etc::main": "/dev/null",
            "Dir::Etc::parts": str(root / "apt.conf.d"),
            "Dir::Etc::sourcelist": str(sources),
            "Dir::Etc::sourceparts": str(root / "sources.list.d"),
            "Dir::Bin::dpkg": str(executor),
            "Dir::Log": str(root),
            "APT::Architecture": "amd64",
            # A private fixture needs no download user; some isolated test
            # containers cannot switch to the host's _apt numeric identity.
            "APT::Sandbox::User": "root",
            "Debug::NoLocking": "0",
            "DPkg::ConfigurePending": "0",
            "APT::Get::Purge": "0",
        }
        if approved is not None:
            options.update({
                "DPkg::Pre-Install-Pkgs::": shlex.join([str(guard), *approved]),
                f"DPkg::Tools::Options::{guard}::Version": "3",
                f"DPkg::Tools::Options::{guard}::InfoFD": "0",
            })
        for key, value in options.items():
            command.extend(["-o", f"{key}={value}"])
        command.extend(arguments)
        return subprocess.run(command, env=environment, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=30)

    def invoke(approved, database):
        status.write_text(database, encoding="utf-8")
        marker.unlink(missing_ok=True)
        return apt(["remove", "-y", "hfsprogs"], approved)

    approved = ["hfsprogs:amd64"]
    ordinary = installed("hfsprogs", "Priority: optional\n")
    result = invoke(approved, ordinary)
    assert marker.exists() and "--remove" in marker.read_text(), (
        result.stdout, marker.read_text() if marker.exists() else "no dpkg call")

    # Both names are catalogue-managed. The newly dependent second package
    # must still be refused because it was absent from the user's approval.
    changed = ordinary + installed("xfsprogs", "Depends: hfsprogs\n")
    result = invoke(approved, changed)
    assert result.returncode != 0 and not marker.exists(), result.stdout
    assert "plan changed after confirmation" in result.stdout, result.stdout

    result = invoke(["hfsprogs:amd64", "xfsprogs:amd64"], changed)
    assert marker.exists() and "--remove" in marker.read_text(), result.stdout

    result = invoke(approved, installed("hfsprogs", "Priority: required\n"))
    assert result.returncode != 0 and not marker.exists(), result.stdout
    assert "marked Essential, Protected, or required" in result.stdout, result.stdout

    result = invoke(["hfsprogs:i386"], ordinary)
    assert result.returncode != 0 and not marker.exists(), result.stdout

    result = invoke(["hfsprogs:all"], installed("hfsprogs", architecture="all"))
    assert marker.exists() and "--remove" in marker.read_text(), result.stdout

    # Bypassing APT does not make the helper a general-purpose approval oracle.
    environment.pop("DPKG_FRONTEND_LOCKED", None)
    result = subprocess.run([str(guard), *approved], env=environment,
                            input="VERSION 3\n\n", text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=10)
    assert result.returncode != 0 and "locked transaction" in result.stdout, result.stdout

    sources.write_text(f"deb [trusted=yes] file:{root / 'repo'} ./\n",
                       encoding="utf-8")
    control_dir = root / "package/DEBIAN"
    control_dir.mkdir(parents=True)
    commands = installation_commands(root)
    for conflict in (False, True):
        index = []
        for package in ("hfsprogs", "linux-headers-fixture", "filesystems"):
            control = (
                f"Package: {package}\nVersion: 1\nArchitecture: amd64\n"
                "Maintainer: Disposable Test <test@example.invalid>\n"
                "Description: disposable install transaction fixture\n")
            if conflict:
                control += "Conflicts: unrelated-desktop\n"
            (control_dir / "control").write_text(control, encoding="utf-8")
            filename = "filesystems.deb" if package == "filesystems" else f"{package}_1_amd64.deb"
            deb = root / "repo" / filename
            subprocess.run(["dpkg-deb", "--build", "--root-owner-group",
                            str(control_dir.parent), str(deb)], check=True,
                           stdout=subprocess.DEVNULL)
            data = deb.read_bytes()
            index.append(control + f"Filename: {deb.name}\nSize: {len(data)}\n"
                         f"SHA256: {hashlib.sha256(data).hexdigest()}\n\n")
        (root / "repo/Packages").write_text("".join(index), encoding="utf-8")
        # Each case must use its new metadata and bytes, even on a filesystem
        # whose timestamps cannot distinguish two fixture rewrites in a second.
        for directory in (root / "lists", root / "archives"):
            for entry in directory.iterdir():
                if entry.is_file():
                    entry.unlink()
        result = apt(["update"])
        assert result.returncode == 0, result.stdout

        for name, arguments in commands.items():
            database = installed("unrelated-desktop", "Priority: optional\n")
            status.write_text(database, encoding="utf-8")
            marker.unlink(missing_ok=True)
            result = apt(arguments[1:])
            if conflict:
                assert result.returncode != 0 and not marker.exists(), (
                    name, "installation reached dpkg despite unapproved removal",
                    result.stdout,
                    marker.read_text() if marker.exists() else "no dpkg call")
                assert "Packages need to be removed but remove is disabled" in result.stdout, (
                    name, result.stdout)
            else:
                assert marker.exists() and "--unpack" in marker.read_text(), (
                    name, result.stdout)
                assert "--remove" not in marker.read_text(), (name, marker.read_text())
            assert status.read_text(encoding="utf-8") == database

print("APT rejects changed removal approvals and installation-triggered removals; ordinary installs reach unpack.")
