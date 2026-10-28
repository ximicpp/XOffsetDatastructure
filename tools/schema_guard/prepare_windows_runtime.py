#!/usr/bin/env python3
"""Prepare checksum-pinned MinGW runtime libraries for Windows wire linking."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

PACKAGES = {
    "windows-x64": ("clang64", [
        ("crt-14.0.0.r532.gdbbc8e99d-1", "5b7daa1849faf8edc81e9e59602f88afb32c3540d6904e20645884ee39f435c5"),
        ("libc++-23.1.4-1", "88426f65bca8cfa96b0ec258ec6612b8a06ea1dc1c64511ac1e72388f4ae35f6"),
        ("libunwind-23.1.3-1", "6dc583bd260a65e36ac7fe3bcf6669848032ed5edf4308591a338a3c02c88241"),
        ("libwinpthread-14.0.0.r495.g63ebe6cdb-1", "7c9e978aeb767af1555667d99e232d1149d60e42d1b6e890f73b2d87101fa483"),
        ("compiler-rt-23.1.3-1", "3f0926a9b74eee208f0add40f05684ca1766bd44b64137de72c73eac165dc244")]),
    "windows-arm64": ("clangarm64", [
        ("crt-14.0.0.r495.g63ebe6cdb-1", "4cb96613bc90cedc7736022237f1359ee7a62789ba2998c25f8faaca4971426e"),
        ("libc++-23.1.3-1", "47c0c7892af756dcdf4f8a53ef5d1c823003c21c9f47a633d4973b0e1c1ff167"),
        ("libunwind-22.1.8-1", "539ab7e4dd324094e24616167f3c3ff0ec115c618604b6fc79de91ff915fbfb4"),
        ("libwinpthread-14.0.0.r353.g6df76fa52-2", "ac98347fd3335ae6d0269acbaef50d855b2abcb78c1e7a054ee6eafeb2e2d12d"),
        ("compiler-rt-23.1.3-1", "87ea55f51adfff96317b2ac7c711f4dce8e282676dadea43086f4c37c43deed6")])
}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", required=True, help="new output directory")
    p.add_argument("--zstd", default=shutil.which("zstd"), required=shutil.which("zstd") is None)
    p.add_argument("--archive-dir", action="append", default=[])
    p.add_argument("--offline", action="store_true", help="fail if an archive is not cached")
    a = p.parse_args()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    records = []
    for target, (repo, entries) in PACKAGES.items():
        arch = "x86_64" if target == "windows-x64" else "aarch64"
        for package, expected in entries:
            name = "mingw-w64-clang-" + arch + "-" + package + "-any.pkg.tar.zst"
            url = "https://mirror.msys2.org/mingw/" + repo + "/" + name.replace("+", "%2B")
            source = next((Path(d) / name for d in a.archive_dir if (Path(d) / name).is_file()), None)
            if source:
                data = source.read_bytes()
            elif a.offline:
                raise FileNotFoundError("archive not cached: " + name)
            else:
                with urllib.request.urlopen(url, timeout=120) as response:
                    data = response.read()
            actual = hashlib.sha256(data).hexdigest()
            if actual != expected:
                raise ValueError("SHA-256 mismatch: " + name)
            archive = out / name
            archive.write_bytes(data)
            proc = subprocess.Popen([a.zstd, "-dc", str(archive)], stdout=subprocess.PIPE)
            try:
                with tarfile.open(fileobj=proc.stdout, mode="r|") as tar:
                    for member in tar:
                        parts = Path(member.name).parts
                        if parts and parts[0] == repo and (member.isfile() or member.isdir()):
                            tar.extract(member, out, filter="data")
            finally:
                proc.stdout.close()
            if proc.wait():
                raise RuntimeError("decompression failed: " + name)
            records.append({"target": target, "url": url, "sha256": actual})
            print(target, name, "verified", flush=True)
    (out / "manifest.json").write_text(json.dumps({"packages": records}, indent=2) + "\n")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
