#!/usr/bin/env python3
"""Generate macOS campaign profiles (profile-normal.json / profile-san.json).

Pure metadata: reads files, runs only `otool -L` / `otool -l` on the already
built sc_* binaries (no native benchmark child is executed). The output matches
campaign.read_profile exactly:
  {schema_version, platform_class, lock_dir, environment, binaries, closure}

  * platform_class  = "macos-arm64-native" (accepted by mac_campaign patch M5)
  * lock_dir        = <business>/runner/build/vm-coordination (= RUNNER.parents[2]/build/vm-coordination)
  * prefix          = [] for every library: no taskset (macOS has no CPU affinity
                      API), no ld-linux loader (dyld resolves LC_RPATH)
  * library/commit/trust_mode are parsed from the meta adapter.hpp constants and
    checked against meta protocol.py IDENTITIES (what `describe` will print)
  * closure binds toolchain, SDK settings, cmake/ninja, competitor sources and
    installs, meta experiment/tool trees, bench build metadata, the xoffset
    source tree, the 14 xoffset production files ('candidate-file/<name>', used
    by mac_campaign A1/A2) and every non-system dylib reported by otool.
    dyld-shared-cache system libraries (/usr/lib, /System) have no on-disk
    bytes; they are listed in the <out>.otool.json sidecar but cannot be hashed.

Usage:
  python3 -I -B make_profiles.py --meta c14 --mode normal --out /ABS/profile-normal.json
  python3 -I -B make_profiles.py --meta r1m2 --mode san    --out /ABS/profile-san.json
"""
from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True

BUSINESS = Path(__file__).resolve().parent
ROOT = Path("/Volumes/XOffsetCS/mac-validation")
TOOLCHAIN = Path("/Volumes/XOffsetCS/toolchain/p2996-d77eff1")
SDK = Path("/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk")
CMAKE = Path("/opt/homebrew/bin/cmake")
NINJA = Path("/opt/homebrew/bin/ninja")
COMPETITORS = Path("/Volumes/XOffsetCS/competitors/sources")
SOURCES = {"c14": Path("/Volumes/XOffsetCS/results/full-native-c14-p2996d77-20261020/source"),
           "r1m2": Path("/Volumes/XOffsetCS/candidate-r1m2-20261020"),
           "w9a": Path("/Volumes/XOffsetCS/candidates/w9a")}
LIBRARIES = ("xoffset", "cista", "flatbuffers", "capnproto", "cereal")
COMPETITOR_DIRS = {"cista": "cista-v0.16", "flatbuffers": "flatbuffers-v25.12.19",
                   "capnproto": "capnproto-v1.5.0", "cereal": "cereal-v1.3.2"}
LOCK_DIR = BUSINESS / "runner/build/vm-coordination"
BASE_ENV = {"LANG": "C", "LC_ALL": "C", "TZ": "UTC", "PATH": "/usr/bin:/bin"}
# Darwin: no LeakSanitizer -> detect_leaks=0 (NO leak claim is made).
# MallocNanoZone=0 silences libmalloc's "nano zone abandoned" stderr line under
# ASan, which would otherwise violate campaign's empty-stderr requirement.
SAN_ENV = {"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=0:allocator_may_return_null=1",
           "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
           "MallocNanoZone": "0"}


def fail(message):
    raise SystemExit("make_profiles: " + message)


def adapter_constants(meta, lib):
    text = (meta / "experiments/serialization_comparison/adapters" / lib / "adapter.hpp").read_text(encoding="utf-8")
    values = {}
    for name in ("library_id", "commit", "trust_mode"):
        found = re.findall(r"\b" + name + r'\s*=\s*"([^"\n]+)"', text)
        if len(found) != 1:
            fail("adapter constant %s/%s not unique" % (lib, name))
        values[name] = found[0]
    return values


def protocol_identities(meta):
    tree = ast.parse((meta / "tools/serialization_comparison/protocol.py").read_bytes())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "IDENTITIES" for t in node.targets):
            return ast.literal_eval(node.value)
    fail("IDENTITIES missing")


def rpaths(binary):
    text = subprocess.run(["/usr/bin/otool", "-l", str(binary)], check=True, capture_output=True, text=True).stdout
    return re.findall(r"cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.+?) \(offset", text)


def dylibs(binary):
    lines = subprocess.run(["/usr/bin/otool", "-L", str(binary)], check=True, capture_output=True, text=True).stdout.splitlines()[1:]
    result = []
    for line in lines:
        name = line.strip().split(" (compatibility")[0]
        if name:
            result.append(name)
    return result


