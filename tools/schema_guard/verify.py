#!/usr/bin/env python3
'''Compile-time XOffset schema gate. Does not run target programs or test wire.'''
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
TARGETS = {
    "linux-x64": "XSG_LINUX_X64", "linux-arm64": "XSG_LINUX_ARM64",
    "macos-arm64": "XSG_MAC_ARM64", "macos-x64": "XSG_MAC_X64",
    "ios-arm64": "XSG_IOS_ARM64", "ios-simulator-arm64": "XSG_IOS_ARM64",
    "ios-simulator-x64": "XSG_IOS_SIM_X64",
    "android-arm64": "XSG_ANDROID_ARM64", "android-x64": "XSG_ANDROID_X64",
    "windows-x64": "XSG_WINDOWS_X64", "windows-arm64": "XSG_WINDOWS_ARM64",
}
IMAGES = {
    "linux-x64": "gcc@sha256:3f73339bf0d3d64081cac3f6d9fdafc6dd4cef2b9c49e0a09f91e97cb0eb139a",
    "linux-arm64": "gcc@sha256:325784a7e64cece62cc2ba202f8ee9e9b6e09dbcecabe98697a79a41148b177a",
}
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def verify_dependencies():
    lock = (ROOT / "cmake/XOffsetDependencyLock.cmake").read_text(encoding="utf-8")
    entries = re.findall(r'"(external/[^"|]+)\|([0-9a-f]{64})"', lock)
    if len(entries) != 14:
        return False, [{"error": "expected 11 locked trees and 3 licenses"}]
    records = []
    for name, expected in entries:
        path = ROOT / name
        if path.is_dir():
            paths = sorted((x for x in path.rglob("*") if x.is_file()),
                           key=lambda x: x.relative_to(path).as_posix())
            rows = "".join(x.relative_to(path).as_posix() + " " + sha(x) + "\n" for x in paths)
            actual = hashlib.sha256(rows.encode("utf-8")).hexdigest()
        elif path.is_file():
            actual = sha(path)
        else:
            actual = None
        records.append({"path": name, "expected": expected, "actual": actual})
    return all(x["actual"] == x["expected"] for x in records), records

def run(cmd, timeout=180):
    try:
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                           errors="replace", timeout=timeout)
        return p.returncode, p.stdout + p.stderr
    except (OSError, subprocess.TimeoutExpired) as exc:
        return None, str(exc)
def includes(base):
    # Discover local source dependencies even when paths inside Docker differ.
    boost = sorted((ROOT / "external/boost/libs").glob("*/include"))
    return [base, base / "external/typelayout/include",
            *(base / path.relative_to(ROOT) for path in boost)]
def flags_for(target, catalog, base):
    result = ["-D" + TARGETS[target] + "=1"]
    if target == "ios-simulator-arm64":
        result.append("-DXSG_SIMULATOR=1")
    result.append('-DXSG_CATALOG="' + str(catalog) + '"')
    for path in includes(base):
        result.extend(["-I", str(path)])
    return result
def sdk(name):
    rc, out = run(["xcrun", "--sdk", name, "--show-sdk-path"], 20)
    return out.strip() if rc == 0 else None
