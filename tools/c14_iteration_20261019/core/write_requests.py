"""Write concrete bounded jobs; this does not execute any validation."""
import hashlib
import json
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
core = repo / "docs/coordination/c14-iteration-20261019-v1/core"
candidate = repo / "build/c14-iteration-20261019-v1/candidates/m1-default-scalar-v2"
base = "/root/xoffset-receive-completion/20261019/v1/final-v4/source"
dest = "/root/xoffset-c14-iteration/20261019/v1/core/m1-default-scalar-v2"
val = "/root/xoffset-c14-iteration/20261019/v1/validation/m1-v2"
tool = w + "/tools/c14_iteration_20261019/core"
exp = w + "/handoff/continuation-20261019-v1/experiments/serialization_comparison"
inputs = w + "/handoff/continuation-20261019-v1/portable-inputs"
compiler = "/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx"
san = "/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/sanitizer-resource"
identity = json.loads((candidate / "IDENTITY.json").read_text(encoding="utf-8-sig"))
common = ["python3", "-B", tool + "/run_m1.py", "--source", dest + "/source",
          "--baseline", base, "--vendor", base, "--experiment", exp, "--compiler", compiler]
jobs = [
    {"id": "M1-STAGE-v2", "depends_on": ["S0 passed"], "deadline_seconds": 60,
     "argv": ["python3", "-B", tool + "/stage_m1.py", "--candidate",
              w + "/build/c14-iteration-20261019-v1/candidates/m1-default-scalar-v2",
              "--out", dest], "native_budget": 0, "compile_budget": 0,
     "expected": [dest + "/IDENTITY.json", dest + "/source (154 exact members)"]},
    {"id": "M1-CODEGEN-v2", "depends_on": ["M1-STAGE-v2"], "deadline_seconds": 600,
     "argv": common + ["--mode", "codegen", "--out", val + "/codegen"],
     "native_budget": 0, "compile_budget": 2,
     "expected": [val + "/codegen/control-driver.s", val + "/codegen/candidate-driver.s",
                  val + "/codegen/INPUTS.json", val + "/codegen/commands.json"],
     "manual_gate": "Independent actual fresh scalar loop call-closure review. If original code already batches or binding repetition is absent: reject M1, stop all following jobs; no timing."},
    {"id": "M1-CORRECTNESS-v2", "depends_on": ["M1-CODEGEN-v2 independent passed"],
     "deadline_seconds": 1800,
     "argv": common + ["--mode", "correctness", "--out", val + "/correctness",
              "--san-resource", san, "--mechanism-admission", val + "/CODEGEN_ADMISSION.json"],
     "native_budget": 8, "compile_budget": 8,
     "tests_each_profile": ["test_scalar_resize", "test_custom_future_lifetime", "test_core_lifetime", "test_m1_scalar_values"],
     "expected": [val + "/correctness/INPUTS.json", val + "/correctness/commands.json", "8 run logs and 8 compile logs"]},
    {"id": "M1-MECHANISM-v2", "depends_on": ["M1-CODEGEN-v2 independent passed", "M1-CORRECTNESS-v2 passed"],
     "deadline_seconds": 600,
     "argv": common + ["--mode", "mechanism", "--out", val + "/mechanism",
              "--wire", inputs + "/old-V8-initial-wire.bin", "--trace", inputs + "/fixed-M/trace.bin",
              "--mechanism-admission", val + "/CODEGEN_ADMISSION.json"],
     "native_budget": 2, "compile_budget": 2,
     "expected": ["one fixed-M validate process/control, one/candidate; each 9 canonical stage digests, both query answers, original owning emit and validated reload",
                  "9 rows/control and 9 rows/candidate logical fresh, element, binding, bulk counters",
                  "diagnostic source/driver/binary identity and commands.json"],
     "checks": ["same 9-stage canonical digest array and fixed-M before/after queries",
                "same scalar entries/capacity/tail/fresh/elements by stage",
                "control binding_evaluations == fresh elements, candidate binding_evaluations == 0 and bulk_elements == control fresh elements for default demo scalar",
                "logical counts do not establish CPU cost or speed"]}
]
files = list((repo / "tools/c14_iteration_20261019/core").glob("*"))
files += [candidate / "IDENTITY.json", candidate / "M1.patch",
          repo / "handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin",
          repo / "handoff/continuation-20261019-v1/portable-inputs/fixed-M/trace.bin",
          repo / "handoff/continuation-20261019-v1/experiments/serialization_comparison/driver.cpp",
          repo / "handoff/continuation-20261019-v1/experiments/serialization_comparison/adapters/xoffset/adapter.hpp"]
bindings = {str(f.relative_to(repo)).replace("\\", "/"):
            {"sha256": hashlib.sha256(f.read_bytes()).hexdigest(), "bytes": f.stat().st_size}
            for f in files if f.is_file()}
d = {"schema_version": 1, "status": "ready_for_orchestrator_forwarding_not_executed",
     "candidate": identity["candidate"], "source_identity": "M1_IDENTITY.json",
     "main_sha256": identity["members"]["xoffsetdatastructure.hpp"]["candidate_sha256"],
     "patch_sha256": identity["patch_sha256"],
     "review": "REVIEW_M1.md v2 addendum; no blocking production-source defect",
     "executor": "validation-thread 01a15479-36f3-72d3-8c17-6630a51f854b only",
     "lease": "existing global run_job.py lock at historical root/build/vm-coordination/queue.lock; orchestrator must route and validation must verify actual lease",
     "s0_required": True, "protocol_frozen_for_screen": False,
     "native_executed_by_core": 0, "timing_admitted": False,
     "admission_json_required": {"status": "passed", "candidate_main_sha256": identity["members"]["xoffsetdatastructure.hpp"]["candidate_sha256"],
                                 "additional": "validation reviewer must include original/candidate machine code evidence and source/runtime binding"},
     "revision_note": "v1 retained unexecuted; v2 explicitly tests !future_custom_lifetime_supported before the two disabled frontier flags as required by amended PLAN.",
     "input_bindings": bindings, "jobs": jobs,
     "remaining_before_any_screen": "Applicable full/business/SML/V1 bidirectional/consumer gates, source/runtime inventory and a separately frozen screening protocol; this request admits none of S3/Q/F."}
core.mkdir(parents=True, exist_ok=True)
(core / "REQUESTS.json").write_text(json.dumps(d, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(json.dumps({"status": d["status"], "jobs": [j["id"] for j in jobs], "timing_admitted": False}))
