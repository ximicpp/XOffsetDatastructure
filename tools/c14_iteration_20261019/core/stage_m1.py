"""Copy exactly the recorded source derivative into a NEW WSL core directory."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--candidate", type=Path, required=True)
p.add_argument("--out", type=Path, required=True)
a = p.parse_args()
allowed = Path("/root/xoffset-c14-iteration/20261019/v1/core")
if allowed not in a.out.resolve().parents or a.out.exists():
    raise ValueError("new WSL core candidate directory required")
d = json.loads((a.candidate / "IDENTITY.json").read_text(encoding="utf-8-sig"))
data = {}
for name, entry in d["members"].items():
    if Path(name).is_absolute() or ".." in Path(name).parts:
        raise ValueError("unsafe source member")
    raw = (a.candidate / "source" / name).read_bytes()
    if hashlib.sha256(raw).hexdigest() != entry["candidate_sha256"]:
        raise ValueError(f"candidate changed: {name}")
    data[name] = raw
a.out.mkdir(parents=True)
for name, raw in data.items():
    target = a.out / "source" / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(raw)
d["staged_source"] = str(a.out / "source")
(a.out / "IDENTITY.json").write_text(json.dumps(d, indent=2) + "\n")
print(json.dumps({"candidate": d["candidate"], "members": len(data), "source": d["staged_source"]}))
