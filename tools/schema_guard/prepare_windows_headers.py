#!/usr/bin/env python3
"""Prepare pinned MSYS2 Windows target C headers and cross-compile profiles."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

VERSION = "14.0.0.r569.g34eeafb7d-1-any.pkg.tar.zst"
PACKAGES = (
    ("clang64", "x86_64", "headers", "7e6684e1b6dc0c6476503dd1c890bd14e8a4f7eb1262e87a073e9e6a03d89b07"),
    ("clangarm64", "aarch64", "headers", "64d74aea9e4b7c15d0478f4b905875a82a382c73daca2ab186bf57668576d3f0"),
    ("clang64", "x86_64", "winpthreads", "acf7ddfa35c0111ee7aa53b23187081f4478eb25c69afd6b67ff939616a23f5c"),
    ("clangarm64", "aarch64", "winpthreads", "9d3f4371feb5e608abce908292877293d0c371d339a564d4e7a92b6706c4daa3"),
)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, help="new directory")
    parser.add_argument("--clang", required=True, help="P2996 compiler or wrapper")
    parser.add_argument("--base-profiles", help="optional existing profiles to extend")
    parser.add_argument("--archive-dir", action="append", default=[], help="look for pinned archives here before downloading")
    parser.add_argument("--zstd", default=shutil.which("zstd"), help="zstd executable")
    args = parser.parse_args()
    if not args.zstd:
        parser.error("zstd is required")
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    profiles = json.loads(Path(args.base_profiles).read_text()) if args.base_profiles else {}
    manifest = []
    include_paths = {}
    for repo, arch, kind, expected in PACKAGES:
        name = f"mingw-w64-clang-{arch}-{kind}-{VERSION}"
        url = f"https://mirror.msys2.org/mingw/{repo}/{name}"
        source = next((Path(d) / name for d in args.archive_dir if (Path(d) / name).is_file()), None)
        if source is None:
            print("Downloading", url, flush=True)
            with urllib.request.urlopen(url, timeout=120) as response:
                data = response.read()
        else:
            data = source.read_bytes()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected:
            raise ValueError(f"SHA-256 mismatch for {name}: {actual}")
        archive = out / name
        archive.write_bytes(data)
        dest = out / kind / repo
        proc = subprocess.Popen([args.zstd, "-dc", str(archive)], stdout=subprocess.PIPE)
        try:
            with tarfile.open(fileobj=proc.stdout, mode="r|") as tar:
                for member in tar:
                    parts = Path(member.name).parts
                    if len(parts) > 2 and parts[:2] == (repo, "include") and (member.isfile() or member.isdir()):
                        tar.extract(member, out / kind, filter="data")
        finally:
            if proc.stdout:
                proc.stdout.close()
        if proc.wait() != 0:
            raise RuntimeError("zstd failed for " + name)
        include_paths[(repo, kind)] = str(dest / "include")
        manifest.append({"url": url, "sha256": actual})
    for target, repo, triple in (
        ("windows-x64", "clang64", "x86_64-w64-windows-gnu"),
        ("windows-arm64", "clangarm64", "aarch64-w64-windows-gnu"),
    ):
        profiles[target] = {"kind": "host", "compiler": str(Path(args.clang).resolve()),
                            "flags": ["-std=c++26", "-freflection", "-fexpansion-statements",
                                      "-stdlib=libc++", "--target=" + triple,
                                      "-isystem", include_paths[(repo, "headers")],
                                      "-isystem", include_paths[(repo, "winpthreads")]]}
    (out / "profiles.json").write_text(json.dumps(profiles, indent=2) + "\n")
    (out / "manifest.json").write_text(json.dumps({"packages": manifest,
        "scope": "MinGW target C headers only; compiler must provide P2996 C++ headers"}, indent=2) + "\n")
    print("Profiles:", out / "profiles.json")

if __name__ == "__main__":
    main()
