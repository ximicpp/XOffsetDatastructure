"""Create diagnostic-only derivatives with an identical hook for C14/R1/M2.

No compile/native execution. Driver milestones/monitor and registration are
validation-owned; all final derived inputs must be bound before either C run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

def once(s, before, after):
    if s.count(before) != 1:
        raise ValueError("diagnostic context not unique: " + before[:90])
    return s.replace(before, after, 1)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, required=True)
    p.add_argument("--experiment", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    if a.out.exists(): raise ValueError("fresh diagnostic output required")
    a.out.mkdir(parents=True)
    # Source closure is small (154 members); dependency vendor remains read-only.
    names = json.loads((Path(__file__).resolve().parents[3] / "handoff/github-20261019/C14_MATERIALIZATION.json").read_text(encoding="utf-8"))["non_external_files"]
    source = a.out / "source"; source.mkdir()
    for name in names:
        target = source / name; target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(a.source / name, target)
    hook = Path(__file__).resolve().parent / "common_diag.hpp"
    shutil.copyfile(hook, source / "c14_common_diag.hpp")
    h = (source / "xoffsetdatastructure.hpp").read_text(encoding="utf-8")
    h = once(h, '#include "xoffset_virtual_memory.hpp"', '#include "xoffset_virtual_memory.hpp"\n#include "c14_common_diag.hpp"')
    h = once(h,
        "            if (canonical_active_count_.load(std::memory_order_acquire) == 0) return nullptr;",
        "            if (c14_diag::enabled) ++c14_diag::counts.canonical;\n"
        "            if (canonical_active_count_.load(std::memory_order_acquire) == 0) {\n"
        "                if (c14_diag::enabled) ++c14_diag::counts.inactive;\n"
        "                return nullptr;\n"
        "            }\n"
        "            if (c14_diag::enabled) ++c14_diag::counts.active;")
    h = once(h,
        "        bool move_assignment_owns_address(const T& value, const void* address) noexcept {\n",
        "        bool move_assignment_owns_address(const T& value, const void* address) noexcept {\n"
        "            c14_diag::OwnsScope observation;\n")
    h = once(h,
        "        const T* data() const noexcept {\n            return fixed_detail::resolve_rel32_const<T>(&data_delta, data_delta);",
        "        const T* data() const noexcept {\n"
        "            if (c14_diag::enabled && c14_diag::owns_depth) ++c14_diag::counts.vector_data_reads;\n"
        "            return fixed_detail::resolve_rel32_const<T>(&data_delta, data_delta);")
    start = h.index("        bool move_assignment_owns_address(const T& value, const void* address) noexcept {")
    end = h.index("        // Both slots must be in the same bounded arena.", start)
    part = h[start:end]
    is_m2 = "const auto* elements = value.data();" in part
    marker = "                    const auto* elements = value.data();" if is_m2 else "                    if (move_assignment_contains(value.data(),"
    part = once(part, marker, "                    if (c14_diag::enabled) ++c14_diag::counts.vectors;\n" + marker)
    part = once(part, "                        return true;\n                    if constexpr (!std::is_trivially_copyable_v<typename CleanT::value_type>) {",
        "                        { if (c14_diag::enabled) ++c14_diag::counts.capacity_hits; return true; }\n"
        "                    if constexpr (!std::is_trivially_copyable_v<typename CleanT::value_type>) {\n"
        "                        if (c14_diag::enabled) {\n"
        "                            ++c14_diag::counts.nontrivial_misses;\n"
        "                            if (value.size()) ++c14_diag::counts.nonempty_misses;\n"
        "                        }")
    h = h[:start] + part + h[end:]
    begin = h.index("        std::string save_verified() {")
    finish = h.index("        static XBuffer load_unverified", begin)
    part = h[begin:finish]
    part = once(part, "        std::string save_verified() {", "        std::string save_verified() {\n"
        "            if (c14_diag::enabled) ++c14_diag::emit.invocations;\n            c14_diag::emit_mark(0);")
    part = once(part, "            XWireHeaderV1 header{};", "            c14_diag::emit_mark(1, payload_size);\n            XWireHeaderV1 header{};")
    part = once(part, "            std::string out;", "            c14_diag::emit_mark(2, sizeof(header));\n            std::string out;")
    part = once(part, "            out.resize_and_overwrite(output_size, [&](char* output, std::size_t) noexcept {",
        "            c14_diag::emit_mark(3, output_size);\n"
        "            out.resize_and_overwrite(output_size, [&](char* output, std::size_t) noexcept {\n"
        "                c14_diag::emit_mark(4, output_size);")
    part = once(part, "                std::memcpy(output, &header, sizeof(header));", "                std::memcpy(output, &header, sizeof(header));\n                c14_diag::emit_mark(5, sizeof(header));")
    part = once(part, "                std::memcpy(output + sizeof(header), base, payload_size);", "                std::memcpy(output + sizeof(header), base, payload_size);\n                c14_diag::emit_mark(6, payload_size);")
    part = once(part, "                return output_size;\n            });", "                return output_size;\n            });\n            c14_diag::emit_mark(7, output_size);")
    part = once(part, "            return out;", "            c14_diag::emit_mark(8, output_size);\n            return out;")
    h = h[:begin] + part + h[finish:]
    (source / "xoffsetdatastructure.hpp").write_text(h, encoding="utf-8")
    adapter = (a.experiment / "adapters/xoffset/adapter.hpp").read_text(encoding="utf-8")
    adapter = once(adapter, "            const auto group = operations.subspan(offset, stop - offset);",
        "            const auto group = operations.subspan(offset, stop - offset);\n"
        "            c14_diag::MutationScope stage_observation(group.front().stage);")
    experiment = a.out / "experiment"; (experiment / "adapters/xoffset").mkdir(parents=True)
    (experiment / "adapters/xoffset/adapter.hpp").write_text(adapter, encoding="utf-8")
    driver = (a.experiment / "driver.cpp").read_text(encoding="utf-8")
    # Primary calls and trace grouping remain identical. The monitor owns t0..t8
    # external events; these POD counter snapshots add no extra native calls.
    driver = once(driver, "    auto initial_usage=usage();const auto t0=Clock::now();",
        "    c14_diag::enabled=true; c14_diag::phase(0);\n    auto initial_usage=usage();const auto t0=Clock::now();")
    for n in range(1, 8):
        text = f"const auto t{n}=Clock::now();"
        driver = once(driver, text, text + f"c14_diag::phase({n});")
    driver = once(driver, "const auto t8=Clock::now();auto primary_usage=usage();",
        "const auto t8=Clock::now();c14_diag::phase(8);c14_diag::enabled=false;auto primary_usage=usage();")
    (experiment / "driver.cpp").write_text(driver, encoding="utf-8")
    identity = {"diagnostic_only": True, "never_for_timing": True, "input_source": str(a.source),
        "hook_sha256": hashlib.sha256(hook.read_bytes()).hexdigest(),
        "patch_tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "M2_observer_predeclared": True,
        "generated": {str(p.relative_to(a.out)): {"bytes": p.stat().st_size,
                      "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                      for p in (source / "xoffsetdatastructure.hpp", source / "c14_common_diag.hpp",
                                experiment / "driver.cpp", experiment / "adapters/xoffset/adapter.hpp")},
        "validation_must_add_before_freeze": "bounded external milestone/flush channel plus post-primary full nine-stage truth replay or equivalent complete audit; exact driver and wrapper identity shared for future M2. No driver/native run admitted by materialization."}
    (a.out / "DIAGNOSTIC_IDENTITY.json").write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(identity))

if __name__ == "__main__": main()
