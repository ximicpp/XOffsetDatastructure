"""M2 candidate-only codegen and finite targeted correctness registration."""
import copy
import hashlib
import json
from pathlib import Path
r = Path(__file__).resolve().parents[3]
doc = r / "docs/coordination/c14-iteration-20261020-v2/core"
old = r / "docs/coordination/c14-iteration-20261019-v1"
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
candidate = r / "build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1"
d = json.loads((candidate / "IDENTITY.json").read_text(encoding="utf-8"))
source = "/root/xoffset-c14-iteration/20261020/v2/validation/m2-v1/source"
out = "/root/xoffset-c14-iteration/20261020/v2/validation/m2-native-v1"
q = json.loads((doc / "REQUESTS_R1_NATIVE_v2.json").read_text(encoding="utf-8"))
q.update(candidate=d["candidate"], stage="D_M2_targeted", source=source, source_members=d["members"],
         output=out, production_main_sha256=d["members"]["xoffsetdatastructure.hpp"]["candidate_sha256"],
         candidate_modified=True, codegen_recompiled=False, compile_budget=8, native_budget=8,
         review_gate="core/REVIEW_M2.md no blocker; M2 candidate codegen must demonstrate 4-to-1 before the fixed-M observation or any screening")
q.pop("revision", None)
q["coverage"] = "same_arena_move, locator_soundness, owner_identity and focused exact-owns test, each normal and SAN; preserve original alias/OOM/callback controls"
q["commands"] = []
prior = json.loads((old / "validation/M1_CORRECTNESS_RESULT.json").read_text(encoding="utf-8"))
tests = [source + "/tests/" + n + ".cpp" for n in ("test_same_arena_move", "test_locator_soundness", "test_owner_identity")]
tests += [w + "/tools/c14_iteration_20261020_v2/core/test_m2_owns.cpp"]
for profile in ("normal", "san"):
    prefix = next(c for c in prior["cases"] if c["name"] == profile + "-test_scalar_resize")["compile"]["argv"][:-3]
    prefix = [x.replace("/root/xoffset-c14-iteration/20261019/v1/validation/m1-v2/staged/source", source) for x in prefix]
    for test in tests:
        label = profile + "-" + Path(test).stem; binary = out + "/" + label
        q["commands"] += [{"label": label + "-compile", "kind": "compile", "timeout_seconds": 120, "argv": prefix + [test, "-o", binary]},
                          {"label": label + "-run", "kind": "native", "timeout_seconds": 20, "argv": [binary]}]
def fid(p):
    return {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
files = [candidate / "IDENTITY.json", candidate / "M2.patch", Path(__file__),
         r / "tools/c14_iteration_20261020_v2/core/test_m2_owns.cpp",
         r / "tools/c14_iteration_20261020_v2/core/stage_source.py"]
q["input_files"].update({w + "/" + p.relative_to(r).as_posix(): fid(p) for p in files})
q["worker_argv"][-1] = w + "/docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_M2_NATIVE.json"
q["next"] = "No M2 screen until actual mechanism and full candidate gates; mechanism registration below is conditional and not admitted by these correctness commands."
with (doc / "REQUESTS_M2_NATIVE.json").open("x", encoding="utf-8") as f:
    json.dump(q, f, indent=2, ensure_ascii=False); f.write("\n")
cg = json.loads((old / "validation/M1_CODEGEN_RESULT.json").read_text(encoding="utf-8"))
argv = list(cg["commands"][0]["argv"])
argv[argv.index("-I/root/xoffset-receive-completion/20261019/v1/final-v4/source")] = "-I" + source
argv[-1] = "/root/xoffset-c14-iteration/20261020/v2/validation/m2-codegen-v1/candidate-driver.s"
request = {"status": "ready_for_validation_review", "candidate": d["candidate"], "stage": "D_M2_codegen",
    "source": source, "candidate_identity": fid(candidate / "IDENTITY.json"), "members": d["members"],
    "reference_control": cg["codegen"]["control"]["assembly"], "control_compiles": 0,
    "candidate_compiles": 1, "native_budget": 0, "timing_admitted": False,
    "source_review": "REVIEW_M2.md actual patch addendum", "task_timeout_seconds": 240,
    "stage_argv": ["python3", "-I", "-B", w + "/tools/c14_iteration_20261020_v2/core/stage_source.py",
                   "--candidate", w + "/build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1", "--out", source],
    "compile_argv": argv, "compile_timeout_seconds": 120,
    "input_files": q["input_files"],
    "mechanism_gate": "Inspect actual Attr owning-vector containment miss closure: old at most4 data/canonical resolutions -> candidate1. Preserve capacity hit and original size-bounded recursion; reject if no reduction or extra hidden work cancels mechanism.",
    "mechanism_registration": {"status": "conditional_not_admitted",
        "preferred_control": "predeclare C-phase C14 original run with the exact common canonical/owns/emit hook as M2 control before it runs",
        "candidate_only_future_native_budget": 1,
        "fallback": "If phase-C hooks/profile/truth not exactly shared, register separate bounded control+candidate mechanism request; never stitch post hoc or rerun C under another name.",
        "candidate_run_argv_shape": ["m2-exact-owns-v1-common-diag", "run", "--wire", "same fixed M wire", "--trace", "same fixed M trace", "--wire-out", "new M2 wire", "--canonical-out", "new M2 canonical", "--queries-out", "new M2 queries", "--result", "new M2 native.json"],
        "gate": "same full original trajectory and post-audit, actual eligible nonempty-miss nodes>0; same owns results/branch counts and canonical/query truth; vector data reads fall per eligible node; no time inference"}}
with (doc / "REQUESTS_M2_CODEGEN.json").open("x", encoding="utf-8") as f:
    json.dump(request, f, indent=2, ensure_ascii=False); f.write("\n")
ready = json.loads((doc / "READY.json").read_text(encoding="utf-8"))
ready["M2"] = "independent C14 candidate ready: REVIEW_M2.md, M2_IDENTITY.json, REQUESTS_M2_CODEGEN.json, REQUESTS_M2_NATIVE.json"
(doc / "READY.json").write_text(json.dumps(ready, indent=2) + "\n", encoding="utf-8")
print("M2 ready: candidate-only codegen1, targeted normal/SAN8; no mechanism execution admitted yet")
