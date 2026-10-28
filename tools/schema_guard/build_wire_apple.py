#!/usr/bin/env python3
"""Build macOS and iOS simulator wire fixtures on macOS."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import subprocess
import wire_inputs
import verify

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--clang", required=True)
    p.add_argument("--libcxx-include", required=True)
    p.add_argument("--resource-dir", required=True)
    p.add_argument("--simulator", required=True, help="booted simulator UDID")
    p.add_argument("--include-simulator-x64", action="store_true", help="also build an x64 iOS simulator fixture; running it requires a compatible simulator runtime")
    p.add_argument("--out", required=True, help="new artifact directory")
    a = p.parse_args()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    fingerprint, inputs = wire_inputs.fingerprint()
    sdk = lambda name: subprocess.check_output(["xcrun","--sdk",name,"--show-sdk-path"],text=True).strip()
    mac, ios = sdk("macosx"), sdk("iphonesimulator")
    common = [a.clang, "-std=c++26", "-freflection", "-fexpansion-statements", "-nostdinc++",
              "-isystem", a.libcxx_include, "-resource-dir", a.resource_dir, "-O1", "-stdlib=libc++",
              '-DXSG_WIRE_INPUT_HASH="' + fingerprint + '"']
    for path in verify.includes(ROOT):
        common += ["-I", str(path)]
    items = [("macos-arm64", "arm64-apple-macos11", mac),
             ("macos-x64", "x86_64-apple-macos11", mac),
             ("ios-simulator-arm64", "arm64-apple-ios15.0-simulator", ios)]
    if a.include_simulator_x64:
        items.append(("ios-simulator-x64", "x86_64-apple-ios15.0-simulator", ios))
    def build(item):
        name,triple,sysroot=item
        binary=out/name
        src=str(ROOT/"tools/schema_guard/wire_runtime.cpp")
        args=common+["--target="+triple, "-isysroot", sysroot, src]
        # The existing P2996 build has only the AArch64 code generator.
        # Emit target-specific IR, then use Xcode's x86_64 backend to link.
        stages=[args+["-S","-emit-llvm","-o",str(out/(name+".ll"))],
                ["xcrun","clang++","-target",triple,"-isysroot",sysroot,"-O1",str(out/(name+".ll")),"-stdlib=libc++","-o",str(binary)]] if name in ("macos-x64", "ios-simulator-x64") else [args+["-o",str(binary)]]
        log=[]
        for cmd in stages:
            q=subprocess.run(cmd,capture_output=True,text=True)
            log.append({"command":cmd,"exit_code":q.returncode,"stdout":q.stdout,"stderr":q.stderr})
            if q.returncode: break
        (out/(name+".build.json")).write_text(json.dumps(log,indent=2)+"\n")
        print(name,"PASS_BUILD" if binary.is_file() and log[-1]["exit_code"]==0 else "FAIL_BUILD",flush=True)
        return name,binary.is_file() and log[-1]["exit_code"]==0
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        built=dict(pool.map(build,items))
    targets={}
    for name,_,_ in items:
        if built[name]:
            runner=["xcrun","simctl","spawn",a.simulator] if name.startswith("ios-simulator-") else []
            targets[name]={"binary":str(out/name),"runner":runner}
    (out/"profiles.json").write_text(json.dumps({"reference":"macos-arm64","targets":targets},indent=2)+"\n")
    (out/"inputs.json").write_text(json.dumps({"fingerprint":fingerprint,"inputs":inputs},indent=2)+"\n")
    return 0 if all(built.values()) else 2

if __name__ == "__main__":
    raise SystemExit(main())