def resolve_dylib(name, binary, search):
    if name.startswith("@rpath/"):
        for directory in search:
            directory = directory.replace("@executable_path", str(binary.parent)).replace("@loader_path", str(binary.parent))
            candidate = Path(directory) / name[len("@rpath/"):]
            if candidate.exists():
                return candidate
        fail("unresolved %s for %s (rpaths %s)" % (name, binary, search))
    return Path(name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--meta", choices=sorted(SOURCES), required=True)
    parser.add_argument("--mode", choices=("normal", "san"), required=True)
    parser.add_argument("--bench", type=Path, help="default: %s/bench/<meta>-<mode>" % ROOT)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    meta = BUSINESS / ("meta-" + args.meta)
    bench = (args.bench or ROOT / "bench" / (args.meta + "-" + args.mode)).resolve()
    source = SOURCES[args.meta]
    capnp_prefix = ROOT / "install" / ("capnproto-asan" if args.mode == "san" else "capnproto")
    out = args.out
    if not out.is_absolute() or out.exists():
        fail("--out must be absolute and fresh")
    identity_path = BUSINESS / "identities" / (args.meta + "-source-identity.json")
    identity = json.loads(identity_path.read_text())
    identities = protocol_identities(meta)
    if identities["xoffset"][0] != identity["candidate_id"]:
        fail("meta protocol label differs from identity record")

    binaries = {}
    for lib in LIBRARIES:
        constants = adapter_constants(meta, lib)
        if (constants["library_id"], constants["commit"]) != tuple(identities[lib]):
            fail("adapter/protocol identity differs: " + lib)
        path = bench / "bin" / ("sc_" + lib)
        if not path.is_file():
            fail("missing built binary: " + str(path))
        binaries[lib] = dict(path=str(path), prefix=[], library=constants["library_id"], commit=constants["commit"],
                             trust_mode=constants["trust_mode"], load_contract="detached")

    closure = {
        "toolchain-clang": str(TOOLCHAIN / "bin/clang++"),
        "toolchain-libcxx-headers": str(TOOLCHAIN / "include/c++/v1"),
        "sdk-settings": str(SDK / "SDKSettings.json"),
        "cmake": str(CMAKE), "ninja": str(NINJA),
        "meta-experiment": str(meta / "experiments/serialization_comparison"),
        "meta-tools": str(meta / "tools/serialization_comparison"),
        "mac-runner": str(BUSINESS / "runner/experiments/vm_coordination/run_job.py"),
        "mac-build-script": str(BUSINESS / "build_bench.sh"),
        "mac-competitor-script": str(BUSINESS / "build_competitors.sh"),
        "bench-compile-commands": str(bench / "compile_commands.json"),
        "bench-cmake-cache": str(bench / "CMakeCache.txt"),
        "bench-generated": str(bench / "generated"),
        "install-flatbuffers": str(ROOT / "install/flatbuffers"),
        "install-capnproto-tools": str(ROOT / "install/capnproto/bin"),
        "install-capnproto-runtime": str(capnp_prefix),
        "xoffset-source-tree": str(source),
        "candidate-source-identity": str(identity_path),
    }
    for lib, name in COMPETITOR_DIRS.items():
        closure["source-" + lib] = str(COMPETITORS / name)
    for name in sorted(identity["production_files"]):
        closure["candidate-file/" + name] = str(source / name)

    otool = {}
    for lib in LIBRARIES:
        binary = Path(binaries[lib]["path"])
        search = rpaths(binary)
        entries = []
        for name in dylibs(binary):
            resolved = resolve_dylib(name, binary, search)
            system = str(resolved).startswith(("/usr/lib/", "/System/"))
            entries.append(dict(install_name=name, resolved=str(resolved), hashed=not system))
            if not system:
                if not resolved.is_file():
                    fail("non-system dylib missing: " + str(resolved))
                closure["dylib/" + resolved.name] = str(resolved)
        otool[lib] = dict(rpaths=search, dylibs=entries)
    if args.mode == "san" and not any("asan" in k for k in closure):
        fail("SAN binaries do not reference the ASan runtime dylib (check build)")
    for key, value in closure.items():
        if not Path(value).exists():
            fail("closure path missing: %s=%s" % (key, value))

    profile = dict(schema_version=1, platform_class="macos-arm64-native", lock_dir=str(LOCK_DIR),
                   environment=dict(BASE_ENV, **(SAN_ENV if args.mode == "san" else {})),
                   binaries=binaries, closure=closure)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(profile, indent=2, sort_keys=True, ensure_ascii=False) + "\n")
    sidecar = out.with_name(out.name + ".otool.json")
    with sidecar.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(dict(schema_version=1, profile=str(out), meta=args.meta, mode=args.mode,
                                     otool=otool, note="system dylibs live in the dyld shared cache and are not hashed"),
                                indent=2, sort_keys=True) + "\n")
    print(json.dumps(dict(profile=str(out), binaries=len(binaries), closure=len(closure)), sort_keys=True))


if __name__ == "__main__":
    main()
