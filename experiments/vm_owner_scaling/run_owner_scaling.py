#!/usr/bin/env python3
"""Isolated D018 preparation, queued build/collection, and offline audit.

No formal timing command. C++ execution requires the accepted default queue.
Each collection job runs at most four of the 26 frozen workload cases.
"""
from __future__ import annotations

import argparse
import copy
import datetime as dt
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import resource
import selectors
import shlex
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
BASELINE = {
    "xoffsetdatastructure.hpp": "9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2",
    "xoffset_virtual_memory.hpp": "22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad",
}
W2 = "docs/validation/vm-observability/frozen-implementation-v1"
QUEUE_SHA = "6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909"
ACCEPTED = {
    "xoffsetdatastructure.hpp": "8d9447a81ca07f758fa813fbcc0823f8e8910d94cc2d9214bd7fda757a2964e6",
    "xoffset_virtual_memory.hpp": "b1caeed243f6ba412c3f68104730df21e0ad580193adc1c3186749c76c1c4e57",
    "n3_runtime.hpp": "0ee7e25b9a76f0e7b06acc25554be57219cd17982e3d6e1898a376ddacfba10a",
    "audit_events.py": "91ac07d1c54f306f965235b156a7ff95f18040c8eb65660434654342a626ee08",
    "event_kinds.json": "7bc7b0a77d24a8557a601a793daead2690406603fed26d78931cd3b942bb2e2c",
}
CONDITIONS = [(8, 1), (64, 1), (256, 1), (256, 2), (256, 4), (256, 8)]
BOOST = "container core config throw_exception static_assert container_hash type_traits winapi intrusive predef".split()
LOCAL = ["owner_scaling.cpp", "audit_owner_scaling.py", "test_audit_owner_scaling.py", "run_owner_scaling.py", "contract.json"]
PROVENANCE = {
    "d018-acceptance.json": "25b432405e2c2a2e948d104ee9f8a622d80ab74f9016746e22cccee6f28a50c3",
    "w2-acceptance.json": "9c3b5c711cc48378b7ec8744c10faeafb30bcf03eb443842262660d78dd9392b",
    "w2-build.json": "5860c08dc5f614993aa88708e2c5a741dd30a69a5362a2df8a50951b592ce579",
    "toolchain.json": "a118ad089d309feed24d05c5ce709dca6e2d02140ff09440242152e7861ba3a7",
    "queue-runner.py": QUEUE_SHA,
}
EVIDENCE_LIMIT = 536870912
LOG_LIMIT = 8388608
COMPILER_FILE_LIMIT = 134217728
TASK_REPO = None
EXTRA_RUNTIME = {"/usr/local/lib/libunwind.1.0.dylib": "a7539f393b8f785dd09b4f7fb7c109382dd7d2db0ee903c10ab45e777c8cc4aa"}


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1048576), b""):
            h.update(block)
    return h.hexdigest()


def read(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON field: " + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique,
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError("invalid constant " + x)))


def write(path, value):
    content = json.dumps(value, indent=2, sort_keys=True) + "\n"
    path = Path(path)
    if TASK_REPO is not None:
        replacing = path.stat().st_size if path.exists() else 0
        require(evidence_size(TASK_REPO) - replacing + len(content.encode()) <= EVIDENCE_LIMIT,
                "D018 evidence budget cannot hold JSON output")
    path.write_text(content)


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def inventory(root):
    result = {}
    for p in sorted(Path(root).rglob("*")):
        require(not p.is_symlink() and (p.is_dir() or p.is_file()), "nonregular artifact " + str(p))
        if p.is_file():
            result[p.relative_to(root).as_posix()] = sha(p)
    return result


def check_files(root, expected, exact=False):
    for name, digest in expected.items():
        p = Path(name)
        require(not p.is_absolute() and ".." not in p.parts, "unsafe inventory path")
        p = Path(root) / p
        require(p.is_file() and not p.is_symlink() and sha(p) == digest, "changed artifact " + str(p))
    if exact:
        actual = inventory(root)
        actual.pop("result.json", None)
        require(actual == expected, "artifact inventory differs")


