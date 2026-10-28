#!/usr/bin/env python3
"""Run a wire fixture in an Android device or emulator via adb."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import uuid

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--adb",default="adb")
    p.add_argument("--serial",required=True,help="explicit Android device/emulator serial")
    p.add_argument("binary")
    p.add_argument("operation",choices=["--xsg-identify","--write","--read"])
    p.add_argument("directory",nargs="?")
    # operation starts with --, so parse the runner prefix separately.
    args,rest=p.parse_known_args()
    # The caller passes a double dash before the binary in its runner profile.
    if rest: p.error("unexpected arguments: "+repr(rest))
    if args.operation!="--xsg-identify" and not args.directory: p.error("wire directory required")
    binary=Path(args.binary).resolve()
    if not binary.is_file(): p.error("binary missing")
    dest="/data/local/tmp/xsg-"+uuid.uuid4().hex
    adb=[args.adb,"-s",args.serial]
    def call(argv):
        return subprocess.run(adb+argv,capture_output=True,text=True,errors="replace",timeout=180)
    def require(argv):
        q=call(argv)
        if q.returncode: raise RuntimeError("adb command failed: "+q.stderr+q.stdout)
        return q
    try:
        require(["shell","mkdir","-p",dest+"/wire"])
        require(["push",str(binary),dest+"/fixture"])
        require(["shell","chmod","700",dest+"/fixture"])
        if args.operation=="--read":
            path=Path(args.directory).resolve()
            if not path.is_dir(): raise RuntimeError("wire input directory missing")
            require(["push",str(path)+"/.",dest+"/wire/"])
        cmd=shlex.quote(dest+"/fixture")+" "+shlex.quote(args.operation)
        if args.operation!="--xsg-identify": cmd+=" "+shlex.quote(dest+"/wire")
        q=require(["shell",cmd])
        if args.operation=="--write":
            path=Path(args.directory).resolve()
            path.mkdir(parents=True,exist_ok=False)
            require(["pull",dest+"/wire/.",str(path)])
        sys.stdout.write(q.stdout); sys.stderr.write(q.stderr)
        return 0
    except (OSError,subprocess.TimeoutExpired,RuntimeError) as exc:
        print("Android wire runner:",exc,file=sys.stderr)
        return 1
    finally:
        try:
            call(["shell","rm","-rf",dest])
        except (OSError,subprocess.TimeoutExpired):
            pass

if __name__=="__main__":
    sys.exit(main())
