#!/usr/bin/env python3
"""Build-time x64 package inventory, recipes, and strict release checksums."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGETS = json.loads((ROOT / "packaging/targets.json").read_text())


def checked_version(version):
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("expected a canonical X.Y.Z version")
    return version


def project_version():
    match = re.search(r"^\s*VERSION (\S+)", (ROOT / "CMakeLists.txt").read_text(), re.MULTILINE)
    return checked_version(match[1] if match else "")


def payloads(version):
    checked_version(version)
    names = [f"kodometer-{version}-source.tar.gz", f"kodometer-{version}-aur.tar.gz",
             f"kodometer-{version}-1-x86_64.pkg.tar.zst", f"kodometer-{version}-linux-x86_64.tar.gz"]
    for target in TARGETS:
        if target["kind"] == "deb":
            names.append(f"kodometer_{version}-1ubuntu{target['version']}_amd64.deb")
        elif target["kind"] == "rpm":
            names.append(f"kodometer-{version}-1.fc{target['version']}.x86_64.rpm")
    return sorted(names)


def manifest_name(version):
    return f"kodometer-{checked_version(version)}-SHA256SUMS"


def assets(version):
    names = payloads(version)
    return sorted(names + [name + ".sha256" for name in names] + [manifest_name(version)])


def digest(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"missing or non-regular artifact: {path.name}")
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def checksum_line(value, name):
    return f"{value}  {name}\n"


def write_manifest(directory, version):
    directory = Path(directory)
    unknown = {path.name for path in directory.iterdir()} - set(assets(version))
    if unknown:
        raise ValueError(f"unexpected release files: {sorted(unknown)}")
    # Validate every payload before writing any checksums.
    rows = [(name, digest(directory / name)) for name in payloads(version)]
    for name in assets(version):
        if (directory / name).is_symlink():
            raise ValueError(f"symlink in release output: {name}")
    for name, value in rows:
        (directory / (name + ".sha256")).write_text(checksum_line(value, name))
    (directory / manifest_name(version)).write_text("".join(checksum_line(value, name) for name, value in rows))


def read_manifest(directory, version):
    path = Path(directory) / manifest_name(version)
    digest(path)  # Reject symlinks and missing/non-regular manifests as well.
    result = {}
    for line in path.read_text().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9_.-]+)", line)
        if not match or match[2] in result:
            raise ValueError("invalid or duplicate release checksum row")
        result[match[2]] = match[1]
    if set(result) != set(payloads(version)):
        raise ValueError("release manifest does not match the required x64 inventory")
    return result


def verify_manifest(directory, version):
    directory = Path(directory)
    rows = read_manifest(directory, version)
    if {path.name for path in directory.iterdir()} != set(assets(version)):
        raise ValueError("download does not contain the exact release asset set")
    for name, value in rows.items():
        if digest(directory / name) != value:
            raise ValueError(f"release checksum mismatch: {name}")
        sidecar = directory / (name + ".sha256")
        digest(sidecar)
        if sidecar.read_text() != checksum_line(value, name):
            raise ValueError(f"invalid individual checksum: {name}")


def render_pkgbuild(version, checksum):
    checked_version(version)
    if not re.fullmatch(r"[0-9a-f]{64}", checksum):
        raise ValueError("expected a lowercase SHA-256 source digest")
    return (ROOT / "packaging/aur/PKGBUILD.in").read_text().replace("@VERSION@", version).replace("@SHA256@", checksum)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["version", "matrix", "assets", "manifest", "verify", "pkgbuild"])
    parser.add_argument("arguments", nargs="*")
    args = parser.parse_args()
    version = project_version()
    if args.command == "version":
        print(version)
    elif args.command == "matrix":
        print(json.dumps({"include": TARGETS}))
    elif args.command == "assets":
        print("\n".join(assets(version)))
    elif args.command == "manifest":
        write_manifest(args.arguments[0], version)
    elif args.command == "verify":
        verify_manifest(args.arguments[0], version)
    else:
        print(render_pkgbuild(version, digest(Path(args.arguments[0]))), end="")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, IndexError) as error:
        print(f"packaging: {error}", file=sys.stderr)
        sys.exit(1)
