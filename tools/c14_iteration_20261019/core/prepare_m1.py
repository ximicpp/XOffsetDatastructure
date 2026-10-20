"""Materialize an isolated, hash-checked C14 derivative; no native execution."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path


def sha(data):
    return hashlib.sha256(data).hexdigest()


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"expected exactly one patch context: {old[:90]!r}")
    return text.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    repo, out = args.repo.resolve(), args.out.resolve()
    allowed = repo / "build/c14-iteration-20261019-v1/candidates"
    if allowed not in out.parents or out.exists():
        raise ValueError("output must be a new directory beneath this iteration's candidates")
    materialization = repo / "handoff/github-20261019/C14_MATERIALIZATION.json"
    manifest = json.loads(materialization.read_text(encoding="utf-8"))
    members = manifest["non_external_files"]
    originals = {}
    for name, entry in members.items():
        path = Path(name)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"unsafe source member: {name}")
        data = (repo / path).read_bytes()
        if sha(data) != entry["sha256"] or len(data) != entry["bytes"]:
            raise ValueError(f"C14 baseline bytes mismatch: {name}")
        originals[name] = data
    updated = dict(originals)
    name = "xoffsetdatastructure.hpp"
    text = originals[name].decode("utf-8")
    text = once(text,
        "        template<class T> consteval bool future_custom_lifetime_supported();\n",
        "        template<class T> consteval bool future_custom_lifetime_supported();\n"
        "        template<class T> consteval bool default_scalar_bulk_supported();\n")
    text = once(text,
        "            resize_construct(count, [&](T* dst) {\n"
        "                fixed_detail::default_construct(dst, binding());\n"
        "            });\n"
        "        }\n\n"
        "        template <typename Construct>\n"
        "        void resize_construct(size_type count, Construct&& construct) {",
        "            if constexpr (detail::default_scalar_bulk_supported<T>()) {\n"
        "                resize_construct<true>(count, [&](T* dst) {\n"
        "                    fixed_detail::default_construct(dst, binding());\n"
        "                });\n"
        "            } else {\n"
        "                resize_construct(count, [&](T* dst) {\n"
        "                    fixed_detail::default_construct(dst, binding());\n"
        "                });\n"
        "            }\n"
        "        }\n\n"
        "        template <bool ValueInitializeTail = false, typename Construct>\n"
        "        void resize_construct(size_type count, Construct&& construct) {\n"
        "            static_assert(!ValueInitializeTail || detail::default_scalar_bulk_supported<T>());")
    text = once(text,
        "                    for (; tail_built < count - old_size; ++tail_built)\n"
        "                        construct(fresh + old_size + tail_built);",
        "                    if constexpr (ValueInitializeTail) {\n"
        "                        // Ordinary scalar value initialization cannot throw or\n"
        "                        // invoke a binding/custom construction callback. Keep\n"
        "                        // the general fresh transaction and commit order.\n"
        "                        std::uninitialized_value_construct_n(\n"
        "                            fresh + old_size, count - old_size);\n"
        "                        tail_built = count - old_size;\n"
        "                    } else {\n"
        "                        for (; tail_built < count - old_size; ++tail_built)\n"
        "                            construct(fresh + old_size + tail_built);\n"
        "                    }")
    updated[name] = text.encode("utf-8")
    name = "xoffset_core_lifetime.hpp"
    text = originals[name].decode("utf-8")
    text = once(text, "namespace XOffsetDatastructure::detail {\n",
        "namespace XOffsetDatastructure::detail {\n\n"
        "// Do not batch across any explicitly enabled construction frontier,\n"
        "// including an incomplete policy which must retain its old dispatch.\n"
        "template<class T>\n"
        "consteval bool default_scalar_bulk_supported() {\n"
        "    return std::is_scalar_v<T> && !future_custom_lifetime_supported<T>() &&\n"
        "           !wire_activation<T>::enabled &&\n"
        "           !wire_future_lifetime<T>::enabled;\n"
        "}\n")
    updated[name] = text.encode("utf-8")
    source = out / "source"
    source.mkdir(parents=True)
    identities, patches = {}, []
    for name, data in updated.items():
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        identities[name] = {"baseline_sha256": sha(originals[name]),
                            "candidate_sha256": sha(data), "bytes": len(data),
                            "changed": data != originals[name]}
        if data != originals[name]:
            patches.extend(difflib.unified_diff(originals[name].decode().splitlines(True),
                data.decode().splitlines(True), fromfile="a/" + name, tofile="b/" + name))
    patch = "".join(patches).encode("utf-8")
    (out / "M1.patch").write_bytes(patch)
    identity = {"schema_version": 1, "candidate": out.name,
        "baseline_commit": "1904b9c71230026ac6f2d58f919a9ab76447d264",
        "materialization_sha256": sha(materialization.read_bytes()),
        "source": str(source), "dependencies": "external dependencies not copied; validation must bind locked vendor tree",
        "member_count": len(identities), "changed": [n for n, v in identities.items() if v["changed"]],
        "patch_sha256": sha(patch), "members": identities,
        "native_executed": False, "mechanism_admitted": False, "integrated": False}
    (out / "IDENTITY.json").write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: identity[k] for k in ("candidate", "member_count", "changed", "patch_sha256")}))


if __name__ == "__main__":
    main()
