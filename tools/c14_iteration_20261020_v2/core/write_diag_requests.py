"""Concrete phase-C/M2 diagnostic proposal; final monitor closure must bind first."""
import copy
import hashlib
import json
from pathlib import Path
r = Path(__file__).resolve().parents[3]
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
docs = r / "docs/coordination/c14-iteration-20261020-v2/core"
prior = json.loads((r / "docs/coordination/c14-iteration-20261019-v1/validation/M1_CODEGEN_RESULT.json").read_text(encoding="utf-8"))
native = json.loads((docs / "REQUESTS_R1_NATIVE_v2.json").read_text(encoding="utf-8"))
original_exp = w + "/handoff/continuation-20261019-v1/experiments/serialization_comparison"
inputdir = w + "/handoff/continuation-20261019-v1/portable-inputs"
output = "/root/xoffset-c14-iteration/20261020/v2/validation/common-diag-v2"
def fid(p): return {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
files = [Path(__file__), r / "tools/c14_iteration_20261020_v2/core/common_diag.hpp",
         r / "tools/c14_iteration_20261020_v2/core/prepare_common_diag.py"]
role_jobs = {}
for role in ("c14", "r1", "m2"):
    prepared = r / ("build/c14-iteration-20261020-v2/diag-prepared/" + role + "-v2")
    files += [prepared / "DIAGNOSTIC_IDENTITY.json"]
    identity = json.loads((prepared / "DIAGNOSTIC_IDENTITY.json").read_text(encoding="utf-8"))
    files += [prepared / Path(p) for p in identity["generated"]]
    final = output + "/" + role
    argv = list(prior["commands"][0]["argv"])
    argv = [x for x in argv if x != "-S"]
    argv[argv.index("-I/root/xoffset-receive-completion/20261019/v1/final-v4/source")] = "-I" + final + "/source"
    argv.insert(argv.index("-I" + original_exp), "-I" + final + "/experiment")
    argv[argv.index(original_exp + "/driver.cpp")] = final + "/experiment/driver.cpp"
    argv[-1] = final + "/diagnostic"
    run = [final + "/diagnostic", "run", "--wire", inputdir + "/old-V8-initial-wire.bin",
           "--trace", inputdir + "/fixed-M/trace.bin", "--wire-out", final + "/final-wire.bin",
           "--canonical-out", final + "/final-canonical.bin", "--queries-out", final + "/queries.bin",
           "--result", final + "/native.json"]
    role_jobs[role] = {"prepared_source": w + "/" + prepared.relative_to(r).as_posix(),
                      "prepared_identity": identity, "final_validation_source": final,
                      "compile_argv": argv, "compile_timeout_seconds": 120,
                      "native_argv": run, "native_timeout_seconds": 20}
files += [r / "handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin",
          r / "handoff/continuation-20261019-v1/portable-inputs/fixed-M/trace.bin",
          r / "handoff/continuation-20261019-v1/portable-inputs/fixed-M/fixture.json"]
bindings = {w + "/" + p.relative_to(r).as_posix(): fid(p) for p in files}
q = {"schema_version": 1, "status": "joint_proposal_not_runnable_until_validation_closure_bound",
    "stage": "C_first_original_run_and_common_mechanism", "role_order": ["c14", "r1"],
    "role_jobs": {k: role_jobs[k] for k in ("c14", "r1")},
    "compile_budget": 2, "native_budget": 2, "task_timeout_seconds": 600,
    "each_child_deadline_seconds": 20, "each_compile_deadline_seconds": 120,
    "body_limit_bytes": 128*1024*1024, "log_limit_bytes": 1024*1024,
    "retries": 0, "warmups": 0, "input_files": bindings,
    "runtime_files": {p: v for p, v in native["input_files"].items() if not p.startswith(w)},
    "global_lease_file": native["global_lease_file"],
    "M2_control_predeclaration": {"control_role": "c14", "declared_before_any_C_run": True,
        "observable_hook_sha256": fid(r / "tools/c14_iteration_20261020_v2/core/common_diag.hpp")["sha256"],
        "metrics": ["owns", "vectors", "capacity_hits", "nontrivial_misses", "nonempty_misses", "vector_data_reads"],
        "conditions": "Exact final driver/adapter/observer/profile/input/audit shared; future M2 candidate only. No stitching if validation changes the hook/profile between roles."},
    "emit_sample_requirements": {"invocations": 1, "seen_mask_required": 511,
        "sample_indices": {"0": "enter/normalization begins", "1": "normalized root/allocator done, payload known",
             "2": "header ready", "3": "string initialized/checks complete, before resize_and_overwrite",
             "4": "callback entered, string preparation complete", "5": "header copied", "6": "payload copied",
             "7": "resize_and_overwrite returned", "8": "save_verified returns"},
        "samples": "all clock_status/rusage_status==0; ns nonzero monotone; rusage cumulative valid. Preparation includes allocator/string/observer work; never call it pure malloc. Fault deltas overlap copy/preparation intervals, never add twice.",
        "flush": "validation flushes fixed POD only after emit has returned; no logging/string/new/prefault/output reserve inside emit hook"},
    "validation_must_complete_before_final_freeze": [
        "10-case R1 B gate passed, current A/runtime/lease/backing/resource evidence",
        "Final bounded live milestone channel/pipe and single-reaper wait4/Popen wrapper, exactly original supervisor session",
        "At most64 durable external events for main/read/decode/hash/t0..t8/post-audit/export/hash/write boundaries; nonblocking bounded channel; abnormal exit leaves final event evidence",
        "Final driver emits buffered counts/emit POD outside emit and post-primary validates all9 stage canonical digests/full query truth without any pre-t0 validation/export/owning warmup",
        "Review actual fully assembled driver/header/adapter/wrapper hashes, wrapper real-error selftests and budget; declare the exact same final hook for future M2 before C14 control starts",
        "Stop remaining C role on first identity/truth/deadline/resource/tool failure; no third diagnostic process and no speed inference"],
    "timing_admitted": False, "screening_admitted": False, "core_native_executed": 0}
with (docs / "REQUESTS_PHASE_C_DRAFT.json").open("x", encoding="utf-8") as f:
    json.dump(q, f, indent=2, ensure_ascii=False); f.write("\n")
m2 = {"schema_version": 1, "status": "conditional_not_admitted_until_common_C_control_and_M2_gates_bound",
    "stage": "D_M2_common_mechanism", "candidate": "m2-exact-owns-v1",
    "role_job": role_jobs["m2"], "candidate_compiles": 1, "native_budget": 1,
    "control_new_compiles_or_runs": 0, "input_files": bindings,
    "common_control": q["M2_control_predeclaration"],
    "conditions": ["candidate bare4-to-1 codegen and targeted normal/SAN passed",
        "phase C final request already declared its sole C14 child as common M2 control before execution",
        "same final instrumentation/profile/driver/audit identity; record actual C14 control/binary/receipt hashes before M2 run",
        "M2 positive eligible nodes and unchanged branch/owns truth; exact original run and complete post-primary audit",
        "if phase C binding is unavailable or differs, this request cannot run; independently register control+candidate budget, do not silently substitute"],
    "timeout_seconds": 300, "native_timeout_seconds": 20, "retry": 0,
    "timing_admitted": False, "screening_admitted": False}
with (docs / "REQUESTS_M2_MECHANISM_DRAFT.json").open("x", encoding="utf-8") as f:
    json.dump(m2, f, indent=2, ensure_ascii=False); f.write("\n")
ready = json.loads((docs / "READY.json").read_text(encoding="utf-8"))
ready["phase_C"] = "REQUESTS_PHASE_C_DRAFT.json: common hook/source prepared+review, final external milestones/flush/audit/wrapper binding required from validation before any run"
ready["M2_mechanism"] = "REQUESTS_M2_MECHANISM_DRAFT.json: executable candidate command proposed, conditional one candidate child with exact predeclared C14 common control"
(docs / "READY.json").write_text(json.dumps(ready, indent=2) + "\n", encoding="utf-8")
print("Phase C joint proposal and M2 conditional mechanism commands bound; neither admits native execution yet")
