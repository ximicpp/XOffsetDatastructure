"""Materialize exactly one local-data reuse change from immutable C14 bytes."""
import difflib
import hashlib
import json
from pathlib import Path

r = Path(__file__).resolve().parents[3]
out = r / "build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1"
if out.exists():
    raise ValueError("new candidate output required")
mapping = r / "handoff/github-20261019/C14_MATERIALIZATION.json"
entries = json.loads(mapping.read_text(encoding="utf-8"))["non_external_files"]
originals = {}
for name, meta in entries.items():
    raw = (r / name).read_bytes()
    if hashlib.sha256(raw).hexdigest() != meta["sha256"] or len(raw) != meta["bytes"]:
        raise ValueError("baseline mismatch: " + name)
    originals[name] = raw
old = '''                    if (move_assignment_contains(value.data(),
                        std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address))
                        return true;
                    if constexpr (!std::is_trivially_copyable_v<typename CleanT::value_type>) {
                        for (const auto& child : value) {
                            if (move_assignment_owns_address(child, address)) return true;
                        }
                    }'''
new = '''                    const auto* elements = value.data();
                    if (move_assignment_contains(elements,
                        std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address))
                        return true;
                    if constexpr (!std::is_trivially_copyable_v<typename CleanT::value_type>) {
                        const auto count = value.size();
                        const auto* last = elements ? elements + count : nullptr;
                        for (auto* item = elements; item != last; ++item) {
                            const auto& child = *item;
                            if (move_assignment_owns_address(child, address)) return true;
                        }
                    }'''
name = "xoffsetdatastructure.hpp"
text = originals[name].decode("utf-8")
if text.count(old) != 1:
    raise ValueError("exact M2 patch context not found once")
text = text.replace(old, new, 1)
changed = text.encode("utf-8")
source = out / "source"
source.mkdir(parents=True)
members = {}
for member, raw in originals.items():
    data = changed if member == name else raw
    p = source / member
    p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data)
    members[member] = {"baseline_sha256": hashlib.sha256(raw).hexdigest(),
        "candidate_sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data), "changed": data != raw}
patch = "".join(difflib.unified_diff(originals[name].decode().splitlines(True), text.splitlines(True),
    fromfile="a/" + name, tofile="b/" + name)).encode("utf-8")
(out / "M2.patch").write_bytes(patch)
d = {"schema_version": 1, "candidate": out.name, "baseline_commit": "1904b9c71230026ac6f2d58f919a9ab76447d264",
    "materialization_sha256": hashlib.sha256(mapping.read_bytes()).hexdigest(), "source": str(source),
    "member_count": len(members), "changed": [name], "patch_sha256": hashlib.sha256(patch).hexdigest(),
    "members": members, "R1_or_M1_combined": False, "native_executed": False, "integrated": False}
(out / "IDENTITY.json").write_text(json.dumps(d, indent=2) + "\n", encoding="utf-8")
(r / "docs/coordination/c14-iteration-20261020-v2/core/M2_IDENTITY.json").write_text(json.dumps(d, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"candidate": out.name, "patch_sha256": d["patch_sha256"], "main_sha256": members[name]["candidate_sha256"]}))