def audit_inputs(root):
    root = Path(root)
    expected = {"input/timing/" + n: h for n, h in BASELINE.items()}
    for n, h in ACCEPTED.items():
        prefix = "common/w2/" if n.endswith((".py", ".json")) else "input/diagnostic/"
        expected[prefix + n] = h
    expected.update({"provenance/" + n: h for n, h in PROVENANCE.items()})
    check_files(root, expected)
    require(set(inventory(root / "input/timing")) == set(BASELINE), "unexpected timing input")
    require(set(inventory(root / "input/diagnostic")) == set(BASELINE) | {"n3_runtime.hpp"}, "unexpected diagnostic input")
    c = read(root / "common/contract.json")
    accepted = read(root / "provenance/d018-acceptance.json")
    require(c["acceptance_sha256"] == PROVENANCE["d018-acceptance.json"] and c["baseline_headers"] == BASELINE,
            "contract authority differs")
    for field in ("conditions", "workload", "diagnostic", "bounded_validation"):
        require(c[field] == accepted[field], "contract differs from D018: " + field)
    require(sha(root / "common/run_owner_scaling.py") == sha(__file__), "audit runner differs from frozen source")


def fresh(path):
    p = Path(os.path.abspath(path))
    require(not p.exists(), "output must be new: " + str(p))
    p.mkdir(parents=True)
    return p


def evidence_size(repo, reserve=0):
    total = sum(p.stat().st_size for directory in (repo / "build/vm-owner-scaling", repo / "docs/validation/vm-owner-scaling")
                if directory.exists() for p in directory.rglob("*") if p.is_file())
    require(total + reserve <= EVIDENCE_LIMIT, "D018 aggregate evidence budget exhausted")
    return total


def finish(out, record):
    record.update(status="passed", finished_at=now(), timing_evidence=False)
    record["files"] = inventory(out)
    record["files"].pop("result.json", None)
    write(out / "result.json", record)


def basic_audit(out, kind):
    r = read(Path(out) / "result.json")
    require(r.get("schema_version") == 1 and r.get("kind") == kind and r.get("status") == "passed"
            and r.get("timing_evidence") is False, "wrong or failed artifact kind")
    check_files(out, r["files"], exact=True)
    return r


def coordinated(repo):
    runner = repo / "experiments/vm_coordination/run_job.py"
    require(sha(runner) == QUEUE_SHA, "default queue runner identity differs")
    lock = repo / "build/vm-coordination/queue.lock"
    require(lock.is_file() and not lock.is_symlink(), "default queue lock unavailable")
    with lock.open("r+") as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(f, fcntl.LOCK_UN)
            raise ValueError("invoke build/collect through default run_job.py")
        metadata = json.load(f)
    ancestors, pid = [], os.getpid()
    while pid > 1 and pid not in ancestors:
        ancestors.append(pid)
        pid = int(subprocess.check_output(["ps", "-o", "ppid=", "-p", str(pid)], text=True, timeout=5).strip())
    require(metadata.get("status") == "running" and metadata.get("owner") == "W1"
            and metadata.get("supervisor_pid") in ancestors, "queue belongs to another process tree")
    request = read(Path(metadata["out"]) / "request.json")
    require(request["run_id"] == metadata["run_id"] and request["owner"] == "W1" and
            request["runner_sha256"] == QUEUE_SHA and 0 < request["timeout_seconds"] <= 180 and
            request["kind"] in ("build", "validation", "functional", "diagnostic") and
            request["lock_dir"] == str(repo / "build/vm-coordination") and request["source_headers"] == BASELINE,
            "queue request differs from D018 bounds/default queue/F")
    return {"runner_sha256": QUEUE_SHA, "metadata": metadata, "ancestors": ancestors, "request": request}