def default_config(target, clang, ndk):
    if target.startswith("linux-"):
        if not shutil.which("docker"):
            return None, "Docker client unavailable on Mac"
        rc, out = run(["docker", "info", "--format", "{{.OSType}}"], 30)
        if rc != 0:
            return None, "Docker daemon unavailable: " + out.strip()
        return {"kind": "docker", "image": IMAGES[target],
                "platform": "linux/amd64" if target.endswith("x64") else "linux/arm64",
                "compiler": "g++", "flags": ["-std=c++26", "-freflection"]}, ""
    if target.startswith("windows-"):
        return None, "No configured Windows P2996 cross compiler; supply a profile"
    if not clang or not Path(clang).is_file():
        return None, "No P2996 Clang supplied (--clang)"
    libcxx = Path(clang).resolve().parent.parent / "include/c++/v1"
    # A compiler wrapper may supply its own matching headers (this Mac does).
    # If it does not, use the toolchain include directory when present; the
    # actual compiler probe, not a path heuristic, determines availability.
    common = ["-std=c++26", "-freflection", "-fexpansion-statements", "-stdlib=libc++"]
    if libcxx.is_dir():
        common += ["-isystem", str(libcxx)]
    if target.startswith("android-"):
        if not ndk:
            return None, "Android NDK not configured (--ndk or ANDROID_NDK_HOME)"
        sysroot = Path(ndk) / "toolchains/llvm/prebuilt"
        found = list(sysroot.glob("*/sysroot"))
        if len(found) != 1:
            return None, "Expected one Android NDK host sysroot under " + str(sysroot)
        return {"kind":"host", "compiler":clang,
                "flags":common + ["--target=" + ("aarch64-linux-android24" if target == "android-arm64" else "x86_64-linux-android24"), "--sysroot=" + str(found[0])]}, ""
    if target.startswith("ios-"):
        name = "iphonesimulator" if target.startswith("ios-simulator") else "iphoneos"
        sysroot = sdk(name)
        if not sysroot:
            return None, "iOS SDK unavailable: " + name
        triple = ("x86_64-apple-ios15.0-simulator" if target == "ios-simulator-x64" else
                  "arm64-apple-ios15.0-simulator" if name == "iphonesimulator" else "arm64-apple-ios15.0")
        return {"kind":"host", "compiler":clang,
                "flags":common + ["--target=" + triple, "-isysroot", sysroot]}, ""
    if target.startswith("macos-"):
        sysroot = sdk("macosx")
        if not sysroot:
            return None, "macOS SDK unavailable"
        arch = "arm64" if target.endswith("arm64") else "x86_64"
        return {"kind":"host", "compiler":clang,
                "flags":common + ["--target=" + arch + "-apple-macos11", "-isysroot", sysroot]}, ""
    return None, "Unknown profile"
def execute(target, config, catalog, out, pull):
    if config.get("kind") == "host":
        cmd = [config["compiler"], *config.get("flags", []),
               *flags_for(target, catalog, ROOT), "-fsyntax-only", str(HERE / "probe.cpp")]
    elif config.get("kind") == "docker":
        try:
            rel = catalog.relative_to(ROOT)
        except ValueError:
            return {"status":"BLOCKED_CONFIG", "reason":"Docker catalog must be inside source tree"}
        base = Path("/workspace")
        cmd = ["docker", "run", "--rm", "--network=none",
               "--pull=" + ("missing" if pull else "never"),
               "--platform", config["platform"], "-v", str(ROOT) + ":/workspace:ro",
               "-w", "/workspace", config["image"], config["compiler"],
               *config.get("flags", []), *flags_for(target, base / rel, base),
               "-fsyntax-only", str(base / "tools/schema_guard/probe.cpp")]
    else:
        return {"status":"BLOCKED_CONFIG", "reason":"profile kind must be host or docker"}
    rc, output = run(cmd)
    (out / (target + ".log")).write_text(output, encoding="utf-8")
    if rc == 0:
        status = "PASS_COMPILE"
    elif rc is None:
        status = "BLOCKED_EXECUTION"
    elif config.get("kind") == "docker" and rc == 125:
        status = "BLOCKED_TOOLCHAIN"
    elif any(x in output.lower() for x in ("schema field names/order mismatch", "schema root field list missing", "schema reachable record field list missing")):
        status = "FAIL_SEMANTIC"
    elif any(x in output.lower() for x in ("schema pin mismatch", "typelayout signature mismatch")):
        status = "FAIL_LAYOUT"
    else:
        status = "FAIL_COMPILE"
    error_line = next((line.strip() for line in output.splitlines()
                       if "error:" in line.lower() or "unable to find image" in line.lower()), "")
    return {"status": status, "exit_code": rc, "command": cmd,
            "log": target + ".log",
            "reason": "" if rc == 0 else (error_line[:300] or "See log; failure is not a platform pass")}
def report(results, out, meta):
    record = {"format": 1, "metadata": meta, "targets": results,
              "scope": {"layout": "compile-time XOffset pin, TypeLayout pin and byte-copy safety",
                        "target_execution": "NOT_RUN",
                        "cross_platform_wire": "NOT_RUN",
                        "semantic_scope": "declared field-name/order and explicit catalog assertions only; other business semantics NOT_RUN"}}
    (out / "result.json").write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = ["# Schema compile-time verification", "",
             "Compile success is not a runtime, wire-exchange or semantic validation.", "",
             "| Target | Compile status | Detail |", "| --- | --- | --- |"]
    for name, result in results.items():
        detail = result.get("reason") or result.get("log", "")
        lines.append("| " + name + " | " + result["status"] + " | " + detail.replace("|", "/").replace("\n", " ") + " |")
    provenance = meta.get("android_ndk_provenance")
    if isinstance(provenance, dict):
        lines += ["", "Android sysroot provenance: " + str(provenance.get("extraction", "unspecified")) +
                  "; full archive SHA1: " + str(provenance.get("full_archive_sha1", "unspecified")) + "."]
    lines += ["", "Target execution: NOT_RUN. Wire exchange: NOT_RUN. Declared field-name/order checks are compile-time; complete business semantics: NOT_RUN.", ""]
    (out / "REPORT.md").write_text("\n".join(lines), encoding="utf-8")
