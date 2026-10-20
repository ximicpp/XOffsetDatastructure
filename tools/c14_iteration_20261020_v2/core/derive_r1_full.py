"""Exact path/identity derivation of existing full/business/consumer gates.

Authoring only: no import of workers, compiler, native child, lease or subprocess.
Original v1 code/fixtures/manifests are never edited.
"""
import ast
import difflib
import hashlib
import json
from pathlib import Path
import re

r = Path(__file__).resolve().parents[3]
prior = r / "tools/c14_iteration_20261019/validation"
out = Path(__file__).resolve().parent / "r1_full"
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
if out.exists(): raise ValueError("new tool derivation directory required")
out.mkdir()
def fid(p):
    b = p.read_bytes(); return {"bytes": len(b), "sha256": hashlib.sha256(b).hexdigest()}
replacements = [
    ("docs/coordination/c14-iteration-20261019-v1/core/REQUESTS.json", "docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_R1_FULL.json"),
    ("docs/coordination/c14-iteration-20261019-v1/validation", "docs/coordination/c14-iteration-20261020-v2/validation/r1_full"),
    ("xoffset-c14-m1-default-scalar-v2", "xoffset-c14-r1-canonical-hotcold-v1"),
    ("m1-default-scalar-v2", "r1-canonical-hotcold-v1"),
    ("m1-full-v2-", "r1-full-v1-"),
    ("m1-business-matrix-v2", "r1-business-matrix-v1"),
    ("m1-business-prepare-v2", "r1-business-prepare-v1"),
    ("m1-business-v2", "r1-business-v1"),
    ("/root/xoffset-c14-iteration/20261019/v1/validation", "/root/xoffset-c14-iteration/20261020/v2/validation"),
    ("M1_CODEGEN_RESULT.json", "R1_FULL_SOURCE_BINDING.json"),
    ("M1", "R1"), ("c14-m1-", "c14-r1-")]
records = {}; patch = []
for folder in ("business", "business_matrix"):
    old = prior / folder
    manifest = json.loads((old / "TOOL_MANIFEST.json").read_text(encoding="utf-8"))
    for n, expected in manifest["files"].items():
        if fid(old / n) != expected: raise ValueError("retained tool identity differs: " + folder + "/" + n)
    for p in sorted(old.rglob("*")):
        if not p.is_file() or p.name in ("TOOL_MANIFEST.json", "derivation.json", "DERIVATION.json", "derive.py"):
            continue
        if "__pycache__" in p.parts: continue
        rel = p.relative_to(prior).as_posix(); target = out / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if p.suffix == ".py":
            original = p.read_text(encoding="utf-8-sig"); text = original
            for before, after in replacements: text = text.replace(before, after)
            if rel in ("business/dispatch.py", "business/workers/common.py", "business_matrix/common.py"):
                text, count = re.subn(r"(?m)^ROOT\s*=.*$", "ROOT=Path(" + repr(w) + ")", text)
                if count != 1: raise ValueError("expected one explicit root assignment: " + rel)
            ast.parse(text)
            target.write_text(text, encoding="utf-8", newline="\n")
            if text != original:
                patch.extend(difflib.unified_diff(original.splitlines(True), text.splitlines(True), fromfile="v1/"+rel, tofile="r1/"+rel))
        else:
            target.write_bytes(p.read_bytes())
        records[rel] = {"original": str(p), "before": fid(p), "after": fid(target)}
# Preserve the existing generic full CMake/CTest implementation; only resolve
# its current root and documentation/output locations for this deeper tool path.
p = prior / "candidate_job.py"; original = p.read_text(encoding="utf-8"); text = original
text = text.replace("ROOT = Path(__file__).resolve().parents[3]", "ROOT = Path(" + repr(w) + ")")
text = text.replace("/root/xoffset-c14-iteration/20261019/v1/validation", "/root/xoffset-c14-iteration/20261020/v2/validation")
text = text.replace("docs/coordination/c14-iteration-20261019-v1/validation", "docs/coordination/c14-iteration-20261020-v2/validation/r1_full")
ast.parse(text)
(out / "full_entry.py").write_text(text, encoding="utf-8", newline="\n")
records["full_entry.py"] = {"original": str(p), "before": fid(p), "after": fid(out / "full_entry.py")}
patch.extend(difflib.unified_diff(original.splitlines(True), text.splitlines(True), fromfile="v1/candidate_job.py", tofile="r1/full_entry.py"))
for folder in ("business", "business_matrix"):
    base = out / folder
    entries = {p.relative_to(base).as_posix(): fid(p) for p in sorted(base.rglob("*")) if p.is_file()}
    (base / "TOOL_MANIFEST.json").write_text(json.dumps({"schema_version":1,"files":entries}, indent=2)+"\n", encoding="utf-8")
(out / "DERIVATION.json").write_text(json.dumps({"source_only": True, "files": records,
    "changes": "Literal R1 candidate/kind/receipt labels, exact current source/document/output paths and explicit root for deeper tool location. No model/oracle/schedule/assertion/compiler/profile/timeout/consumer logic changed.",
    "replacements": replacements, "native_executed": 0}, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
(out / "TOOLS.patch").write_text("".join(patch), encoding="utf-8")
print(json.dumps({"derived_files":len(records),"output":str(out),"native_executed":0}))
