#!/usr/bin/env python3
"""Run all target binaries and require bidirectional cross-platform wire exchange."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import verify
import wire_inputs
import build_evidence

HERE = Path(__file__).resolve().parent
EXPECTED_CORPUS = {"empty.xwire", "small.xwire", "medium_holes.xwire", "reloaded_mutated.xwire",
                   "sparse.xwire", "elided.xwire", "slg_player.xwire", "slg_battle.xwire",
                   "slg_shard.xwire", "slg_report.xwire"}

def exchange_ok(path, status):
    if not path.is_file():
        return False
    result = json.loads(path.read_text())
    return result.get("status") == status and set(result.get("corpus_files", [])) == EXPECTED_CORPUS

def invoke(cmd, timeout=180):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except (OSError, subprocess.TimeoutExpired) as exc:
        return None, "", str(exc)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profiles", help="JSON mapping target names to binary and runner argv")
    p.add_argument("--out", required=True, help="new result directory")
    p.add_argument("--step-timeout", type=int, default=180, help="seconds for each wire operation (1..600)")
    a = p.parse_args()
    if not 1 <= a.step_timeout <= 600: p.error("invalid step timeout")
    out=Path(a.out).resolve()
    out.mkdir(parents=True,exist_ok=False)
    profile = json.loads(Path(a.profiles).read_text()) if a.profiles else {}
    refname=profile.get("reference","macos-arm64")
    targets=profile.get("targets",{})
    if refname not in verify.TARGETS or not isinstance(targets,dict) or set(targets)-set(verify.TARGETS):
        p.error("invalid reference or unknown target")
    fingerprint, inputs=wire_inputs.fingerprint()
    record={"format":1,"scope":"generic cross_compiler.v2.root and four SLG benchmark roots; not production business schemas",
            "input_fingerprint":fingerprint,"inputs":inputs,"reference":refname,
            "profiles_sha256":verify.sha(a.profiles) if a.profiles else None,"targets":{}}
    statuses=record["targets"]
    def identify(name):
        item=targets.get(name)
        if item is None: return None,"BLOCKED_RUNTIME","No runtime binary configured"
        if not isinstance(item,dict): return None,"BLOCKED_CONFIG","Target configuration must be an object"
        kind=item.get("runtime_kind", "native" if not name.startswith("windows-") else None)
        valid=("windows", "wine") if name.startswith("windows-") else ("native",)
        if kind not in valid: return None,"BLOCKED_CONFIG","runtime_kind must be one of: " + ", ".join(valid)
        if name==refname and kind=="wine": return None,"BLOCKED_CONFIG","Wine cannot be the native reference"
        binary=item.get("binary"); runner=item.get("runner",[])
        if not isinstance(binary,str) or not isinstance(runner,list) or not all(isinstance(v,str) for v in runner):
            return None,"BLOCKED_CONFIG","Expected binary path and runner argv list"
        binary=Path(binary).resolve()
        if not binary.is_file(): return None,"BLOCKED_RUNTIME","Runtime binary does not exist"
        try:
            if not isinstance(item.get("build_manifest"),str) or not isinstance(item.get("expected_build_id"),str):
                raise ValueError("V2 build manifest and expected_build_id required; rebuild fixture")
            checker=item.get("build_input_verifier")
            if checker is not None and (not isinstance(checker,list) or not checker or not all(isinstance(v,str) for v in checker)):
                raise ValueError("build_input_verifier must be argv")
            manifest=build_evidence.validate(item["build_manifest"], binary, name, fingerprint, item["expected_build_id"],check_inputs=not checker)
            if checker:
                check_rc,check_out,check_err=invoke(checker,a.step_timeout)
                if check_rc!=0 or check_out.strip()!="XSG_BUILD_INPUTS_V1 "+manifest["build_id"]+" "+manifest["binary_sha256"]:
                    raise ValueError("live build input check failed: "+check_err[-300:])
        except (KeyError, OSError, ValueError) as exc:
            return None,"BLOCKED_BUILD",str(exc)
        cmd=[*runner,str(binary),"--xsg-identify"]
        rc, stdout, stderr=invoke(cmd, a.step_timeout)
        (out/(name+".identity.log")).write_text(stdout+stderr)
        environment = kind if name.startswith("windows-") else ("ios-simulator" if name.startswith("ios-simulator-") else name.rsplit("-",1)[0])
        expected=f"XSG_RUNTIME_V2 {name} 64 le {fingerprint} {environment} {manifest.get('compile_id',manifest['build_id'])}"
        if rc!=0: return None,"FAIL_RUNTIME","Identity process failed; see identity log"
        if stdout.strip()!=expected: return None,"FAIL_IDENTITY","Target identity or source fingerprint mismatch"
        return {"binary":str(binary),"runner":runner,"sha256":verify.sha(binary),"identity":stdout.strip(),"runtime_kind":kind,"build_id":manifest["build_id"]},None,None

    ref,code,detail=identify(refname)
    if ref:
        command=[sys.executable,str(HERE/"wire_check.py"),"--reference-binary",ref["binary"],
                 "--target-binary",ref["binary"],"--reference-runner",json.dumps(ref["runner"]),
                 "--target-runner",json.dumps(ref["runner"]),"--out",str(out/refname),"--step-timeout",str(a.step_timeout)]
        rc,stdout,stderr=invoke(command, 4*a.step_timeout+30)
        (out/(refname+".matrix.log")).write_text(stdout+stderr)
        result=out/refname/"result.json"
        ok=rc==0 and exchange_ok(result, "PASS_SELF_TEST")
        statuses[refname]={"status":"PASS_REFERENCE" if ok else "FAIL_WIRE","identity":ref,
                           "detail":"Reference write/read self-check" if ok else "Reference self-check failed"}
        if not ok: ref=None
    else:
        statuses[refname]={"status":code,"detail":detail}
    for name in verify.TARGETS:
        if name==refname: continue
        target,code,detail=identify(name)
        if not target:
            statuses[name]={"status":code,"detail":detail}
        elif not ref:
            statuses[name]={"status":"BLOCKED_REFERENCE","detail":"Reference runtime unavailable"}
        elif target["sha256"]==ref["sha256"]:
            statuses[name]={"status":"FAIL_IDENTITY","detail":"Reference and target binaries have identical hashes"}
        else:
            command=[sys.executable,str(HERE/"wire_check.py"),"--reference-binary",ref["binary"],
                     "--target-binary",target["binary"],"--reference-runner",json.dumps(ref["runner"]),
                     "--target-runner",json.dumps(target["runner"]),"--out",str(out/name),"--step-timeout",str(a.step_timeout)]
            rc,stdout,stderr=invoke(command, 4*a.step_timeout+30)
            (out/(name+".matrix.log")).write_text(stdout+stderr)
            result=out/name/"result.json"
            ok=rc==0 and exchange_ok(result, "PASS")
            compat=target["runtime_kind"]=="wine"
            status=("PASS_WIRE_WINE" if compat else "PASS_WIRE") if ok else "FAIL_WIRE"
            detail=("bidirectional read and byte comparison via Wine; native Windows unverified" if compat else "bidirectional read and byte comparison") if ok else "wire exchange failed; see logs"
            statuses[name]={"status":status,"identity":target,"detail":detail}
    record["targets"]={name:statuses[name] for name in verify.TARGETS}
    success=all(x["status"]==("PASS_REFERENCE" if name==refname else "PASS_WIRE")
                for name,x in record["targets"].items())
    record["status"]="PASS" if success else "FAIL"
    (out/"result.json").write_text(json.dumps(record,indent=2)+"\n")
    lines=["# Runtime and bidirectional wire matrix","","Scope: "+record["scope"],"",
           "| Target | Status | Detail |","| --- | --- | --- |"]
    for name,result in record["targets"].items():
        lines.append("| "+name+" | "+result["status"]+" | "+result["detail"].replace("|","/")+" |")
    lines += ["","Overall: "+record["status"],""]
    (out/"REPORT.md").write_text("\n".join(lines))
    print("Wire matrix:",record["status"],out/"REPORT.md",flush=True)
    for name,result in record["targets"].items(): print(name,result["status"],flush=True)
    return 0 if success else 2

if __name__=="__main__":
    sys.exit(main())
