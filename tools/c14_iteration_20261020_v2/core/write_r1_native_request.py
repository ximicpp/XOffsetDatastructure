"""Bind the frozen R1 and ten finite correctness executions in new v2 output."""
import hashlib
import json
from pathlib import Path

r = Path(__file__).resolve().parents[3]
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
old = r / "docs/coordination/c14-iteration-20261019-v1"
doc = r / "docs/coordination/c14-iteration-20261020-v2/core"
source = "/root/xoffset-c14-iteration/20261019/v1/validation/r1-v1/source"
out = "/root/xoffset-c14-iteration/20261020/v2/validation/r1-native-v1"
identity_file = r / "build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json"
identity = json.loads(identity_file.read_text(encoding="utf-8"))
prior = json.loads((old / "validation/M1_CORRECTNESS_RESULT.json").read_text(encoding="utf-8"))
s0 = json.loads((old / "validation/S0_RESULT.json").read_text(encoding="utf-8"))
def record(path):
    return {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
commands = []
tests = [source + "/tests/" + n + ".cpp" for n in (
    "test_canonical_zero_target", "test_canonical_cold_no_callback", "test_canonical_registry_v2", "test_owner_identity")]
tests += [w + "/tools/c14_iteration_20261020_v2/core/test_r1_thread_handoff.cpp"]
for profile in ("normal", "san"):
    case = next(c for c in prior["cases"] if c["name"] == profile + "-test_scalar_resize")
    prefix = case["compile"]["argv"][:-3]
    prior_source = "/root/xoffset-c14-iteration/20261019/v1/validation/m1-v2/staged/source"
    prefix = [x.replace(prior_source, source) for x in prefix]
    for test in tests:
        name = profile + "-" + Path(test).stem
        binary = out + "/" + name
        commands += [{"label": name + "-compile", "kind": "compile", "timeout_seconds": 120,
                      "argv": prefix + [test, "-o", binary]},
                     {"label": name + "-run", "kind": "native", "timeout_seconds": 20, "argv": [binary]}]
files = [identity_file, old / "validation/R1_CODEGEN_RESULT.json", old / "validation/S0_RESULT.json",
         r / "tools/c14_iteration_20261020_v2/core/run_native_targets.py",
         r / "tools/c14_iteration_20261020_v2/core/test_r1_thread_handoff.cpp", Path(__file__)]
inputs = {w + "/" + f.relative_to(r).as_posix(): record(f) for f in files}
inputs.update({p: {"sha256": v["actual"]} for p, v in s0["toolchain"]["files"].items()})
san = "/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/sanitizer-resource/"
inputs.update({san + p: v for p, v in s0["toolchain"]["sanitizer_files"].items()})
q = {"schema_version": 1, "status": "ready_for_validation_review", "stage": "B_R1_targeted",
     "candidate": identity["candidate"], "source": source, "source_members": identity["members"],
     "production_main_sha256": identity["members"]["xoffsetdatastructure.hpp"]["candidate_sha256"],
     "candidate_modified": False, "baseline_modified": False, "codegen_recompiled": False,
     "runtime_binding": "Same retained P2996/libc++/SAN tool/runtime hashes; current phase A must confirm unchanged",
     "global_lease_file": "/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/build/vm-coordination/queue.lock",
     "output": out, "task_timeout_seconds": 1500, "compile_budget": 10, "native_budget": 10,
     "commands": commands, "input_files": inputs, "timing_admitted": False, "screening_admitted": False,
     "review_gate": "core/REVIEW_R1_NATIVE.md plus retained REVIEW_R1.md; validation inspects final request/runtime/input identities",
     "worker_argv": ["python3", "-I", "-B", w + "/tools/c14_iteration_20261020_v2/core/run_native_targets.py",
                     "--request", w + "/docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_R1_NATIVE.json"],
     "coverage": "Four retained canonical/owner programs plus a synchronized real-C/ordinary copy-load thread handoff program, each normal and SAN. Every payload exclusive or synchronized; no concurrent mapping retirement.",
     "next": "Phase C joint request must be separately bound/reviewed before the two diagnostic runs; full candidate gates and independent screen freeze remain pending."}
doc.mkdir(parents=True, exist_ok=True)
with (doc / "REQUESTS_R1_NATIVE.json").open("x", encoding="utf-8") as f:
    json.dump(q, f, indent=2, ensure_ascii=False); f.write("\n")
with (doc / "READY.json").open("w", encoding="utf-8") as f:
    json.dump({"status": "R1_NATIVE_request_ready_for_validation_review", "request": "REQUESTS_R1_NATIVE.json",
               "candidate_unchanged": True, "core_native_executed": 0, "timing_admitted": False,
               "M2": "independent C14 candidate implementation in progress",
               "phase_C": "joint instrumentation request in preparation; no diagnostic run admitted yet"}, f, indent=2)
    f.write("\n")
print("R1 native request ready: 10 compiles, 10 finite native executions; no codegen, diagnostic or timing execution by core")
