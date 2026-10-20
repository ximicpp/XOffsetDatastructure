"""Independent C14 derivative: outline only canonical's active-owner body."""
import difflib
import hashlib
import json
from pathlib import Path
from prepare_m1 import once

repo = Path(__file__).resolve().parents[3]
out = repo / "build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1"
if out.exists():
    raise ValueError("R1 source must be materialized into a new directory")
mapping = repo / "handoff/github-20261019/C14_MATERIALIZATION.json"
entries = json.loads(mapping.read_text(encoding="utf-8"))["non_external_files"]
originals = {}
for name, meta in entries.items():
    data = (repo / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != meta["sha256"]:
        raise ValueError(f"baseline mismatch: {name}")
    originals[name] = data
name = "xoffsetdatastructure.hpp"
text = originals[name].decode("utf-8")
text = once(text,
    "            if (canonical_active_count_.load(std::memory_order_acquire) == 0) return nullptr;\n"
    "            const auto address = reinterpret_cast<std::uintptr_t>(anchor);",
    "            if (canonical_active_count_.load(std::memory_order_acquire) == 0) return nullptr;\n"
    "            return canonical_bytes_at_active(anchor, target, bytes);\n"
    "        }\n\n"
    "    private:\n"
    "        // Keep the inactive acquire-load/return in the small public helper.\n"
    "        // The active path retains all original owner/TLS/registry semantics.\n"
    "#if defined(_MSC_VER)\n"
    "        __declspec(noinline)\n"
    "#elif defined(__clang__) || defined(__GNUC__)\n"
    "        __attribute__((noinline))\n"
    "#endif\n"
    "        static std::byte* canonical_bytes_at_active(const void* anchor,\n"
    "                std::uintptr_t target, std::size_t bytes) noexcept {\n"
    "            const auto address = reinterpret_cast<std::uintptr_t>(anchor);")
text = once(text,
    "            return recover(record);\n"
    "        }\n\n"
    "        XGrowPolicy growth_policy() const noexcept",
    "            return recover(record);\n"
    "        }\n\n"
    "    public:\n"
    "        XGrowPolicy growth_policy() const noexcept")
changed = text.encode("utf-8")
source = out / "source"
source.mkdir(parents=True)
members = {}
for member, original in originals.items():
    data = changed if member == name else original
    p = source / member
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    members[member] = {"baseline_sha256": hashlib.sha256(original).hexdigest(),
        "candidate_sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data), "changed": data != original}
patch = "".join(difflib.unified_diff(originals[name].decode().splitlines(True),
    text.splitlines(True), fromfile="a/"+name, tofile="b/"+name)).encode("utf-8")
(out / "R1.patch").write_bytes(patch)
identity = {"schema_version": 1, "candidate": out.name,
    "baseline_commit": "1904b9c71230026ac6f2d58f919a9ab76447d264",
    "materialization_sha256": hashlib.sha256(mapping.read_bytes()).hexdigest(),
    "source": str(source), "dependencies": "locked WSL final-v4 vendor content; not copied",
    "member_count": len(members), "changed": [name],
    "patch_sha256": hashlib.sha256(patch).hexdigest(), "members": members,
    "M1_combined": False, "native_executed": False, "mechanism_admitted": False, "integrated": False}
(out / "IDENTITY.json").write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
doc = repo / "docs/coordination/c14-iteration-20261019-v1/core/R1_IDENTITY.json"
doc.write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"candidate": out.name, "patch_sha256": identity["patch_sha256"],
                  "main_sha256": members[name]["candidate_sha256"]}))