def execute(out, name, argv, record, timeout=30):
    command = {"argv": [str(x) for x in argv], "started_at": now(), "timeout_seconds": timeout}
    command["log_file_limit_bytes"] = LOG_LIMIT
    file_limit = COMPILER_FILE_LIMIT if name.startswith("compile-") else LOG_LIMIT
    command["child_file_limit_bytes"] = file_limit
    if TASK_REPO is not None:
        evidence_size(TASK_REPO, 2 * LOG_LIMIT + (2 * file_limit if name.startswith("compile-") else 0) + 4194304)
    def cap_files():
        resource.setrlimit(resource.RLIMIT_FSIZE, (file_limit, file_limit))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    try:
        with (out / (name + ".stdout")).open("wb") as stdout, (out / (name + ".stderr")).open("wb") as stderr:
            process = subprocess.Popen(command["argv"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=cap_files)
            deadline = time.monotonic() + timeout
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(process.stdout, selectors.EVENT_READ, (stdout, 0))
                    selector.register(process.stderr, selectors.EVENT_READ, (stderr, 0))
                    while selector.get_map():
                        remaining = deadline - time.monotonic()
                        if remaining <= 0:
                            raise subprocess.TimeoutExpired(command["argv"], timeout)
                        for key, _ in selector.select(min(remaining, 0.1)):
                            chunk = os.read(key.fd, 65536)
                            if not chunk:
                                selector.unregister(key.fileobj)
                                continue
                            stream, used = key.data
                            require(used + len(chunk) <= LOG_LIMIT, "bounded child log exceeded: " + name)
                            stream.write(chunk)
                            selector.modify(key.fileobj, selectors.EVENT_READ, (stream, used + len(chunk)))
                command["returncode"] = process.wait(timeout=max(0.001, deadline - time.monotonic()))
            except BaseException:
                process.kill()
                process.wait(timeout=5)
                raise
            finally:
                process.stdout.close()
                process.stderr.close()
    except subprocess.TimeoutExpired:
        command["timed_out"] = True
        raise
    finally:
        command["finished_at"] = now()
        record.setdefault("commands", []).append(command)
        write(out / "result.json", record)
    return command, (out / (name + ".stdout")).read_bytes()


def prepare(args, out, record):
    repo = args.repo_root.resolve()
    sources = {"common/" + n: repo / "experiments/vm_owner_scaling" / n for n in LOCAL}
    for name in BASELINE:
        sources["input/timing/" + name] = repo / name
    frozen = repo / W2
    for name in ACCEPTED:
        dest = "common/w2/" if name.endswith((".py", ".json")) else "input/diagnostic/"
        src = "source/" if dest == "common/w2/" else "build/input/diagnostic/"
        sources[dest + name] = frozen / src / name
    extras = {
        "d018-acceptance.json": repo / "docs/coordination/vm-followup/contracts/W1-owner-scaling-v1/acceptance.json",
        "w2-acceptance.json": repo / "docs/coordination/vm-followup/W2-G1-acceptance.json",
        "w2-build.json": frozen / "build/result.json",
        "toolchain.json": repo / "toolchains/vm-validation.json",
        "queue-runner.py": repo / "experiments/vm_coordination/run_job.py",
    }
    sources.update({"provenance/" + n: p for n, p in extras.items()})
    before = {n: sha(p) for n, p in sources.items()}
    evidence_size(repo, sum(p.stat().st_size for p in sources.values()) + 4194304)
    for n, h in BASELINE.items():
        require(before["input/timing/" + n] == h, "F identity differs")
    for n, h in ACCEPTED.items():
        prefix = "common/w2/" if n.endswith((".py", ".json")) else "input/diagnostic/"
        require(before[prefix + n] == h, "W2 accepted identity differs: " + n)
    c = read(sources["common/contract.json"])
    require(before["provenance/d018-acceptance.json"] == c["acceptance_sha256"], "D018 identity differs")
    require(before["provenance/queue-runner.py"] == QUEUE_SHA, "queue identity differs")
    for n, p in sources.items():
        target = out / n
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, target)
    require(before == {n: sha(p) for n, p in sources.items()}, "source changed during freeze")
    check_files(out, before)
    audit_inputs(out)
    record.update(source_files=before, decision="D018", baseline_headers=BASELINE)
    finish(out, record)


