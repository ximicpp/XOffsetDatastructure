"""Freeze R1's one-compile codegen request; retain all M1 requests unchanged."""
import hashlib
import json
from pathlib import Path

r = Path(__file__).resolve().parents[3]
docs = r / "docs/coordination/c14-iteration-20261019-v1"
candidate = r / "build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1"
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
dest = "/root/xoffset-c14-iteration/20261019/v1/core/r1-canonical-hotcold-v1"
out = "/root/xoffset-c14-iteration/20261019/v1/validation/r1-v1/codegen"
old = json.loads((docs / "validation/M1_CODEGEN_RESULT.json").read_text(encoding="utf-8"))
d = json.loads((candidate / "IDENTITY.json").read_text(encoding="utf-8"))
argv = list(old["commands"][0]["argv"])
baseline_include = "-I/root/xoffset-receive-completion/20261019/v1/final-v4/source"
if argv.count(baseline_include) != 1 or argv[-2] != "-o":
    raise ValueError("unexpected original bare codegen command")
argv[argv.index(baseline_include)] = "-I" + dest + "/source"
argv[-1] = out + "/candidate-driver.s"
files = [candidate / "IDENTITY.json", candidate / "R1.patch",
    r / "tools/c14_iteration_20261019/core/stage_m1.py",
    r / "tools/c14_iteration_20261019/core/prepare_r1.py",
    Path(__file__), docs / "validation/M1_CODEGEN_RESULT.json"]
request = {"schema_version": 1, "status": "ready_after_REVIEW_R1_source_gate",
    "candidate": d["candidate"], "candidate_main_sha256": d["members"]["xoffsetdatastructure.hpp"]["candidate_sha256"],
    "candidate_patch_sha256": d["patch_sha256"], "independent_C14_derivative": True,
    "M1_combined": False, "M1_REQUESTS_overwritten": False,
    "executor": "validation-thread only under the existing actual global shared lease",
    "scope": "one new original-driver bare O3 DNDEBUG assembly compile; no native, tests or timing admitted",
    "reference_control": old["codegen"]["control"]["assembly"],
    "reference_binding": "Reuse the already verified identical C14 control/driver/vendor/compiler/profile assembly; verify its recorded SHA before comparison, do not rebuild it.",
    "source_review": "core/REVIEW_R1.md must find no blocker before the compile",
    "prepare_output_directory": out,
    "jobs": [{"id": "R1-STAGE-v1", "deadline_seconds": 60, "compile_budget": 0, "native_budget": 0,
              "argv": ["python3", "-B", w + "/tools/c14_iteration_20261019/core/stage_m1.py",
                       "--candidate", w + "/build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1", "--out", dest]},
             {"id": "R1-CODEGEN-v1", "depends_on": ["R1-STAGE-v1", "source review passed"],
              "deadline_seconds": 240, "compile_budget": 1, "native_budget": 0, "argv": argv,
              "expected": [out + "/candidate-driver.s", "compile argv/exit/log/compiler/source/profile identities"],
              "gate": "Actual query_view must inline the acquire count load/zero branch and avoid entering canonical cold prologue on inactive path, with no hoisting across public calls. Active cold body and callbacks must remain semantically intact. If no actual call/prologue savings: reject R1 without timing."}],
    "observed_control": {"assembly_sha256": old["codegen"]["control"]["assembly"]["sha256"],
        "canonical_definition_1based": 19278, "pre_load_pushes": 5, "count_load_1based": 19299,
        "zero_branch_1based": 19301, "query_view_start_1based": 46902,
        "query_call_examples_1based": [47036, 47057, 47069, 47136]},
    "input_bindings": {str(p.relative_to(r)).replace("\\", "/"): {
        "bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files},
    "following_requests": "Only after mechanism admission: targeted canonical-zero/cold-no-callback/registry/owner normal+SAN, one fixed-M control/candidate original trajectory with actual active/inactive hit accounting, then complete applicable business/V1 gates and separately frozen screen. None authorized by this codegen request."}
target = docs / "core/REQUESTS_R1_CODEGEN.json"
if target.exists():
    raise ValueError("R1 request already frozen; do not overwrite an executed registration")
target.write_text(json.dumps(request, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(json.dumps({"status": request["status"], "request": str(target), "compiles": 1, "native": 0}))
