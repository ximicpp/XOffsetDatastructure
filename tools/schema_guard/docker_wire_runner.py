#!/usr/bin/env python3
"""Run a Linux wire fixture inside an explicitly selected Docker image."""
import argparse
import re
from pathlib import Path
import subprocess
import sys

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--docker",default="docker")
    p.add_argument("--image",required=True,help="immutable digest-pinned image")
    p.add_argument("--platform",required=True,choices=["linux/amd64","linux/arm64"])
    p.add_argument("binary")
    p.add_argument("operation",choices=["--xsg-identify","--write","--read"])
    p.add_argument("directory",nargs="?")
    a=p.parse_args()
    if not re.fullmatch(r".+@sha256:[0-9a-f]{64}", a.image): p.error("image must be digest-pinned")
    binary=Path(a.binary).resolve()
    if not binary.is_file(): p.error("binary missing")
    if a.operation!="--xsg-identify" and not a.directory: p.error("wire directory required")
    cmd=[a.docker,"run","--rm","--network=none","--pull=never","--platform",a.platform,
         "-v",str(binary)+":/xsg/fixture:ro"]
    if a.directory:
        path=Path(a.directory).resolve()
        if not path.parent.is_dir(): p.error("wire parent directory missing")
        cmd+=["-v",str(path.parent)+":/xsg/data"]
    cmd += [a.image,"/xsg/fixture",a.operation]
    if a.directory: cmd+=["/xsg/data/"+path.name]
    try: return subprocess.run(cmd).returncode
    except OSError as exc:
        print("Docker wire runner:",exc,file=sys.stderr)
        return 1

if __name__=="__main__":
    sys.exit(main())