def compiler_argv(root, profile, repo):
    argv = ["/usr/local/bin/clang++", "-O2", "-std=c++26", "-freflection", "-fexpansion-statements",
            "-stdlib=libc++", "-pthread", "-arch", "arm64", "-isysroot",
            "/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk", "-mmacosx-version-min=10.15"]
    for n in BOOST:
        argv += ["-isystem", str(repo / "external/boost/libs" / n / "include")]
    argv += ["-isystem", str(repo / "external/typelayout/include"), "-L/usr/local/lib", "-Wl,-rpath,/usr/local/lib",
             "-MD", "-MF", str(root / (profile + ".d")), "-DN3_DIAGNOSTIC=" + str(int(profile == "diagnostic")),
             "-I", str(root / "input" / profile), str(root / "common/owner_scaling.cpp"),
             "-o", str(root / ("owner_scaling_" + profile))]
    return argv


def dep_paths(path):
    text = Path(path).read_text().replace("\\\n", " ")
    return [Path(os.path.abspath(p)) for p in shlex.split(text.split(":", 1)[1])]


def build(args, out, record):
    repo = args.repo_root.resolve()
    record["coordination"] = coordinated(repo)
    evidence_size(repo, 6 * LOG_LIMIT)
    parent = basic_audit(args.prepared, "prepare")
    audit_inputs(args.prepared)
    evidence_size(repo, sum(p.stat().st_size for p in args.prepared.rglob("*") if p.is_file()) + 4194304)
    require(parent["source_files"]["common/run_owner_scaling.py"] == sha(__file__), "execute frozen runner")
    for n in ("common", "input", "provenance"):
        shutil.copytree(args.prepared / n, out / n)
    shutil.copy2(args.prepared / "result.json", out / "prepared-result.json")
    lock = read(out / "provenance/toolchain.json")
    expected = read(out / "provenance/w2-build.json")["external_files"]
    catalog = dict(expected)
    catalog.update({str(Path(p).resolve()): h for p, h in expected.items()})
    # Include new standard synchronization headers in the prebuild catalog.
    for prefix in (Path("/usr/local/include/c++/v1"), Path("/usr/local/lib/clang/21/include")):
        for p in prefix.rglob("*"):
            if p.is_file():
                catalog[str(p)] = sha(p)
                catalog[str(p.resolve())] = sha(p)
    for p, h in expected.items():
        require(sha(p) == h, "accepted external dependency changed: " + p)
    for n, commit in lock["dependencies"].items():
        actual = subprocess.check_output(["git", "-C", str(repo / "external" / n), "rev-parse", "HEAD"], text=True).strip()
        dirty = subprocess.check_output(["git", "-C", str(repo / "external" / n), "status", "--porcelain"], text=True).strip()
        require(actual == commit and not dirty, "dependency revision/cleanliness differs: " + n)
    tc = lock["profiles"]["macos-p2996"]
    compiler = Path("/usr/local/bin/clang++")
    require(sha(compiler) == tc["compiler_binary_sha256_observed"], "compiler identity differs")
    version_command, raw = execute(out, "compiler-version", [str(compiler), "--version"], record)
    require(version_command["returncode"] == 0 and tc["compiler_commit"] in raw.decode(), "compiler revision differs")
    record["compiler"] = {"executed": str(compiler), "resolved": str(compiler.resolve()), "sha256": sha(compiler), "version": raw.decode()}
    record["dependencies"] = lock["dependencies"]
    sdk = Path("/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk")
    require(str(read(sdk / "SDKSettings.json")["Version"]) == "26.5", "SDK version differs")
    record["platform"] = {"system": platform.system(), "machine": platform.machine(), "version": platform.version()}
    record["sdk_settings"] = {str(sdk / "SDKSettings.json"): sha(sdk / "SDKSettings.json")}
    record["link_tools"] = {p: sha(p) for p in ("/usr/bin/ld", "/usr/bin/otool",
                            "/Library/Developer/CommandLineTools/usr/bin/ld", "/Library/Developer/CommandLineTools/usr/bin/otool")}
    record["runtime"] = {str(Path("/usr/local/lib") / n): h for n, h in tc["runtime_sha256_observed"].items() if "tsan" not in n} | EXTRA_RUNTIME
    for p, h in record["runtime"].items():
        require(sha(p) == h, "runtime identity differs")
    record["build_root"] = str(out)
    record["repo_root"] = str(repo)
    write(out / "prebuild-external.json", catalog)
    record["closures"] = {}
    record["dependency_aliases"] = {}
    record["binaries"] = {}
    for profile in ("timing", "diagnostic"):
        command, _ = execute(out, "compile-" + profile, compiler_argv(out, profile, repo), record)
        require(command["returncode"] == 0, "compile failed: " + profile)
        spellings = dep_paths(out / (profile + ".d"))
        record["dependency_aliases"][profile] = {str(p): str(p.resolve()) for p in spellings}
        dependencies = [p.resolve() for p in spellings]
        selected = out / "input" / profile
        for n in BASELINE:
            require(selected / n in dependencies, "selected profile header missing from actual closure")
            require(sha(selected / n) == (BASELINE if profile == "timing" else ACCEPTED)[n], "profile identity changed")
        require(not any(p.name in BASELINE and p.parent != selected for p in dependencies), "header shadowing in actual closure")
        has_runtime = selected / "n3_runtime.hpp" in dependencies
        require(has_runtime == (profile == "diagnostic"), "diagnostic runtime closure differs")
        closure = {}
        for p in dependencies:
            digest = sha(p)
            try:
                local = p.relative_to(out).as_posix()
            except ValueError:
                require(str(p) in catalog and catalog[str(p)] == digest, "new/changed unregistered external header: " + str(p))
                closure[str(p)] = digest
            else:
                closure["artifact:" + local] = digest
        record["closures"][profile] = closure
        record["binaries"][profile] = sha(out / ("owner_scaling_" + profile))
        execute(out, "linkage-" + profile, ["/usr/bin/otool", "-L", str(out / ("owner_scaling_" + profile))], record)
    for p, h in catalog.items():
        require(sha(p) == h, "external dependency changed during build: " + p)
    require(sha(compiler) == record["compiler"]["sha256"], "compiler changed during build")
    for p, h in {**record["sdk_settings"], **record["link_tools"], **record["runtime"]}.items():
        require(sha(p) == h, "toolchain changed during build")
    check_files(out, parent["source_files"])
    evidence_size(repo)
    finish(out, record)