def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", required=True, help="new output directory; never reused")
    p.add_argument("--clang", default=os.environ.get("CXX"))
    p.add_argument("--ndk", default=os.environ.get("ANDROID_NDK_HOME"))
    p.add_argument("--catalog", default=str(HERE / "catalog.hpp"))
    p.add_argument("--profiles", help="JSON file overriding target profiles")
    p.add_argument("--target", action="append", choices=TARGETS)
    p.add_argument("--pull", action="store_true", help="allow Docker to pull pinned images")
    args = p.parse_args()
    catalog = Path(args.catalog).resolve()
    if not catalog.is_file():
        p.error("catalog not found: " + str(catalog))
    profiles = json.loads(Path(args.profiles).read_text(encoding="utf-8")) if args.profiles else {}
    unknown = set(profiles) - set(TARGETS)
    if unknown:
        p.error("unknown profile targets: " + ", ".join(sorted(unknown)))
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    rc, git = run(["git", "rev-parse", "HEAD"], 20)
    rc2, dirty = run(["git", "status", "--porcelain"], 20)
    version_rc, version = run([args.clang, "--version"], 20) if args.clang else (None, "")
    source_files = ["xoffsetdatastructure.hpp", "xoffset_core_lifetime.hpp",
                    "xoffset_virtual_memory.hpp", "xoffset_slot_storage.hpp",
                    "cmake/XOffsetDependencyLock.cmake"]
    meta = {"source": str(ROOT), "git_head": git.strip() if rc == 0 else None,
            "git_status": dirty if rc2 == 0 else "unavailable",
            "source_sha256": {name: sha(ROOT / name) for name in source_files},
            "compiler_version": version.strip() if version_rc == 0 else None,
            "compiler_sha256": sha(args.clang) if args.clang and Path(args.clang).is_file() else None,
            "profiles_sha256": sha(args.profiles) if args.profiles else None,
            "catalog": str(catalog), "catalog_sha256": sha(catalog),
            "probe_sha256": sha(HERE / "probe.cpp"),
            "compiler_argument": args.clang, "host": platform.platform(),
            "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    if args.profiles:
        provenance = Path(args.profiles).resolve().parent / "manifest.json"
        if provenance.is_file():
            try:
                meta["profiles_provenance"] = json.loads(provenance.read_text(encoding="utf-8"))
                meta["profiles_provenance_sha256"] = sha(provenance)
            except (OSError, ValueError):
                meta["profiles_provenance"] = "unreadable"
        else:
            meta["profiles_provenance"] = "not supplied"
    if args.ndk:
        meta["android_ndk"] = str(Path(args.ndk).resolve())
        provenance = Path(args.ndk).resolve().parent / "manifest.json"
        if provenance.is_file():
            try:
                meta["android_ndk_provenance"] = json.loads(provenance.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                meta["android_ndk_provenance"] = "unreadable"
        else:
            meta["android_ndk_provenance"] = "not supplied"
    dependencies_ok, dependency_records = verify_dependencies()
    meta["dependency_lock"] = dependency_records
    meta["dependency_lock_passed"] = dependencies_ok
    results = {}
    for target in TARGETS:
        if args.target and target not in args.target:
            results[target] = {"status":"NOT_RUN", "reason":"Not selected"}
        elif not dependencies_ok:
            results[target] = {"status":"BLOCKED_DEPENDENCY", "reason":"Pinned dependency content mismatch; see metadata"}
        else:
            config, reason = (profiles[target], "") if target in profiles else default_config(target, args.clang, args.ndk)
            if config is None:
                results[target] = {"status":"BLOCKED_TOOLCHAIN", "reason":reason}
            else:
                try:
                    results[target] = execute(target, config, catalog, out, args.pull)
                except (KeyError, TypeError, ValueError) as exc:
                    results[target] = {"status":"BLOCKED_CONFIG", "reason":str(exc)}
        report(results, out, meta)
        print(target, results[target]["status"], flush=True)
    print("Report:", out / "REPORT.md", flush=True)
    selected = [results[t]["status"] for t in TARGETS if not args.target or t in args.target]
    return 0 if selected and all(s == "PASS_COMPILE" for s in selected) else 2
if __name__ == "__main__":
    sys.exit(main())
