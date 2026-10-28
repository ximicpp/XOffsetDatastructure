#!/usr/bin/env python3
"""Build Linux wire fixtures inside pinned GCC 16 Docker images."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import verify
import wire_inputs

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", required=True, help="new artifact directory")
    p.add_argument("--docker", default="docker")
    p.add_argument("--pull", action="store_true", help="allow pulling missing pinned images")
    p.add_argument("--registry-prefix", default="", help="optional registry mirror prefix, e.g. mirror.gcr.io/library/")
    p.add_argument("--base-profiles", help="optional runtime profiles to extend")
    p.add_argument("--target", action="append", choices=["linux-x64","linux-arm64"], help="limit builds; runtime gate still requires every target")
    a = p.parse_args()
    out=Path(a.out).resolve()
    out.mkdir(parents=True,exist_ok=False)
    fingerprint,inputs=wire_inputs.fingerprint()
    data=json.loads(Path(a.base_profiles).read_text()) if a.base_profiles else {"reference":"macos-arm64","targets":{}}
    targets=data.setdefault("targets",{})
    results={}
    for target,platform in (("linux-x64","linux/amd64"),("linux-arm64","linux/arm64")):
        if a.target and target not in a.target:
            continue
        image=a.registry_prefix+verify.IMAGES[target]
        cmd=[a.docker,"run","--rm","--network=none","--pull="+("missing" if a.pull else "never"),
             "--platform",platform,"-v",str(ROOT)+":/workspace:ro","-v",str(out)+":/xsg-out",
             "-w","/workspace",image,"g++","-std=c++26","-freflection","-O1","-pthread",
             '-DXSG_WIRE_INPUT_HASH="'+fingerprint+'"']
        for inc in verify.includes(Path("/workspace")):
            cmd+=["-I",str(inc)]
        cmd += ["/workspace/tools/schema_guard/wire_runtime.cpp","-o","/xsg-out/"+target]
        try:
            result=subprocess.run(cmd,capture_output=True,text=True,errors="replace",timeout=1200)
            rc,log=result.returncode,result.stdout+result.stderr
        except (OSError,subprocess.TimeoutExpired) as exc:
            rc,log=None,str(exc)
        (out/(target+".log")).write_text(log)
        binary=out/target
        passed=rc==0 and binary.is_file()
        results[target]={"status":"PASS_BUILD" if passed else "FAIL_BUILD","exit_code":rc,"command":cmd}
        if passed:
            targets[target]={"binary":str(binary),"runner":[sys.executable,str(HERE/"docker_wire_runner.py"),
                               "--docker",a.docker,"--image",image,"--platform",platform,"--"]}
        else:
            targets.pop(target,None)
        print(target,results[target]["status"],flush=True)
    (out/"profiles.json").write_text(json.dumps(data,indent=2)+"\n")
    (out/"inputs.json").write_text(json.dumps({"fingerprint":fingerprint,"inputs":inputs},indent=2)+"\n")
    (out/"build-result.json").write_text(json.dumps(results,indent=2)+"\n")
    return 0 if all(x["status"]=="PASS_BUILD" for x in results.values()) else 2

if __name__=="__main__":
    sys.exit(main())