def load_auditor(path):
    spec = importlib.util.spec_from_file_location("owner_auditor", Path(path) / "audit_owner_scaling.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def cases():
    result = {}
    for profile, requests in (("diagnostic", 256), ("timing", 256), ("timing", 8192)):
        for c, t in CONDITIONS:
            result[f"{profile}-r{requests}-o{c}-t{t}"] = (profile, c, t, requests, "none")
    for profile in ("diagnostic", "timing"):
        for control in ("partial_startup", "fail_create", "fail_allocate", "fail_verify"):
            c, t = (256, 4) if control == "partial_startup" else (8, 1)
            result[profile + "-" + control] = (profile, c, t, 256, control)
    return result


def audit_build(path):
    path = Path(path)
    r = basic_audit(path, "build")
    audit_inputs(path)
    parent = read(path / "prepared-result.json")
    require(parent.get("status") == "passed" and parent.get("kind") == "prepare", "prepared lineage differs")
    check_files(path, parent["source_files"])
    require(r.get("executed_runner_sha256") == sha(path / "common/run_owner_scaling.py"), "build runner identity differs")
    require(set(r["closures"]) == set(r["binaries"]) == {"timing", "diagnostic"}, "missing build profile")
    recorded_root, repo = Path(r["build_root"]), Path(r["repo_root"])
    require(recorded_root.is_absolute() and repo.is_absolute(), "build origins must be absolute")
    lock = read(path / "provenance/toolchain.json")
    tc = lock["profiles"]["macos-p2996"]
    compiler = r["compiler"]
    require(compiler["executed"] == "/usr/local/bin/clang++" and
            compiler["sha256"] == tc["compiler_binary_sha256_observed"] and
            tc["compiler_commit"] in compiler["version"] and "Target: " + tc["target"] in compiler["version"],
            "compiler profile differs")
    require(r["dependencies"] == lock["dependencies"], "dependency commits differ")
    require(r["runtime"] == ({"/usr/local/lib/" + n: h for n, h in tc["runtime_sha256_observed"].items() if "tsan" not in n} | EXTRA_RUNTIME),
            "runtime identity differs")
    require(r["platform"]["system"] == "Darwin" and r["platform"]["machine"] == "arm64", "host platform differs")
    catalog = read(path / "prebuild-external.json")
    inherited = read(path / "provenance/w2-build.json")["external_files"]
    require(all(catalog.get(p) == h for p, h in inherited.items()), "accepted external lineage differs")
    require(len(r["commands"]) == 5 and all(c.get("returncode") == 0 for c in r["commands"]), "build commands incomplete/failed")
    require(r["commands"][0]["argv"] == ["/usr/local/bin/clang++", "--version"], "compiler probe differs")
    for profile in r["binaries"]:
        require(sha(Path(path) / ("owner_scaling_" + profile)) == r["binaries"][profile], "binary identity differs")
        argv = compiler_argv(recorded_root, profile, repo)
        commands = [c for c in r["commands"] if c["argv"] == argv]
        require(len(commands) == 1 and commands[0]["timeout_seconds"] == 30, "compiler invocation differs")
        spellings = dep_paths(path / (profile + ".d"))
        aliases = r["dependency_aliases"][profile]
        require(set(aliases) == {str(p) for p in spellings}, "dependency alias inventory differs")
        for p in spellings:
            target = Path(aliases[str(p)])
            require(target.is_absolute() and ".." not in target.parts, "unsafe dependency alias")
            if p.is_relative_to(recorded_root):
                require(target == p, "local dependency alias escapes frozen artifact")
        dependencies = [Path(aliases[str(p)]) for p in spellings]
        expected_closure = {}
        for p in dependencies:
            try:
                local = p.relative_to(recorded_root).as_posix()
            except ValueError:
                require(str(p) in catalog, "external closure is absent from prebuild catalog")
                expected_closure[str(p)] = catalog[str(p)]
            else:
                require(".." not in Path(local).parts, "unsafe local dependency")
                expected_closure["artifact:" + local] = sha(path / local)
        require(r["closures"][profile] == expected_closure, "actual dependency closure differs")
        selected = recorded_root / "input" / profile
        for n in BASELINE:
            require(selected / n in dependencies, "required profile header missing from closure")
        require(not any(p.name in BASELINE and p.parent != selected for p in dependencies), "actual profile header shadowing")
        require((selected / "n3_runtime.hpp" in dependencies) == (profile == "diagnostic"), "runtime profile differs")
        for n, h in r["closures"][profile].items():
            if n.startswith("artifact:"):
                suffix = Path(n[9:])
                require(not suffix.is_absolute() and ".." not in suffix.parts, "unsafe artifact closure path")
                require(sha(Path(path) / n[9:]) == h, "local dependency identity differs")
    return r


def collect(args, out, record):
    repo = args.repo_root.resolve()
    record["coordination"] = coordinated(repo)
    canonical_attempts = repo / "build/vm-owner-scaling/attempts-d018"
    require(args.attempts.absolute() == canonical_attempts and not args.attempts.is_symlink(),
            "D018 requires its single canonical attempt ledger")
    evidence_size(repo, 2 * LOG_LIMIT)
    build_record = audit_build(args.build)
    require(sha(args.build / "common/run_owner_scaling.py") == sha(__file__), "execute frozen runner")
    selected = args.case_ids.split(",")
    require(1 <= len(selected) <= 4 and len(set(selected)) == len(selected) and set(selected) <= set(cases()), "select one to four unique frozen cases")
    record.update(build_result_sha256=sha(args.build / "result.json"), build_root=str(args.build.absolute()),
                  collection_root=str(out), cases={}, attempt_root=str(args.attempts.absolute()))
    args.attempts.mkdir(parents=True, exist_ok=True)
    auditor = load_auditor(args.build / "common")
    for p, h in build_record["runtime"].items():
        require(sha(p) == h, "runtime changed before collection")
    for name in selected:
        profile, c, t, requests, control = cases()[name]
        binary = args.build.absolute() / ("owner_scaling_" + profile)
        require(sha(binary) == build_record["binaries"][profile], "binary changed before execution")
        argv = [binary, "--owners", c, "--threads", t, "--requests", requests, "--control", control]
        evidence_size(repo, 2 * LOG_LIMIT)
        claim = {"case_id": name, "build_result_sha256": record["build_result_sha256"],
                 "collection_root": str(out), "argv": [str(x) for x in argv], "status": "claimed", "claimed_at": now()}
        # A consumed claim is never reused, including failed or interrupted attempts.
        with (args.attempts / (name + ".json")).open("x") as f:
            json.dump(claim, f, indent=2, sort_keys=True)
        command, _ = execute(out, name, argv, record)
        require(command["returncode"] == (0 if control == "none" else 2), "unexpected child exit: " + name)
        child = read(out / (name + ".stdout"))
        result = auditor.audit_record(child, expected_profile=profile, expected_condition=(c, t, requests), expected_control=control)
        record["cases"][name] = result
        claim.update(status="audited", returncode=command["returncode"], stdout_sha256=sha(out / (name + ".stdout")),
                     stderr_sha256=sha(out / (name + ".stderr")))
        write(args.attempts / (name + ".json"), claim)
        require(sha(binary) == build_record["binaries"][profile], "binary changed after execution")
    audit_build(args.build)
    for p, h in build_record["runtime"].items():
        require(sha(p) == h, "runtime changed during collection")
    record["aggregate_evidence_bytes_at_finish"] = evidence_size(repo)
    finish(out, record)


def audit(args):
    build_record = audit_build(args.build)
    auditor = load_auditor(args.build / "common")
    seen, reports = set(), {}
    queue_spec = importlib.util.spec_from_file_location("owner_queue_audit", args.build / "provenance/queue-runner.py")
    queue_auditor = importlib.util.module_from_spec(queue_spec)
    sys.modules[queue_spec.name] = queue_auditor
    queue_spec.loader.exec_module(queue_auditor)
    receipts = {}
    for receipt in args.receipts:
        q = queue_auditor.audit(receipt)
        require(q["job_status"] == "complete" and q["run_id"] not in receipts, "queue job failed/duplicate")
        receipts[q["run_id"]] = read(receipt / "request.json")
    artifacts = [build_record] + [basic_audit(g, "collect") for g in args.groups]
    require(len(receipts) == len(artifacts), "queue receipt count differs")
    for artifact in artifacts:
        co = artifact["coordination"]
        request = receipts.get(co["metadata"]["run_id"])
        require(request == co["request"] and co["runner_sha256"] == QUEUE_SHA and
                request["owner"] == "W1" and 0 < request["timeout_seconds"] <= 180 and
                request["runner_sha256"] == QUEUE_SHA and request["source_headers"] == BASELINE and
                request["lock_dir"] == str(Path(build_record["repo_root"]) / "build/vm-coordination"),
                "completed queue request differs from artifact")
    attempt_files = inventory(args.attempts)
    require(set(attempt_files) == {n + ".json" for n in cases()}, "missing or extra workload attempts")
    for group in args.groups:
        result = basic_audit(group, "collect")
        require(result["build_result_sha256"] == sha(args.build / "result.json"), "collection build identity differs")
        require(result["attempt_root"] == str(Path(build_record["repo_root"]) / "build/vm-owner-scaling/attempts-d018"),
                "collection did not use the canonical attempt ledger")
        coordination = result["coordination"]
        require(coordination["runner_sha256"] == QUEUE_SHA and coordination["metadata"]["owner"] == "W1" and
                coordination["metadata"]["status"] == "running" and
                coordination["metadata"]["supervisor_pid"] in coordination["ancestors"], "collection queue provenance differs")
        require(1 <= len(result["cases"]) <= 4 and len(result["commands"]) == len(result["cases"]), "group process count differs")
        for name, old in result["cases"].items():
            require(name in cases() and name not in seen, "unknown/duplicate validation case")
            seen.add(name)
            profile, c, t, requests, control = cases()[name]
            child = read(group / (name + ".stdout"))
            computed = auditor.audit_record(child, expected_profile=profile, expected_condition=(c, t, requests), expected_control=control)
            require(computed == old, "stored workload audit differs")
            argv = [str(Path(result["build_root"]) / ("owner_scaling_" + profile)), "--owners", str(c),
                    "--threads", str(t), "--requests", str(requests), "--control", control]
            matched = [x for x in result["commands"] if x["argv"] == argv]
            require(len(matched) == 1, "raw child invocation differs")
            command = matched[0]
            require(command["returncode"] == (0 if control == "none" else 2) and command["timeout_seconds"] == 30,
                    "wrong raw exit code/deadline")
            claim = read(args.attempts / (name + ".json"))
            require(claim["case_id"] == name and claim["status"] == "audited" and claim["argv"] == argv and
                    claim["build_result_sha256"] == result["build_result_sha256"] and
                    claim["collection_root"] == result["collection_root"] and claim["returncode"] == command["returncode"] and
                    claim["stdout_sha256"] == sha(group / (name + ".stdout")) and
                    claim["stderr_sha256"] == sha(group / (name + ".stderr")), "attempt claim differs")
            reports[name] = child
    require(seen == set(cases()), "missing validation cases")
    for requests in (256, 8192):
        checksums = {r["checksum"] for r in reports.values() if r["control"] == "none" and r["condition"]["requests"] == requests}
        require(len(checksums) == 1, "equivalent workload checksums differ")
    # Three bounded negative checks on owned in-memory copies, never raw evidence.
    sample = reports["diagnostic-r256-o8-t1"]
    negatives = {}
    bad = copy.deepcopy(sample)
    bad["condition"]["requests"] *= 2
    variants = {"wrong_request_total": bad}
    bad = copy.deepcopy(sample)
    events = bad["trace"]["events"]
    index = next(i for i, e in enumerate(events) if e["kind"] == "registry_lookup_hit")
    del events[index]
    for seq, e in enumerate(events, 1):
        e["sequence"] = seq
    variants["missing_lookup"] = bad
    bad = copy.deepcopy(sample)
    bad["owners"][0]["destroyed"] = False
    variants["residual_owner"] = bad
    for name, bad in variants.items():
        try:
            auditor.audit_record(bad, expected_profile="diagnostic", expected_condition=(8, 1, 256))
        except ValueError as e:
            negatives[name] = {"rejected": True, "reason": str(e)}
        else:
            raise ValueError("negative check accepted: " + name)
    return {"schema_version": 1, "status": "passed", "timing_evidence": False, "workload_processes": len(seen),
            "successful_processes": 18, "expected_failure_processes": 8, "negative_checks": negatives,
            "binaries": build_record["binaries"], "case_ids": sorted(seen)}


def main():
    global TASK_REPO
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for kind in ("prepare", "build", "collect"):
        p = sub.add_parser(kind)
        p.add_argument("--out", type=Path, required=True)
        p.add_argument("--repo-root", type=Path, default=Path.cwd())
        if kind == "build":
            p.add_argument("--prepared", type=Path, required=True)
        if kind == "collect":
            p.add_argument("--build", type=Path, required=True)
            p.add_argument("--attempts", type=Path, required=True)
            p.add_argument("--case-ids", required=True)
    p = sub.add_parser("audit")
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--attempts", type=Path, required=True)
    p.add_argument("--groups", type=Path, nargs="+", required=True)
    p.add_argument("--receipts", type=Path, nargs="+", required=True)
    args = parser.parse_args()
    if args.command == "audit":
        print(json.dumps(audit(args), sort_keys=True))
        return
    repo = args.repo_root.resolve()
    TASK_REPO = repo
    destination = args.out.resolve()
    require(any(destination.is_relative_to(repo / suffix) for suffix in ("build/vm-owner-scaling", "docs/validation/vm-owner-scaling")),
            "output must remain in D018 owned evidence paths")
    evidence_size(repo)
    out = fresh(destination)
    record = {"schema_version": 1, "kind": args.command, "status": "running", "started_at": now(),
              "executed_runner_sha256": sha(__file__), "timing_evidence": False}
    try:
        globals()[args.command](args, out, record)
    except Exception as e:
        record.update(status="failed", reason=type(e).__name__ + ": " + str(e), finished_at=now())
        write(out / "result.json", record)
        raise
    print(json.dumps({"status": "passed", "kind": args.command, "out": str(out)}))


if __name__ == "__main__":
    main()
