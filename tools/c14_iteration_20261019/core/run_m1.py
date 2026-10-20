"""Finite M1 jobs. CALL ONLY INSIDE the validation thread's global lease.

This does not acquire a second lock or admit timing. Source arguments are exact
trees; vendor/experiment are read-only. All outputs must be new directories.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"ambiguous diagnostic context: {old[:70]!r}")
    return text.replace(old, new, 1)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser()
    for arg in ("source", "baseline", "vendor", "experiment", "out", "compiler"):
        p.add_argument("--" + arg, type=Path, required=True)
    p.add_argument("--mode", choices=("codegen", "correctness", "mechanism"), required=True)
    p.add_argument("--san-resource", type=Path)
    p.add_argument("--wire", type=Path)
    p.add_argument("--trace", type=Path)
    p.add_argument("--mechanism-admission", type=Path)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    here = Path(__file__).resolve().parent
    records = []
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    def run(argv, name, limit=240):
        argv = [str(x) for x in argv]
        start = time.monotonic()
        record = {"argv": argv, "timeout_seconds": limit}
        try:
            with (a.out / (name + ".log")).open("xb") as f:
                done = subprocess.run(argv, stdout=f, stderr=subprocess.STDOUT,
                                      timeout=limit, env=env, check=False)
            record["exit_code"] = done.returncode
        finally:
            record["elapsed_seconds"] = time.monotonic() - start
            records.append(record)
            (a.out / "commands.json").write_text(json.dumps(records, indent=2) + "\n")
        if done.returncode:
            raise RuntimeError(f"{name} failed; inspect its log; no retry")

    includes = ["-I" + str(a.vendor / "external/typelayout/include")]
    for name in ("config", "container", "container_hash", "core", "intrusive", "predef",
                 "static_assert", "throw_exception", "type_traits", "winapi"):
        includes += ["-I" + str(a.vendor / f"external/boost/libs/{name}/include")]
    common = [str(a.compiler), "-std=gnu++26", "-freflection", "-fexpansion-statements",
              "-stdlib=libc++", "-pthread", "-O3", "-DNDEBUG"] + includes
    exp = ["-I" + str(a.experiment), "-I" + str(a.experiment / "include"),
           '-DSC_ADAPTER_HEADER="adapters/xoffset/adapter.hpp"',
           "-DSC_ADAPTER_TYPE=sc::XOffsetAdapter"]
    identity = {"mode": a.mode, "compiler": str(a.compiler),
                "compiler_sha256": digest(a.compiler), "source": str(a.source),
                "baseline": str(a.baseline), "vendor": str(a.vendor),
                "source_main_sha256": digest(a.source / "xoffsetdatastructure.hpp"),
                "baseline_main_sha256": digest(a.baseline / "xoffsetdatastructure.hpp"),
                "tool_sha256": digest(Path(__file__)), "timing_admitted": False}
    for name in ("wire", "trace"):
        value = getattr(a, name)
        if value:
            identity[name] = {"path": str(value), "sha256": digest(value)}
    (a.out / "INPUTS.json").write_text(json.dumps(identity, indent=2) + "\n")
    if a.mode == "codegen":
        # These are the ORIGINAL complete driver and original adapter calls.
        # No diagnostics, assertion re-enable flag or custom benchmark here.
        for role, source in (("control", a.baseline), ("candidate", a.source)):
            run(common + ["-I" + str(source)] + exp + ["-S", a.experiment / "driver.cpp",
                "-o", a.out / (role + "-driver.s")], role + "-driver-codegen")
        (a.out / "REVIEW_REQUIRED.json").write_text(json.dumps({
            "status": "manual_machine_code_review_required",
            "reject_if": "control already batches fresh scalar initialization or no repeated binding survives",
            "no_native_or_timing_executed": True}, indent=2) + "\n")
        return
    if not a.mechanism_admission:
        raise ValueError("correctness/mechanism require independent codegen admission JSON")
    admission = json.loads(a.mechanism_admission.read_text())
    if admission.get("status") != "passed" or admission.get("candidate_main_sha256") != identity["source_main_sha256"]:
        raise ValueError("mechanism not admitted for this exact source")
    if a.mode == "correctness":
        if not a.san_resource:
            raise ValueError("SAN resource-dir is required; no ABI fallback")
        tests = [a.source / "tests/test_scalar_resize.cpp",
                 a.source / "tests/test_custom_future_lifetime.cpp",
                 a.source / "tests/test_core_lifetime.cpp",
                 here / "test_m1_scalar_values.cpp"]
        for profile in ("normal", "san"):
            options = common + ["-UNDEBUG", "-I" + str(a.source), "-I" + str(a.source / "tests")]
            if profile == "san":
                options += ["-resource-dir=" + str(a.san_resource), "-fsanitize=address,undefined",
                            "-fno-omit-frame-pointer", "-g"]
            for test in tests:
                target = a.out / (profile + "-" + test.stem)
                run(options + [test, "-o", target], target.name + "-compile")
                run([target], target.name + "-run", 30)
        return
    if not a.wire or not a.trace:
        raise ValueError("fixed M wire and trace required")
    # Pure logical counts in a separate derivative. Never used as timing code.
    stats = '''
namespace XOffsetDatastructure::detail::m1_observe {
inline thread_local unsigned long long entries, capacity, tail, fresh, elements, bindings, bulk;
inline void reset() { entries=capacity=tail=fresh=elements=bindings=bulk=0; }
}
'''
    for role, source in (("control", a.baseline), ("candidate", a.source)):
        overlay = a.out / role / "source"
        shutil.copytree(source, overlay)
        h = (overlay / "xoffsetdatastructure.hpp").read_text()
        h = once(h, "namespace XOffsetDatastructure {", stats + "\nnamespace XOffsetDatastructure {")
        h = once(h, "void resize_default_scalar(size_type count) requires std::is_scalar_v<T> {",
                 "void resize_default_scalar(size_type count) requires std::is_scalar_v<T> {\n"
                 "            ++detail::m1_observe::entries;")
        h = once(h, "std::uninitialized_value_construct_n(data() + old_size, count - old_size);",
                 "++detail::m1_observe::capacity;\n                "
                 "std::uninitialized_value_construct_n(data() + old_size, count - old_size);")
        h = once(h, "auto* expanded = detail::rebuild_dynamic_array<T>(old, count, old_size);",
                 "++detail::m1_observe::tail;\n                "
                 "auto* expanded = detail::rebuild_dynamic_array<T>(old, count, old_size);")
        start = h.index("void resize_default_scalar(")
        end = h.index("void resize_construct(", start)
        block = h[start:end]
        # Only the actual original/default fallback lambda invocation counts
        # as per-element binding work. Candidate bulk must leave this zero.
        block = block.replace("fixed_detail::default_construct(dst, binding());",
            "++detail::m1_observe::bindings; fixed_detail::default_construct(dst, binding());")
        marker = "            if constexpr (detail::default_scalar_bulk_supported<T>()) {" if role == "candidate" else "            resize_construct(count, [&](T* dst) {"
        block = once(block, marker,
            "            ++detail::m1_observe::fresh;\n"
            "            detail::m1_observe::elements += count - old_size;\n" + marker)
        h = h[:start] + block + h[end:]
        if role == "candidate":
            h = once(h, "if constexpr (ValueInitializeTail) {",
                "if constexpr (ValueInitializeTail) {\n                        detail::m1_observe::bulk += count - old_size;")
        (overlay / "xoffsetdatastructure.hpp").write_text(h)
        driver = (a.experiment / "driver.cpp").read_text()
        call = "        A::mutate(state,std::span(trace.mutations).subspan(start,next-start));"
        driver = once(driver, call,
            "        XOffsetDatastructure::detail::m1_observe::reset();\n" + call + '''
        { using namespace XOffsetDatastructure::detail::m1_observe;
          std::cout << "M1 stage=" << si << " entries=" << entries << " capacity=" << capacity
            << " tail=" << tail << " fresh=" << fresh << " elements=" << elements
            << " binding_evaluations=" << bindings << " bulk_elements=" << bulk << '\\n'; }
''')
        cpp = a.out / role / "diagnostic_driver.cpp"
        cpp.write_text(driver)
        binary = a.out / role / "fixed-m"
        run(common + ["-I" + str(overlay)] + exp + [cpp, "-o", binary], role + "-diagnostic-compile")
        run([binary, "validate", "--wire", a.wire, "--trace", a.trace,
             "--result", a.out / role / "validation.json"], role + "-fixed-m", 30)
        (a.out / role / "DIAGNOSTIC_IDENTITY.json").write_text(json.dumps({
            "main_sha256": digest(overlay / "xoffsetdatastructure.hpp"),
            "driver_sha256": digest(cpp), "binary_sha256": digest(binary),
            "instrumented": True, "not_for_timing": True}, indent=2) + "\n")


if __name__ == "__main__":
    main()
