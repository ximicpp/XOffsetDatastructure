#!/usr/bin/env python3
"""Run a Windows ARM64 wire fixture with Wine in an isolated Linux container."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--docker", default="docker")
    p.add_argument("--image", required=True, help="immutable local image id sha256:...")
    p.add_argument("--timeout", type=int, default=90)
    p.add_argument("binary")
    p.add_argument("operation", choices=["--xsg-identify", "--write", "--read"])
    p.add_argument("directory", nargs="?")
    a = p.parse_args()
    if a.timeout <= 0 or a.timeout > 150:
        p.error("timeout must be between 1 and 150 seconds")
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", a.image):
        p.error("image must be an immutable local image ID")
    binary = Path(a.binary).resolve()
    if not binary.is_file():
        p.error("fixture missing")
    if a.operation != "--xsg-identify" and not a.directory:
        p.error("wire directory required")
    command = [a.docker, "run", "--rm", "--network=none", "--pull=never",
               "--platform", "linux/arm64", "-e", "WINEDEBUG=-all",
               "-v", str(binary) + ":/xsg/fixture.exe:ro"]
    if a.directory:
        path = Path(a.directory).resolve()
        if not path.parent.is_dir():
            p.error("wire directory parent missing")
        command += ["-v", str(path.parent) + ":/xsg/data"]
    command += [a.image, "timeout", str(a.timeout) + "s", "/usr/lib/wine/wine64", "/xsg/fixture.exe", a.operation]
    if a.directory:
        command += ["Z:\\xsg\\data\\" + path.name]
    try:
        return subprocess.run(command).returncode
    except OSError as exc:
        print("Wine wire runner:", exc, file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
