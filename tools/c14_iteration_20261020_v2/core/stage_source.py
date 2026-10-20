"""Stage hash-exact candidate members into a NEW validation source directory."""
import argparse
import hashlib
import json
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument("--candidate", type=Path, required=True)
p.add_argument("--out", type=Path, required=True)
a = p.parse_args()
if Path("/root/xoffset-c14-iteration/20261020/v2/validation") not in a.out.resolve().parents or a.out.exists():
    raise ValueError("new v2 validation output required")
d = json.loads((a.candidate / "IDENTITY.json").read_text(encoding="utf-8"))
files = {}
for n, v in d["members"].items():
    if Path(n).is_absolute() or ".." in Path(n).parts:
        raise ValueError("unsafe member")
    raw = (a.candidate / "source" / n).read_bytes()
    if hashlib.sha256(raw).hexdigest() != v["candidate_sha256"] or len(raw) != v["bytes"]:
        raise ValueError("candidate changed: " + n)
    files[n] = raw
a.out.mkdir(parents=True)
for n, raw in files.items():
    target = a.out / n; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(raw)
print(json.dumps({"candidate": d["candidate"], "source": str(a.out), "members": len(files)}))
