#!/usr/bin/env python3
"""Prepare/functional/freeze/run/audit/report for the first five-library cell.

Only run under the existing POSIX global run_job lease for native work. Merely
importing this module, prepare, audit and report never execute native programs.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import importlib.util
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import platform
import shutil
import stat
import subprocess
import sys
import time

try:
    from . import core as C, protocol as P
except ImportError:
    import core as C
    import protocol as P

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
# This candidate fork retains the original project's single WSL global lease.
# A copied worktree must never create an independent lock namespace.
RUNNER = Path("/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py")
SIZES = {"S":128, "M":16384, "L":131072}
FILES = ("input.canonical.bin", "trace.bin", "final.canonical.bin", "query-before.bin", "query-after.bin")
PHASES = ("load", "query_before", "mutate", "emit_after", "retire_modified_state", "reload", "query_after", "cleanup")
NATIVE_KEYS = {"schema_version", "kind", "library", "status", "phase_ns", "total_ns", "input_sha256", "trace_sha256",
               "final_wire_sha256", "final_canonical_sha256", "queries_sha256", "query_before_sha256", "query_after_sha256",
               "peak_rss_before_audit_kib", "peak_rss_after_audit_kib", "cpu_ns", "minor_faults", "major_faults", "record_count"}


def tools_identity():
    return {p.name:C.file_id(p) for p in (HERE/"core.py", HERE/"protocol.py", HERE/"campaign.py", RUNNER)}


def postrun_pin(directory):
    """Bind both manifests after the maintained queue has finished writing."""
    directory=Path(directory)
    raw=b"SC-POST-RUN-PIN-v1\x00"+bytes.fromhex(C.sha(directory/"manifest.json"))+bytes.fromhex(C.sha(directory/"queue/manifest.json"))
    import hashlib
    return hashlib.sha256(raw).hexdigest()


def save_tools(out):
    for path in (HERE/"core.py", HERE/"protocol.py", HERE/"campaign.py", RUNNER):
        C.copy_file(path, Path(out)/"protocol"/path.name)


def read_profile(path):
    value = C.exact(C.read(path), ("schema_version", "platform_class", "lock_dir", "environment", "binaries", "closure"), "profile")
    C.require(type(value["schema_version"]) is int and value["schema_version"] == 1, "unsupported profile schema")
    C.require(value["platform_class"] == "wsl2-vm-x86_64" and C.absolute_string(value["lock_dir"]), "wrong platform/lock path")
    C.require(type(value["environment"]) is dict and value["environment"] and all(type(k) is str and type(v) is str and "\x00" not in k+v for k,v in value["environment"].items()), "invalid explicit environment")
    C.require(type(value["binaries"]) is dict and set(value["binaries"]) == set(P.LIBRARIES), "five binary profiles required")
    for lib, entry in value["binaries"].items():
        keys = ("path", "prefix", "library", "commit", "trust_mode", "load_contract")
        C.exact(entry, (*keys,"source_content_sha256") if lib == "xoffset" else keys, "binary profile")
        C.require(C.absolute_string(entry["path"]) and type(entry["prefix"]) is list and all(type(v) is str and v for v in entry["prefix"]), "invalid binary command")
        C.require(not entry["prefix"] or C.absolute_string(entry["prefix"][0]), "launcher must be absolute")
        C.require((entry["library"],entry["commit"]) == P.IDENTITIES[lib], "wrong fixed release identity")
        if lib == "xoffset":
            C.require(entry["source_content_sha256"] == P.CANDIDATE_CONTENT_SHA256,"wrong fixed candidate content identity")
        C.require(type(entry["trust_mode"]) is str and entry["trust_mode"] and entry["load_contract"] == "detached", "wrong trust/ownership mode")
    C.require(type(value["closure"]) is dict and value["closure"] and all(type(k) is str and k and C.absolute_string(v) for k,v in value["closure"].items()), "explicit toolchain/source/runtime closure required")
    C.require(set(P.CANDIDATE_CLOSURE_IDS) <= set(value["closure"]),"candidate identity/header closure labels required")
    return value


def candidate_runtime_identity(runtime, allow_identity_error=False):
    """Check captured metadata only; never open original paths during audit."""
    if allow_identity_error and type(runtime) is dict and set(runtime) == {"identity_error"}:
        C.require(type(runtime["identity_error"]) is str and runtime["identity_error"],"invalid runtime identity error")
        return
    C.require(type(runtime) is dict and type(runtime.get("closure")) is dict,"candidate runtime closure missing")
    for label,identity in P.CANDIDATE_CLOSURE_IDS.items():
        C.require(C.same(runtime["closure"].get(label),{".":identity}),"captured fixed candidate identity differs: "+label)


def runtime_identity(profile):
    # Fail before scanning the larger closure if a critical candidate file changed.
    for label,identity in P.CANDIDATE_CLOSURE_IDS.items():
        C.require(C.file_id(profile["closure"][label]) == identity,"actual fixed candidate file differs: "+label)
    result = dict(binaries={k:C.file_id(v["path"]) for k,v in profile["binaries"].items()},
                closure={k:C.tree_id(v) for k,v in profile["closure"].items()},
                tools=tools_identity(), interpreter=dict(path=str(Path(sys.executable).resolve()),identity=C.file_id(Path(sys.executable).resolve())),
                environment=profile["environment"], platform_class=profile["platform_class"])
    candidate_runtime_identity(result)
    return result


def import_fixture(path):
    spec = importlib.util.spec_from_file_location("serialization_comparison_reference", path)
    C.require(spec is not None and spec.loader is not None, "cannot load fixture module")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    # No bytecode/cache mutation beside an immutable copied fixture source.
    exec(compile(Path(path).read_bytes(),str(path),"exec"),module.__dict__)
    C.require(callable(getattr(module, "generate_fixture", None)), "missing Python generate_fixture API")
    return module


def query_pair(before, after):
    return b"SCQPAIR1" + (1).to_bytes(4,"little") + len(before).to_bytes(8,"little") + before + len(after).to_bytes(8,"little") + after


def validate_stages(path, entry, fixture):
    row = C.exact(C.read(path),("schema_version","kind","library","status","stages"),"stage validation")
    expected = C.read(Path(fixture)/"fixture.json")["stages"]
    C.require(type(row["schema_version"]) is int and row["schema_version"] == 1
              and row["kind"] == "serialization-comparison-validation" and row["library"] == entry["library"]
              and row["status"] == "passed" and type(row["stages"]) is list and len(row["stages"]) == 9
              and all(C.hexsha(s) for s in row["stages"])
              and row["stages"] == [s["canonical_sha256"] for s in expected],"stage validation differs from independent oracle")
    return row


def fixture_check(directory, n):
    directory = Path(directory)
    manifest = C.exact(C.read(directory/"fixture.json"),("schema","model_version","n","seed","trace_name","query_name","input_digest","final_digest","query_before_digest","query_after_digest","query_count","mutation_count","stages","files"),"fixture manifest")
    C.require(manifest.get("schema") == "sc-fixture-v1" and type(manifest.get("n")) is int and manifest["n"] == n
              and type(manifest.get("model_version")) is int and manifest.get("model_version") == 1
              and manifest.get("seed") == "xoffset-competitor-v1" and manifest.get("trace_name") == "mixed-growth-v1"
              and manifest.get("query_name") == "query-v1", "fixture contract differs")
    C.require(type(manifest.get("files")) is dict, "missing fixture inventory")
    for name in FILES:
        identity = manifest["files"].get(name)
        C.require(type(identity) is dict and identity.get("sha256") == C.sha(directory/name)
                  and str(identity.get("bytes")) == str((directory/name).stat().st_size), "fixture byte identity differs: "+name)
    for key,name in (("input_digest","input.canonical.bin"),("final_digest","final.canonical.bin"),("query_before_digest","query-before.bin"),("query_after_digest","query-after.bin")):
        C.require(manifest[key] == C.sha(directory/name),"fixture declared digest differs")
    C.require(C.integer(manifest["query_count"]) and C.integer(manifest["mutation_count"])
              and type(manifest["stages"]) is list and len(manifest["stages"]) == 9,"fixture counts/stages invalid")
    for i,stage in enumerate(manifest["stages"]):
        C.exact(stage,("stage","mutation_count","canonical_sha256"),"fixture stage")
        C.require(type(stage["stage"]) is int and stage["stage"] == i+1 and C.integer(stage["mutation_count"])
                  and C.hexsha(stage["canonical_sha256"]),"fixture stage schema differs")
    expected = query_pair((directory/"query-before.bin").read_bytes(), (directory/"query-after.bin").read_bytes())
    C.require((directory/"queries.bin").read_bytes() == expected, "query pair differs")
    return manifest


def prepare(module_path, out):
    module_path = C.absolute(module_path)
    module_id = C.file_id(module_path)
    out = C.fresh(out)
    C.copy_file(module_path, out/"reference.py")
    module = import_fixture(out/"reference.py")
    for size,n in SIZES.items():
        module.generate_fixture(out/size, n, "xoffset-competitor-v1")
        (out/size/"queries.bin").write_bytes(query_pair((out/size/"query-before.bin").read_bytes(), (out/size/"query-after.bin").read_bytes()))
        fixture_check(out/size,n)
    C.require(C.file_id(module_path) == module_id, "fixture source changed")
    C.seal(out, dict(schema_version=1, kind="sc-prepared", status="complete", fixture_source=module_id,
                     original_fixture_source=str(module_path), record_counts=SIZES, performance_accepted=False))
    return audit(out)


def copy_bundle(source, target):
    C.check_inventory(source)
    C.require(not Path(target).exists(), "nested bundle target exists")
    members = C.inventory(source, exclude=(), exclude_roots=("queue",))
    executables = set()
    if C.read(Path(source)/"status.json").get("kind") == "sc-frozen":
        executables = {"bin/"+lib for lib in P.LIBRARIES}
        C.require({name for name in members if name.split("/")[0] == "bin"} == executables,
                  "frozen executable members differ from the five registered binaries")
    for name in members:
        copied = Path(target)/name
        C.copy_file(Path(source)/name, copied)
        if os.name == "posix" and name in executables:
            # The byte-copy helper deliberately creates ordinary files. Restore
            # only the five frozen adapters' required mode, never other members.
            # Do not follow a destination replaced with a symlink after copying.
            fd = os.open(copied,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
            try:
                C.require(stat.S_ISREG(os.fstat(fd).st_mode),"copied frozen binary is not a regular file")
                os.fchmod(fd,0o755)
                restored,current = os.fstat(fd),copied.lstat()
                C.require(stat.S_ISREG(current.st_mode)
                          and (restored.st_dev,restored.st_ino) == (current.st_dev,current.st_ino)
                          and stat.S_IMODE(restored.st_mode) == 0o755
                          and os.access(copied,os.X_OK),"copied frozen binary is not executable")
            finally:
                os.close(fd)


def active_queue(queue, profile, action, inputs):
    C.require(platform.system() == "Linux" and "microsoft" in platform.release().lower(), "native execution requires the registered WSL2 profile")
    import fcntl
    queue = C.absolute(queue)
    request = C.read(queue/"request.json")
    expected = [str(Path(__file__).resolve()), *sys.argv[1:]]
    command = request.get("command")
    interpreters=(str(Path(sys.executable).absolute()),str(Path(sys.executable).resolve()))
    C.require(type(command) is list and len(command)>=3 and command[0] in interpreters
              and command[1:] in (expected,["-B",*expected]), "queue must invoke exact absolute interpreter/adapter/argv directly")
    C.require(request.get("owner") == "W3" and request.get("kind") == ("timing" if action == "run" else "functional")
              and request.get("runner_sha256") == C.sha(RUNNER) and request.get("lock_dir") == profile["lock_dir"]
              and request.get("out") == str(queue) and request.get("cwd") == str(Path.cwd()), "queue request differs")
    fd = os.open(Path(profile["lock_dir"])/"queue.lock", os.O_RDONLY)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(fd, fcntl.LOCK_UN)
            raise C.ProtocolError("global lease is not held")
        record = C.parse(os.read(fd,65536))
    finally:
        os.close(fd)
    C.require(record.get("status") == "running" and record.get("supervisor_pid") == os.getppid()
              and record.get("run_id") == request["run_id"] and record.get("owner") == "W3", "not the direct leased coordinator")
    C.require(profile["lock_dir"] == str(RUNNER.parents[2]/"build/vm-coordination"), "first campaign requires the shared global lock")
    return dict(schema_version=1, action=action, original_output=str(queue.parent), request=request,
                queue_request_sha256=C.sha(queue/"request.json"),
                interpreter=dict(path=str(Path(sys.executable).resolve()), identity=C.file_id(Path(sys.executable).resolve())),
                requested_interpreter=command[0],
                adapter_path=str(Path(__file__).resolve()), tools=tools_identity(),
                inputs={flag:dict(original_path=str(C.absolute(path)), identity=C.file_id(path)) for flag,path in inputs.items()})


def begin_native(out, queue, profile, action, inputs):
    out, queue = C.absolute(out), C.absolute(queue)
    C.require(queue == out/"queue" and out.is_dir() and {p.name for p in out.iterdir()} == {"queue"}, "run output must only contain its active queue")
    context = active_queue(queue, profile, action, inputs)
    C.write(out/"execution-context.json", context)
    (out/"commands").mkdir()
    save_tools(out)
    return out,context


def command(entry, verb, *args, binary=None):
    return [*entry["prefix"], str(binary or entry["path"]), verb, *map(str,args)]


def execute(argv, prefix, timeout, environment, file_limit=512*1024**2):
    prefix = Path(prefix)
    C.require(type(timeout) in (int,float) and 0 < timeout <= 60, "invalid child timeout")
    receipt = dict(schema_version=1, argv=list(argv), execution_environment=environment,
                   timeout_seconds=timeout, started_monotonic_ns=str(time.monotonic_ns()),
                   finished_monotonic_ns=None, returncode=None, timed_out=False, pid=None, launch_error=None,file_limit_bytes=str(file_limit))
    with prefix.with_suffix(".stdout").open("xb") as stdout, prefix.with_suffix(".stderr").open("xb") as stderr:
        process = None
        try:
            options = {}
            if os.name == "posix":
                import resource
                def bound_files():
                    resource.setrlimit(resource.RLIMIT_FSIZE,(file_limit,file_limit))
                    resource.setrlimit(resource.RLIMIT_AS,(16*1024**3,16*1024**3))
                options["preexec_fn"] = bound_files
            process = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr, env=environment,**options)
            receipt["pid"] = process.pid
            try:
                process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                receipt["timed_out"] = True
                process.kill()
                process.wait()
        except OSError as error:
            receipt["launch_error"] = type(error).__name__+": "+str(error)
        finally:
            if process is not None and process.poll() is None:
                process.kill()
                process.wait()
            receipt["returncode"] = process.returncode if process is not None else None
            receipt["finished_monotonic_ns"] = str(time.monotonic_ns())
            if int(receipt["finished_monotonic_ns"])-int(receipt["started_monotonic_ns"])>timeout*10**9:
                # The cap covers observed launch/wait bookkeeping, not only the
                # duration passed to Popen.wait. An exit-0 child can still be late.
                receipt["timed_out"] = True
            C.write(prefix.with_suffix(".receipt.json"), receipt)
    C.require(not receipt["timed_out"] and receipt["returncode"] == 0, "native child failed/timed out")
    C.require(prefix.with_suffix(".stderr").stat().st_size == 0, "native stderr is not empty")
    return receipt


def launch_preflight(prefix, deadline, maximum_seconds, retained_wire=0,
                     expected_wire=0, file_limit=512*1024**2, wire_budget=None):
    """All deterministic no-launch decisions precede the attempted ledger."""
    remaining=deadline-time.monotonic()
    C.require(type(remaining) in (int,float) and 0<remaining,"child deadline exhausted before launch")
    C.require(C.integer(maximum_seconds,1,60) and C.integer(file_limit,1),"invalid child limits before launch")
    if wire_budget is not None:
        C.require(C.integer(retained_wire) and C.integer(expected_wire) and C.integer(wire_budget,1),"invalid wire budget before launch")
        C.require(retained_wire+expected_wire<=wire_budget,"wire budget would be exceeded before launch")
        C.require(retained_wire+file_limit<=wire_budget,"bounded child file ceiling exceeds remaining wire budget")
    for ext in (".stdout",".stderr",".receipt.json"):
        C.require(not Path(prefix).with_suffix(ext).exists(),"child evidence slot already exists")
    return min(maximum_seconds,remaining)


def functional_jobs():
    rows = []
    for lib in P.LIBRARIES:
        rows.append(dict(id=lib+"-describe",library=lib,size=None,action="describe"))
        for size in SIZES:
            for action in ("encode","inspect-input","validate","run","inspect-final"):
                rows.append(dict(id=lib+"-"+size+"-"+action,library=lib,size=size,action=action))
    return rows


def functional_argv(job,profile,original):
    entry = profile["binaries"][job["library"]]
    action = job["action"]
    if action == "describe":
        return command(entry,"describe")
    prefix = original/"commands"/(job["library"]+"-"+job["size"])
    fixture = original/"prepared"/job["size"]
    wire,final,canon,queries,result = (prefix.with_suffix(ext) for ext in (".input.wire",".wire",".canonical",".queries",".native.json"))
    if action == "encode":
        return command(entry,"encode","--canonical",fixture/"input.canonical.bin","--wire",wire)
    if action == "inspect-input":
        return command(entry,"inspect","--wire",wire,"--canonical",prefix.with_suffix(".initial.canonical"))
    if action == "validate":
        return command(entry,"validate","--wire",wire,"--trace",fixture/"trace.bin","--result",prefix.with_suffix(".validation.json"))
    if action == "inspect-final":
        return command(entry,"inspect","--wire",final,"--canonical",prefix.with_suffix(".reloaded.canonical"))
    return command(entry,"run","--wire",wire,"--trace",fixture/"trace.bin","--wire-out",final,"--canonical-out",canon,"--queries-out",queries,"--result",result)


def functional_output(out,job,profile,receipt):
    entry = profile["binaries"][job["library"]]
    if job["action"] == "describe":
        keys = ("schema_version","library","commit","trust_mode","load_contract","capabilities")
        row = C.exact(C.read(Path(out)/"commands"/(job["id"]+".stdout")),(*keys,"source_content_sha256") if job["library"] == "xoffset" else keys,"description")
        C.require(type(row["schema_version"]) is int and row["schema_version"] == 1 and type(row["capabilities"]) is dict
                  and all(row[k] == entry[k] for k in ("library","commit","trust_mode","load_contract")),"description/profile differs")
        if job["library"] == "xoffset":
            C.require(row["source_content_sha256"] == entry["source_content_sha256"] == P.CANDIDATE_CONTENT_SHA256,"candidate description/content identity differs")
        return row
    prefix = Path(out)/"commands"/(job["library"]+"-"+job["size"])
    fixture = Path(out)/"prepared"/job["size"]
    action = job["action"]
    if action == "encode":
        C.require(prefix.with_suffix(".input.wire").is_file(),"encoder output missing")
    elif action == "inspect-input":
        C.require(C.file_id(prefix.with_suffix(".initial.canonical")) == C.file_id(fixture/"input.canonical.bin"),"initial inspect differs")
    elif action == "validate":
        validate_stages(prefix.with_suffix(".validation.json"),entry,fixture)
    elif action == "inspect-final":
        C.require(C.file_id(prefix.with_suffix(".reloaded.canonical")) == C.file_id(fixture/"final.canonical.bin"),"fresh inspect differs")
    else:
        native_result(prefix.with_suffix(".native.json"),entry,fixture,prefix.with_suffix(".input.wire"),prefix.with_suffix(".wire"),prefix.with_suffix(".canonical"),prefix.with_suffix(".queries"),receipt)
    return None


def native_result(path, entry, fixture, input_wire, wire, canonical, queries, receipt, expected_wire_bytes=None):
    row = C.exact(C.read(path), NATIVE_KEYS, "native result")
    C.require(type(row["schema_version"]) is int and row["schema_version"] == 1
              and row["kind"] == "serialization-comparison-native-result" and row["status"] == "passed"
              and row["library"] == entry["library"], "wrong native result identity/status")
    C.exact(row["phase_ns"],PHASES,"native phases")
    phases = [C.decimal(row["phase_ns"][k]) for k in PHASES]
    total = C.decimal(row["total_ns"],positive=True)
    interval = C.decimal(receipt["finished_monotonic_ns"]) - C.decimal(receipt["started_monotonic_ns"])
    C.require(sum(phases) <= total <= interval and interval <= receipt["timeout_seconds"]*1e9, "native timer interval differs")
    for key in ("peak_rss_before_audit_kib","peak_rss_after_audit_kib","cpu_ns","minor_faults","major_faults","record_count"):
        C.decimal(row[key])
    C.require(C.decimal(row["peak_rss_after_audit_kib"]) >= C.decimal(row["peak_rss_before_audit_kib"]), "RSS high-water reversed")
    expected = {"input_sha256":C.sha(input_wire), "trace_sha256":C.sha(fixture/"trace.bin"),
                "final_wire_sha256":C.sha(wire), "final_canonical_sha256":C.sha(canonical),
                "queries_sha256":C.sha(queries), "query_before_sha256":C.sha(fixture/"query-before.bin"),
                "query_after_sha256":C.sha(fixture/"query-after.bin")}
    C.require(all(C.hexsha(row[k]) and row[k] == v for k,v in expected.items()), "native digest differs from actual bytes")
    C.require(C.file_id(canonical) == C.file_id(fixture/"final.canonical.bin") and C.file_id(queries) == C.file_id(fixture/"queries.bin"), "native output differs from independent oracle")
    C.require(C.decimal(row["record_count"]) == C.read(fixture/"fixture.json")["n"], "native record count differs")
    if expected_wire_bytes is not None:
        C.require(Path(wire).stat().st_size==expected_wire_bytes,"deterministic final wire size changed")
    return row


def functional(prepared, profile_path, out, queue):
    prepared,profile_path = C.absolute(prepared),C.absolute(profile_path)
    audit(prepared)
    profile = read_profile(profile_path)
    out,context = begin_native(out,queue,profile,"functional",{"--prepared":prepared/"manifest.json","--profile":profile_path})
    copy_bundle(prepared,out/"prepared")
    C.copy_file(profile_path,out/"profile.json")
    before = runtime_identity(profile)
    C.write(out/"runtime-before.json",before)
    jobs,descriptions,completed,attempted = functional_jobs(),{},[],[]
    error = None
    started = time.monotonic()
    try:
        for job in jobs:
            argv=functional_argv(job,profile,out)
            prefix=out/"commands"/job["id"]
            timeout=launch_preflight(prefix,started+3600,60)
            attempted.append(job)
            receipt = execute(argv,prefix,timeout,profile["environment"])
            output = functional_output(out,job,profile,receipt)
            if job["action"] == "describe":
                descriptions[job["library"]] = output
            if job["action"] == "inspect-final":
                prefix = out/"commands"/(job["library"]+"-"+job["size"])
                completed.append(dict(library=job["library"],size=job["size"],input_wire=prefix.with_suffix(".input.wire").relative_to(out).as_posix(),result=prefix.with_suffix(".native.json").relative_to(out).as_posix()))
    except (C.ProtocolError,OSError,ValueError) as caught:
        error = type(caught).__name__+": "+str(caught)
    try:
        after = runtime_identity(profile)
    except (C.ProtocolError,OSError,ValueError) as caught:
        after = dict(identity_error=type(caught).__name__+": "+str(caught))
    C.write(out/"runtime-after.json",after)
    if before != after:
        error = "runtime/source/tool identity changed"
    C.seal(out,dict(schema_version=1,kind="sc-functional",status="complete" if error is None else "failed",error=error,
        completed=completed,descriptions=descriptions,attempted=attempted,unexecuted=jobs[len(attempted):],
        p4_complete=False,performance_accepted=False))
    C.require(error is None,"functional preserved failure: "+str(error))
    return dict(status="functional-native-completed-awaiting-queue",performance_accepted=False)


def freeze(functional_path, authorization_path, window_out, out):
    functional_path,authorization_path,window_out = map(C.absolute,(functional_path,authorization_path,window_out))
    audit(functional_path)
    status = C.read(functional_path/"status.json")
    C.require(status["kind"] == "sc-functional" and status["status"] == "complete" and len(status["completed"]) == 15, "normal functional gate incomplete")
    auth = C.exact(C.read(authorization_path),("schema_version","user_authorization","p4_evidence","p4_passed","quiet_window_confirmed","window_out"),"P6 authorization")
    C.require(type(auth["schema_version"]) is int and auth["schema_version"] == 1 and type(auth["user_authorization"]) is str and auth["user_authorization"]
              and auth["p4_passed"] is True and auth["quiet_window_confirmed"] is True and auth["window_out"] == str(window_out)
              and not window_out.exists(),"new P4/P6 approval/output prerequisite missing")
    p4 = C.absolute(auth["p4_evidence"])
    C.require(p4.is_file(),"P4 evidence missing")
    profile = read_profile(functional_path/"profile.json")
    runtime = runtime_identity(profile)
    C.require(runtime == C.read(functional_path/"runtime-after.json"),"functional inputs changed before freeze")
    out = C.fresh(out)
    C.copy_file(functional_path/"profile.json",out/"profile.json")
    C.copy_file(authorization_path,out/"authorization.json")
    C.copy_file(p4,out/"p4-evidence.json")
    C.copy_file(functional_path/"manifest.json",out/"functional-reference-manifest.json")
    C.copy_file(functional_path/"status.json",out/"functional-reference-status.json")
    for name in (*FILES,"fixture.json","queries.bin"):
        C.copy_file(functional_path/"prepared/M"/name,out/"fixture"/name)
    for lib in P.LIBRARIES:
        entry = profile["binaries"][lib]
        C.copy_file(entry["path"],out/"bin"/lib)
        if os.name == "posix":
            (out/"bin"/lib).chmod(0o755)
        matches = [r for r in status["completed"] if r["library"] == lib and r["size"] == "M"]
        C.require(len(matches) == 1,"missing M functional wire")
        C.copy_file(functional_path/matches[0]["input_wire"],out/"input-wires"/(lib+".wire"))
    C.write(out/"runtime.json",runtime)
    C.write(out/"schedule.json",P.schedule())
    C.write(out/"policy.json",P.POLICY)
    save_tools(out)
    # Exact final wire sizes are available from each functional M run.
    final_sizes = {}
    for lib in P.LIBRARIES:
        sample = next(r for r in status["completed"] if r["library"] == lib and r["size"] == "M")
        final_sizes[lib] = int(C.file_id(functional_path/Path(sample["result"]).with_suffix("").with_suffix(".wire"))["bytes"])
    predicted = sum(final_sizes[j["library"]] for j in P.schedule())
    C.require(predicted <= int(P.POLICY["maximum_new_wire_bytes"]),"predicted wire budget exceeded")
    C.require(shutil.disk_usage(out).free >= predicted + 1024**3,"insufficient retained-output disk space")
    C.require(runtime_identity(profile) == runtime,"inputs changed during freeze")
    C.seal(out,dict(schema_version=1,kind="sc-frozen",status="complete",window_out=str(window_out),
        functional_manifest=C.file_id(functional_path/"manifest.json"),p4_evidence=C.file_id(p4),
        predicted_new_wire_bytes=str(predicted),final_wire_bytes={k:str(v) for k,v in final_sizes.items()},
        performance_accepted=False,old_N3_authorization_reused=False))
    return audit(out)


def run(frozen, external_sha, out, queue):
    start = time.monotonic()
    frozen = C.absolute(frozen)
    audit(frozen,external_sha)
    profile = read_profile(frozen/"profile.json")
    frozen_status = C.read(frozen/"status.json")
    C.require(str(C.absolute(out)) == frozen_status["window_out"],"output differs from preregistration")
    before = runtime_identity(profile)
    C.require(before == C.read(frozen/"runtime.json") and tools_identity() == before["tools"],"registered runtime/tool/source changed")
    out,context = begin_native(out,queue,profile,"run",{"--frozen":frozen/"manifest.json"})
    C.require(context["request"]["timeout_seconds"] <= 2700,"queue deadline exceeds registered window")
    copy_bundle(frozen,out/"frozen")
    C.write(out/"frozen-pin.json",dict(sha256=external_sha))
    C.write(out/"runtime-before.json",before)
    jobs,samples,attempted,total_wire,error = P.schedule(),[],[],0,None
    try:
        for job in jobs:
            lib = job["library"]
            p = out/"commands"/job["id"]
            wire,canon,queries,result = (p.with_suffix(ext) for ext in (".wire",".canonical",".queries",".native.json"))
            entry = profile["binaries"][lib]
            argv = command(entry,"run","--wire",out/"frozen/input-wires"/(lib+".wire"),"--trace",out/"frozen/fixture/trace.bin",
                "--wire-out",wire,"--canonical-out",canon,"--queries-out",queries,"--result",result,binary=out/"frozen/bin"/lib)
            limit=max(int(frozen_status["final_wire_bytes"][lib]),(out/"frozen/fixture/final.canonical.bin").stat().st_size,
                      (out/"frozen/fixture/queries.bin").stat().st_size,1024**2)
            timeout=launch_preflight(p,start+2700,20,total_wire,int(frozen_status["final_wire_bytes"][lib]),limit,16*1024**3)
            attempted.append(job)
            receipt = execute(argv,p,timeout,profile["environment"],limit)
            row = native_result(result,entry,out/"frozen/fixture",out/"frozen/input-wires"/(lib+".wire"),wire,canon,queries,receipt,int(frozen_status["final_wire_bytes"][lib]))
            size = wire.stat().st_size
            total_wire += size
            samples.append(dict(job=job,total_ns=row["total_ns"]))
            if job["kind"] == "aa" and (len(samples) == len(jobs) or jobs[len(samples)].get("anchor") != job["anchor"]):
                C.require(P.anchor_gates(samples,job["anchor"])["qualified"],"anchor qualification failed: "+job["anchor"])
    except (C.ProtocolError,OSError,ValueError) as caught:
        error = type(caught).__name__+": "+str(caught)
    total_wire = sum(p.stat().st_size for p in (out/"commands").glob("*.wire"))
    try:
        after = runtime_identity(profile)
    except (C.ProtocolError,OSError,ValueError) as caught:
        after = dict(identity_error=type(caught).__name__+": "+str(caught))
    C.write(out/"runtime-after.json",after)
    if before != after:
        error = "runtime/source/tool identity changed"
    rebuilt = P.summary(samples)
    if error is not None:
        rebuilt["performance_accepted"] = False
    C.write(out/"summary.json",rebuilt)
    C.seal(out,dict(schema_version=1,kind="sc-run",status="complete" if error is None else "failed",error=error,
        attempted=attempted,successful_children=len(samples),unexecuted=jobs[len(attempted):],new_wire_bytes=str(total_wire),
        elapsed_ns=str(int((time.monotonic()-start)*1e9)),performance_accepted=False))
    C.require(error is None,"preserved campaign failure: "+str(error))
    return dict(status="native-completed-awaiting-queue-audit",performance_accepted=False)


def queue_audit(out, profile, action):
    out = Path(out)
    context = C.read(out/"execution-context.json")
    queue = out/"queue"
    qmanifest = C.read(queue/"manifest.json")
    actual = C.inventory(queue,exclude=("manifest.json",))
    # The maintained runner uses integer byte lengths; validate and normalize explicitly.
    qfiles = {r["path"]:dict(bytes=str(r["bytes"]),sha256=r["sha256"]) for r in qmanifest["files"]}
    C.require(qfiles == actual and len(qfiles) == len(qmanifest["files"])
              and set(qfiles) == {"request.json","result.json","events.json","supervisor.log","stdout.log","stderr.log"},"queue members changed")
    request,result = C.read(queue/"request.json"),C.read(queue/"result.json")
    C.require(context["request"] == request and context["queue_request_sha256"] == C.sha(queue/"request.json")
              and context["action"] == action and request["lock_dir"] == profile["lock_dir"]
              and request["runner_sha256"] == C.sha(out/"protocol/run_job.py")
              and request["kind"] == ("timing" if action == "run" else "functional")
              and request["owner"] == result["owner"] == "W3" and request["run_id"] == result["run_id"],"queue/context identity differs")
    basis=C.read(out/("frozen/runtime.json" if action=="run" else "runtime-before.json"))
    C.require(context["tools"] == basis["tools"] and context["interpreter"] == basis["interpreter"]
              and context["tools"] == {p.name:C.file_id(p) for p in (out/"protocol").iterdir()},"queue interpreter/tool bytes differ from frozen basis")
    C.require(result["status"] in ("complete","failed","timeout","cancelled","error") and result.get("cleanup") == {"live":[],"process_groups":[],"zombies":[]},"queue cleanup/status invalid")
    maximum=2700 if action=="run" else 3600
    elapsed=result.get("command_elapsed_seconds")
    C.require(type(elapsed) in (int,float) and elapsed>0,"invalid queue elapsed")
    try:
        created=datetime.fromisoformat(request["created_at"].replace("Z","+00:00"))
        finished=datetime.fromisoformat(result["finished_at"].replace("Z","+00:00"))
        C.require(created.utcoffset() is not None and finished.utcoffset() is not None,"queue timestamps require timezone")
        whole=(finished-created).total_seconds()
    except (ValueError,TypeError,AttributeError) as error:
        raise C.ProtocolError("invalid queue created/finished timestamps") from error
    C.require(whole>0 and elapsed<=whole+0.001,"queue timestamps/command elapsed differ")
    if result["status"]=="complete":
        C.require(elapsed<=maximum and whole<=maximum,"complete queue exceeds strict full-window budget")
    else:
        grace,wait=request.get("termination_grace_seconds",2),request.get("wait_seconds",0)
        C.require(type(grace) in (int,float) and grace>0 and type(wait) in (int,float) and wait>=0,"invalid failed queue cleanup/wait allowance")
        C.require(elapsed<=maximum+grace+1 and whole<=maximum+wait+grace+1,"failed queue exceeds declared cleanup allowance")
    command_argv = request["command"]
    expected_head = [context["requested_interpreter"],context["adapter_path"]]
    if len(command_argv)>1 and command_argv[1] == "-B":
        command_argv = [command_argv[0],*command_argv[2:]]
    C.validate_id(context["interpreter"]["identity"])
    C.require(command_argv[:2] == expected_head and C.absolute_string(expected_head[0]) and C.absolute_string(expected_head[1])
              and command_argv[2] == action,"queue interpreter/adapter/action differs")
    args = command_argv[3:]
    C.require(len(args)%2 == 0,"odd queue argv")
    flags = dict(zip(args[::2],args[1::2]))
    expected_flags = {"--frozen","--frozen-sha256","--out","--queue"} if action == "run" else {"--prepared","--profile","--out","--queue"}
    C.require(set(flags) == expected_flags and len(flags)*2 == len(args) and flags["--out"] == context["original_output"]
              and flags["--queue"] == request["out"] == context["original_output"]+"/queue","queue original output differs")
    for flag,binding in context["inputs"].items():
        original = flags.get(flag)
        C.require(C.absolute_string(original) and C.absolute_string(binding["original_path"]),"relative input path")
        suffix = "/manifest.json" if flag in ("--frozen","--prepared") else ""
        C.require(original+suffix == binding["original_path"],"queue original input differs")
        archived = out/({"--frozen":"frozen/manifest.json","--prepared":"prepared/manifest.json","--profile":"profile.json"}[flag])
        C.require(C.file_id(archived) == binding["identity"],"queue input identity differs")
    if action == "run":
        C.require(flags["--frozen-sha256"] == C.read(out/"frozen-pin.json")["sha256"],"external argv pin differs")
    return result


def _receipt(out, name, expected_argv, environment, previous_end=0, maximum=60, file_limit=512*1024**2):
    prefix = Path(out)/"commands"/name
    row = C.exact(C.read(prefix.with_suffix(".receipt.json")),("schema_version","argv","execution_environment","timeout_seconds","started_monotonic_ns","finished_monotonic_ns","returncode","timed_out","pid","launch_error","file_limit_bytes"),"receipt")
    C.require(type(row["schema_version"]) is int and row["schema_version"] == 1 and row["argv"] == expected_argv and row["execution_environment"] == environment,"receipt command/environment differs")
    start,end = C.decimal(row["started_monotonic_ns"],True),C.decimal(row["finished_monotonic_ns"],True)
    C.require(start >= previous_end and end >= start and type(row["timeout_seconds"]) in (int,float)
              and 0 < row["timeout_seconds"] <= maximum and type(row["timed_out"]) is bool,"invalid/overlapping receipt")
    C.require((row["launch_error"] is None and type(row["returncode"]) is int and C.integer(row["pid"],1))
              or (type(row["launch_error"]) is str and row["launch_error"] and row["returncode"] is None and row["pid"] is None),"invalid launch result")
    C.require(C.decimal(row["file_limit_bytes"],True)==file_limit,"child file ceiling differs")
    successful=row["returncode"]==0 and not row["timed_out"] and row["launch_error"] is None
    allowance=0 if successful else 1
    C.require(end-start <= (row["timeout_seconds"]+allowance)*1e9,"receipt exceeds success timeout/failed cleanup allowance")
    return row,end


def audit(out, external_sha=None, bundle_sha=None):
    out = C.absolute(out)
    status = C.read(out/"status.json")
    C.check_inventory(out, external_sha if status.get("kind") == "sc-frozen" else None)
    if bundle_sha is not None:
        C.require(C.hexsha(bundle_sha) and postrun_pin(out)==bundle_sha,"external composite post-run pin differs")
    C.require(type(status.get("schema_version")) is int and status["schema_version"] == 1,"unknown bundle schema")
    kind = status.get("kind")
    if kind == "sc-prepared":
        C.exact(status,("schema_version","kind","status","fixture_source","original_fixture_source","record_counts","performance_accepted"),"prepared status")
        C.require(status["status"] == "complete" and status["record_counts"] == SIZES,"prepared status differs")
        C.require(C.file_id(out/"reference.py") == status["fixture_source"],"fixture source identity differs")
        for size,n in SIZES.items():
            fixture_check(out/size,n)
    elif kind == "sc-frozen":
        C.exact(status,("schema_version","kind","status","window_out","functional_manifest","p4_evidence","predicted_new_wire_bytes","final_wire_bytes","performance_accepted","old_N3_authorization_reused"),"frozen status")
        C.require(status["status"] == "complete" and C.same(C.read(out/"schedule.json"),P.schedule())
                  and C.same(C.read(out/"policy.json"),P.POLICY),"frozen schedule/policy differs")
        fixture_check(out/"fixture",16384)
        profile = read_profile(out/"profile.json")
        runtime = C.read(out/"runtime.json")
        candidate_runtime_identity(runtime)
        auth=C.exact(C.read(out/"authorization.json"),("schema_version","user_authorization","p4_evidence","p4_passed","quiet_window_confirmed","window_out"),"archived authorization")
        C.require(type(auth["schema_version"]) is int and auth["schema_version"]==1 and type(auth["user_authorization"]) is str and auth["user_authorization"]
                  and auth["p4_passed"] is True and auth["quiet_window_confirmed"] is True and auth["window_out"]==status["window_out"]
                  and C.absolute_string(auth["p4_evidence"]) and C.absolute_string(status["window_out"]),"archived P4/P6 authorization invalid")
        C.require(set(status["final_wire_bytes"])==set(P.LIBRARIES),"frozen output size inventory differs")
        predicted=sum(C.decimal(status["final_wire_bytes"][j["library"]],True) for j in P.schedule())
        C.require(C.decimal(status["predicted_new_wire_bytes"])==predicted<=int(P.POLICY["maximum_new_wire_bytes"]),"frozen expected output budget differs")
        for lib in P.LIBRARIES:
            C.require(C.file_id(out/"bin"/lib) == runtime["binaries"][lib],"frozen binary identity differs")
        C.require(C.file_id(out/"functional-reference-manifest.json") == status["functional_manifest"]
                  and C.file_id(out/"p4-evidence.json") == status["p4_evidence"],"functional/P4 provenance differs")
        C.require(runtime["tools"] == {p.name:C.file_id(p) for p in (out/"protocol").iterdir()},"frozen protocol identity differs")
    elif kind == "sc-functional":
        C.exact(status,("schema_version","kind","status","error","completed","descriptions","attempted","unexecuted","p4_complete","performance_accepted"),"functional status")
        audit(out/"prepared")
        profile = read_profile(out/"profile.json")
        queue_result = queue_audit(out,profile,"functional")
        before,after = C.read(out/"runtime-before.json"),C.read(out/"runtime-after.json")
        candidate_runtime_identity(before)
        runtime_changed = before != after
        C.require(not runtime_changed or status["status"] == "failed" and status["error"] == "runtime/source/tool identity changed","unreported functional runtime change")
        candidate_runtime_identity(after,allow_identity_error=runtime_changed)
        C.require(status["status"] in ("complete","failed"),"invalid functional status")
        original,previous = context_original(out),0
        attempted,jobs = status["attempted"],functional_jobs()
        C.require(type(attempted) is list and C.same(attempted,jobs[:len(attempted)]) and C.same(status["unexecuted"],jobs[len(attempted):]),"functional prefix differs")
        C.require({p.name.removesuffix(".receipt.json") for p in (out/"commands").glob("*.receipt.json")} == {j["id"] for j in attempted},"functional extra/missing receipt")
        seen_completed,descriptions = [],{}
        for job in attempted:
            receipt,previous = _receipt(out,job["id"],functional_argv(job,profile,original),profile["environment"],previous)
            success = not receipt["timed_out"] and receipt["returncode"] == 0 and (out/"commands"/(job["id"]+".stderr")).stat().st_size == 0
            if success:
                try:
                    output = functional_output(out,job,profile,receipt)
                except (C.ProtocolError,OSError,ValueError):
                    C.require(job == attempted[-1] and status["status"] == "failed","unreported functional output failure")
                else:
                    if job["action"] == "describe":
                        descriptions[job["library"]] = output
                    if job["action"] == "inspect-final":
                        prefix = Path("commands")/(job["library"]+"-"+job["size"])
                        seen_completed.append(dict(library=job["library"],size=job["size"],input_wire=prefix.with_suffix(".input.wire").as_posix(),result=prefix.with_suffix(".native.json").as_posix()))
            else:
                C.require(job == attempted[-1] and status["status"] == "failed","continued after functional child failure")
        C.require(seen_completed == status["completed"] and descriptions == status["descriptions"],"functional completed inventory differs")
        if status["status"] == "complete":
            C.require(queue_result["status"] == "complete" and queue_result["command_returncode"] == 0 and len(attempted) == 80 and len(status["completed"]) == 15,"functional success incomplete")
    elif kind == "sc-run":
        C.exact(status,("schema_version","kind","status","error","attempted","successful_children","unexecuted","new_wire_bytes","elapsed_ns","performance_accepted"),"run status")
        C.require(C.hexsha(external_sha),"run audit requires independent external frozen manifest pin")
        pin = C.read(out/"frozen-pin.json")["sha256"]
        C.require(pin == external_sha,"external frozen manifest pin differs")
        audit(out/"frozen",pin)
        profile = read_profile(out/"frozen/profile.json")
        qresult = queue_audit(out,profile,"run")
        before,after = C.read(out/"runtime-before.json"),C.read(out/"runtime-after.json")
        candidate_runtime_identity(before)
        runtime_changed = before != after
        C.require(before == C.read(out/"frozen/runtime.json"),"runtime before differs from frozen")
        C.require(not runtime_changed or status["status"] == "failed" and status["error"] == "runtime/source/tool identity changed","unreported runtime change")
        candidate_runtime_identity(after,allow_identity_error=runtime_changed)
        attempted,jobs = status["attempted"],P.schedule()
        C.require(type(attempted) is list and C.same(attempted,jobs[:len(attempted)]) and C.same(status["unexecuted"],jobs[len(attempted):]),"not exact attempted prefix/unexecuted suffix")
        original,previous,samples,total,first_start = context_original(out),0,[],0,None
        receipt_names={p.name.removesuffix(".receipt.json") for p in (out/"commands").glob("*.receipt.json")}
        C.require(receipt_names == {j["id"] for j in attempted},"extra/missing attempted receipts")
        allowed={j["id"]+ext for j in attempted for ext in (".receipt.json",".stdout",".stderr",".native.json",".wire",".canonical",".queries")}
        C.require(all(p.name in allowed and p.is_file() for p in (out/"commands").iterdir()),"unregistered command output")
        for job in attempted:
            name,lib = job["id"],job["library"]
            prefix = original/"commands"/name
            wire,canon,queries,result = (prefix.with_suffix(ext) for ext in (".wire",".canonical",".queries",".native.json"))
            entry = profile["binaries"][lib]
            argv = command(entry,"run","--wire",original/"frozen/input-wires"/(lib+".wire"),"--trace",original/"frozen/fixture/trace.bin", "--wire-out",wire,"--canonical-out",canon,"--queries-out",queries,"--result",result,binary=original/"frozen/bin"/lib)
            limit=max(int(C.read(out/"frozen/status.json")["final_wire_bytes"][lib]),(out/"frozen/fixture/final.canonical.bin").stat().st_size,(out/"frozen/fixture/queries.bin").stat().st_size,1024**2)
            receipt,previous = _receipt(out,name,argv,profile["environment"],previous,20,limit)
            if first_start is None:
                first_start=C.decimal(receipt["started_monotonic_ns"])
            success = not receipt["timed_out"] and receipt["returncode"] == 0 and (out/"commands"/(name+".stderr")).stat().st_size == 0
            if success:
                try:
                    native = native_result(out/result.relative_to(original),entry,out/"frozen/fixture",out/"frozen/input-wires"/(lib+".wire"),out/wire.relative_to(original),out/canon.relative_to(original),out/queries.relative_to(original),receipt,int(C.read(out/"frozen/status.json")["final_wire_bytes"][lib]))
                except (C.ProtocolError,OSError,ValueError):
                    C.require(job == attempted[-1] and status["status"] == "failed" and status["successful_children"] == len(samples),"unreported invalid final native output")
                else:
                    samples.append(dict(job=job,total_ns=native["total_ns"]))
                    total += (out/wire.relative_to(original)).stat().st_size
            else:
                C.require(job == attempted[-1] and status["status"] == "failed","execution after failed child")
        total=sum(p.stat().st_size for p in (out/"commands").glob("*.wire"))
        elapsed=C.decimal(status["elapsed_ns"])
        C.require(C.integer(status["successful_children"],0,285) and status["successful_children"] == len(samples) and status["new_wire_bytes"] == str(total)
                  and total <= 16*1024**3 and elapsed <= qresult["command_elapsed_seconds"]*10**9+10**6
                  and (first_start is None or previous-first_start<=elapsed+10**6)
                  and (status["status"]=="failed" or elapsed<=2700*10**9),"campaign count/wire/time budget differs")
        rebuilt = P.summary(samples)
        if status["status"] == "failed":
            rebuilt["performance_accepted"] = False
        C.require(C.same(C.read(out/"summary.json"),rebuilt),"summary differs from raw native results")
        if status["status"] == "complete":
            C.require(len(attempted) == 285 and rebuilt["measurement_qualified"] and qresult["status"] == "complete"
                      and qresult["command_returncode"] == 0,"incomplete successful campaign")
    else:
        raise C.ProtocolError("unknown bundle kind")
    return dict(status="audit-passed" if status["status"] == "complete" else "audit-preserved-failure",kind=kind,
                performance_accepted=kind == "sc-run" and status["status"] == "complete" and bundle_sha is not None,
                final_manifest_externally_pinned=bundle_sha is not None,
                runtime_contents_archived=False if kind in ("sc-frozen","sc-run") else None)


def context_original(out):
    text = C.read(Path(out)/"execution-context.json")["original_output"]
    C.require(C.absolute_string(text),"missing original output path")
    return PureWindowsPath(text) if PureWindowsPath(text).is_absolute() else PurePosixPath(text)


def report(bundle, out, external_sha=None, bundle_sha=None):
    result = audit(bundle,external_sha,bundle_sha)
    C.require(C.read(Path(bundle)/"status.json")["kind"] != "sc-run" or bundle_sha is not None,"run report requires external final manifest pin")
    out = C.fresh(out)
    C.write(out/"report.json",dict(schema_version=1,audit=result,bundle_manifest=C.file_id(Path(bundle)/"manifest.json"),
                                summary=C.read(Path(bundle)/"summary.json") if (Path(bundle)/"summary.json").exists() else None))
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action",required=True)
    p = sub.add_parser("prepare"); p.add_argument("--fixture-module",type=Path,required=True); p.add_argument("--out",type=Path,required=True)
    p = sub.add_parser("functional")
    for flag in ("prepared","profile","out","queue"):
        p.add_argument("--"+flag,type=Path,required=True)
    p = sub.add_parser("freeze")
    for flag in ("functional","authorization","window-out","out"):
        p.add_argument("--"+flag,type=Path,required=True)
    p = sub.add_parser("run")
    for flag in ("frozen","out","queue"):
        p.add_argument("--"+flag,type=Path,required=True)
    p.add_argument("--frozen-sha256",required=True)
    p = sub.add_parser("audit"); p.add_argument("--bundle",type=Path,required=True); p.add_argument("--frozen-sha256"); p.add_argument("--postrun-pin")
    p = sub.add_parser("report"); p.add_argument("--bundle",type=Path,required=True); p.add_argument("--out",type=Path,required=True); p.add_argument("--frozen-sha256"); p.add_argument("--postrun-pin")
    p = sub.add_parser("pin",help="read-only composite post-run pin; does not grant authorization");p.add_argument("--bundle",type=Path,required=True)
    args = parser.parse_args(argv)
    try:
        if args.action == "prepare": result = prepare(args.fixture_module,args.out)
        elif args.action == "functional": result = functional(args.prepared,args.profile,args.out,args.queue)
        elif args.action == "freeze": result = freeze(args.functional,args.authorization,args.window_out,args.out)
        elif args.action == "run": result = run(args.frozen,args.frozen_sha256,args.out,args.queue)
        elif args.action == "report": result = report(args.bundle,args.out,args.frozen_sha256,args.postrun_pin)
        elif args.action == "pin": result=dict(schema_version=1,kind="sc-composite-post-run-pin",algorithm="SHA256(SC-POST-RUN-PIN-v1\\0 + raw32(main_manifest_sha256) + raw32(queue_manifest_sha256))",pin=postrun_pin(args.bundle),performance_accepted=False)
        else:
            result = audit(args.bundle,args.frozen_sha256,args.postrun_pin)
        print(C.encoded(result).decode(),end="")
        return 0
    except (C.ProtocolError,OSError,ValueError,KeyError) as error:
        print(C.encoded(dict(status="rejected",reason=type(error).__name__+": "+str(error),performance_accepted=False)).decode(),end="")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
