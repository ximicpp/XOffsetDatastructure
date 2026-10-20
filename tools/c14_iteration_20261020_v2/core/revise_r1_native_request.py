"""Preserve unexecuted v1 and bind accurate TLS coverage wording before B."""
import hashlib
import json
from pathlib import Path
r = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
docs = r / "docs/coordination/c14-iteration-20261020-v2/core"
w = "/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发"
oldtest = here / "test_r1_thread_handoff.cpp"
newtest = here / "test_r1_thread_handoff_v2.cpp"
if newtest.exists():
    raise ValueError("new revision required")
text = oldtest.read_text(encoding="utf-8").replace(
    "// Repetition exercises a cold thread's first lookup and its warmed TLS.",
    "// Repeated reads check stable domain results. Actual cold/hot TLS\n"
    "    // coverage is supplied by the retained canonical fixture programs;\n"
    "    // cold canonical lookup does not itself install a TLS cache entry.").replace(
    "zero/active/zero, cold TLS, handoff", "zero/active/zero, repeated domain reads, handoff")
newtest.write_text(text, encoding="utf-8")
oldq = docs / "REQUESTS_R1_NATIVE.json"
q = json.loads(oldq.read_text(encoding="utf-8"))
oldname = w + "/" + oldtest.relative_to(r).as_posix()
newname = w + "/" + newtest.relative_to(r).as_posix()
q["output"] = q["output"].replace("r1-native-v1", "r1-native-v2")
for job in q["commands"]:
    job["argv"] = [x.replace(oldname, newname).replace("r1-native-v1", "r1-native-v2") for x in job["argv"]]
    if "thread_handoff" in job["label"]:
        job["label"] += "-v2"
def identity(p):
    return {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
q["input_files"][newname] = identity(newtest)
q["input_files"][w + "/" + oldq.relative_to(r).as_posix()] = identity(oldq)
q["input_files"][w + "/" + Path(__file__).relative_to(r).as_posix()] = identity(Path(__file__))
q["revision"] = "v2 wording-only correction before first native: repeated reads do not prove warm TLS. The four retained canonical programs supply actual cold/hot coverage; all handoff assertions unchanged. v1 preserved and not a separate execution budget."
newq = docs / "REQUESTS_R1_NATIVE_v2.json"
q["worker_argv"][-1] = w + "/" + newq.relative_to(r).as_posix()
with newq.open("x", encoding="utf-8") as f:
    json.dump(q, f, indent=2, ensure_ascii=False); f.write("\n")
ready = json.loads((docs / "READY.json").read_text(encoding="utf-8"))
ready["request"] = newq.name
ready["superseded_unexecuted_request"] = oldq.name
ready["R1_revision"] = q["revision"]
(docs / "READY.json").write_text(json.dumps(ready, indent=2) + "\n", encoding="utf-8")
print("R1_NATIVE_v2 registered; v1 preserved, shared total B budget remains 10 native runs")
