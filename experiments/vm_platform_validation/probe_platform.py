#!/usr/bin/env python3
"""Read-only platform inventory and frozen Windows ZIP audit, never a test run.

--out must be new. --audit reads an existing evidence directory offline.
The only remote operation is one bounded identity/tool-path SSH probe of the
existing sfcdev alias, after static config review; no builds or jobs are launched.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import socket
import stat
import subprocess
import sys
import time
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SELF = Path(__file__).resolve()
ZIP_REL = "experiments/vm_windows/artifacts/frozen-f-storage.zip"
WINDOWS_MANIFEST_SHA = "b4db950e010cfe68958360103a5cbfe3330f565c9ec63d2b0f88897daa5d8025"
WINDOWS_ARCHIVE_SHA = "c1a69f06cf67db2f773368d5828798873a9faac071f40f0e4c5550507897b3a3"


def now():
    return datetime.now(timezone.utc).isoformat()


def sha(path):
    with open(path, "rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def write_new(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "x", encoding="utf-8") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")


def file_identity(path):
    path = Path(path)
    if not path.is_file():
        return {"path": str(path), "exists": False}
    return {"path": str(path), "resolved_path": str(path.resolve()), "exists": True,
            "bytes": path.stat().st_size, "sha256": sha(path)}


def all_files(out):
    return [{"path": p.relative_to(out).as_posix(), "bytes": p.stat().st_size, "sha256": sha(p)}
            for p in sorted(out.rglob("*"), key=lambda p: p.relative_to(out).as_posix())
            if p.is_file() and p != out / "manifest.json"]


def audit(out):
    expected = json.loads((out / "manifest.json").read_text())
    actual = {"schema_version": 1, "evidence_kind": "read_only_platform_inventory", "files": all_files(out)}
    if expected != actual:
        raise ValueError("evidence manifest mismatch")
    result = json.loads((out / "result.json").read_text())
    if result["compile_status"] != "unexecuted" or result["test_run_status"] != "unexecuted":
        raise ValueError("inventory incorrectly claims compile/test execution")
    return {"status": "audit_passed", "files": len(actual["files"]),
            "inventory_status": result["status"], "compile_status": "unexecuted",
            "test_run_status": "unexecuted"}


class Probe:
    def __init__(self, out):
        self.out = out
        self.commands = []

    def command(self, argv, timeout=15, env=None):
        argv = [str(a) for a in argv]
        started = time.monotonic()
        record = {"argv": argv, "cwd": str(ROOT), "started_at": now(), "timeout_seconds": timeout}
        try:
            p = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True,
                               errors="replace", timeout=timeout, env=env)
            record.update(returncode=p.returncode, stdout=p.stdout, stderr=p.stderr, status="exited")
        except subprocess.TimeoutExpired as error:
            def decode(value):
                return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
            record.update(returncode=None, stdout=decode(error.stdout), stderr=decode(error.stderr),
                          status="timeout")
        except OSError as error:
            record.update(returncode=None, stdout="", stderr=str(error), status="spawn_error")
        record.update(finished_at=now(), elapsed_seconds=time.monotonic() - started)
        self.commands.append(record)
        write_new(self.out / "commands" / (str(len(self.commands)).zfill(3) + ".json"), record)
        return record

    def capture_inputs(self):
        names = [str(SELF.relative_to(ROOT)), "docs/VM_PLATFORM_FOLLOWUP.md",
                 "docs/VM_WINDOWS_HANDOFF.md", "docs/VM_FOLLOWUP_ORCHESTRATION.md",
                 "docs/VM_NEXT_STAGE_PLAN.md", "docs/VM_NETWORK_PLAN.md",
                 "openspec/changes/vm-next-stage-hardening/tasks.md", "toolchains/vm-validation.json",
                 "cmake/XOffsetDependencyLock.cmake", "xoffsetdatastructure.hpp",
                 "xoffset_virtual_memory.hpp", ZIP_REL, "experiments/vm_windows/artifacts/SHA256SUMS"]
        identities = {}
        for relative in names:
            source = ROOT / relative
            identities[relative] = file_identity(source)
            target = self.out / "inputs" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with open(target, "xb") as copied:
                copied.write(source.read_bytes())
        return identities

    def tools(self, manifest):
        names = ("clang++", "clang-cl", "cmake", "ctest", "ninja", "python3", "docker", "orbctl",
                 "ssh", "cl", "nmake", "pwsh", "powershell", "lld-link")
        paths = {name: shutil.which(name) for name in names}
        versions = {}
        for name in ("clang++", "clang-cl", "cmake", "ctest", "ninja", "python3", "docker"):
            if paths[name]:
                versions[name] = self.command([paths[name], "--version"])
        expected = manifest["profiles"]["macos-p2996"]
        compiler = file_identity(paths["clang++"]) if paths["clang++"] else {"exists": False}
        compiler["matches_frozen_binary"] = compiler.get("sha256") == expected["compiler_binary_sha256_observed"]
        compiler["expected_commit_in_version"] = expected["compiler_commit"] in versions.get("clang++", {}).get("stdout", "")
        compiler["expected_target_in_version"] = expected["target"] in versions.get("clang++", {}).get("stdout", "")
        resource_roots = set()
        if paths["clang++"]:
            resource = self.command([paths["clang++"], "-print-resource-dir"])
            if resource["returncode"] == 0:
                resource_roots.add(Path(resource["stdout"].strip()))
        developer = self.command(["xcode-select", "--print-path"])
        sdk = self.command(["xcrun", "--show-sdk-path"])
        self.command(["xcrun", "--show-sdk-version"])
        apple = self.command(["xcrun", "--find", "clang++"])
        if apple["returncode"] == 0:
            resource = self.command([apple["stdout"].strip(), "-print-resource-dir"])
            if resource["returncode"] == 0:
                resource_roots.add(Path(resource["stdout"].strip()))
        clang_roots = {Path("/usr/local/lib/clang"),
                       Path("/Library/Developer/CommandLineTools/usr/lib/clang")}
        if developer["returncode"] == 0:
            dev = Path(developer["stdout"].strip())
            clang_roots.update((dev / "usr/lib/clang",
                               dev / "Toolchains/XcodeDefault.xctoolchain/usr/lib/clang"))
        for app in Path("/Applications").glob("Xcode*.app"):
            clang_roots.add(app / "Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang")
        tsan_paths = set()
        for root in resource_roots:
            tsan_paths.update(root.glob("lib/darwin/libclang_rt.tsan_osx_dynamic.dylib"))
        for root in clang_roots:
            tsan_paths.update(root.glob("*/lib/darwin/libclang_rt.tsan_osx_dynamic.dylib"))
        runtime_candidates = {
            "libc++.1.0.dylib": [Path("/usr/local/lib/libc++.1.0.dylib")],
            "libc++abi.1.0.dylib": [Path("/usr/local/lib/libc++abi.1.0.dylib")],
            "libclang_rt.tsan_osx_dynamic.dylib": sorted(tsan_paths),
        }
        runtimes = {}
        for name, candidates in runtime_candidates.items():
            entries = [file_identity(p) for p in candidates]
            expected_sha = expected["runtime_sha256_observed"][name]
            runtimes[name] = {"expected_sha256": expected_sha, "candidates": entries,
                              "matching_paths": [entry["path"] for entry in entries
                                                 if entry.get("sha256") == expected_sha]}
        frozen_sdk = Path("/Library/Developer/CommandLineTools/SDKs") / expected["sdk"]
        return {"paths": paths, "compiler": compiler, "runtimes": runtimes,
                "tsan_search_roots": [str(p) for p in sorted(resource_roots | clang_roots)],
                "sdk": {"default_path": sdk["stdout"].strip() if sdk["returncode"] == 0 else None,
                        "frozen_path": str(frozen_sdk), "frozen_exists": frozen_sdk.is_dir(),
                        "frozen_resolved_path": str(frozen_sdk.resolve()),
                        "settings": [file_identity(frozen_sdk / name)
                                     for name in ("SDKSettings.json", "SDKSettings.plist")],
                        "requirement": "pass frozen SDK path explicitly; do not inherit xcrun default"}}

    def dependencies(self):
        lock = (ROOT / "cmake/XOffsetDependencyLock.cmake").read_text()
        records = []
        for relative, expected in re.findall(r'"(external/[^"|]+)\|([a-f0-9]{64})"', lock):
            path = ROOT / relative
            if path.is_dir():
                files = sorted((p for p in path.rglob("*") if p.is_file()),
                               key=lambda p: p.relative_to(path).as_posix())
                content = "".join(p.relative_to(path).as_posix() + " " + sha(p) + "\n" for p in files)
                actual = hashlib.sha256(content.encode()).hexdigest()
                record = {"path": relative, "file_count": len(files), "sha256": actual}
            else:
                record = {"path": relative, **file_identity(path)}
                actual = record.get("sha256")
            record.update(expected_sha256=expected, matches=actual == expected)
            records.append(record)
        return {"sort_rule": "lexical full relative path, matching CMake list(SORT)",
                "checks": records, "all_match": all(r["matches"] for r in records)}

    def windows_zip(self, manifest):
        path = ROOT / ZIP_REL
        archive_sha = sha(path)
        sum_lines = (ROOT / "experiments/vm_windows/artifacts/SHA256SUMS").read_text().splitlines()
        expected_sha = next(line.split()[0] for line in sum_lines if line.split()[-1] == path.name)
        errors = []
        members = []
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            names = [i.filename for i in infos]
            if len(names) != len(set(names)):
                errors.append("duplicate ZIP members")
            bundle = json.loads(archive.read("bundle-manifest.json"))
            source_bytes = archive.read("source-manifest.json")
            source = json.loads(source_bytes)
            source_sha = hashlib.sha256(source_bytes).hexdigest()
            expected_names = set(bundle["files"]) | {"bundle-manifest.json"}
            if set(names) != expected_names:
                errors.append("ZIP member set differs from bundle manifest")
            for info in infos:
                data = archive.read(info.filename)
                actual = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                members.append({"path": info.filename, **actual})
                if info.filename != "bundle-manifest.json" and actual != bundle["files"].get(info.filename):
                    errors.append("bundle hash/length mismatch: " + info.filename)
            source_names = {name.removeprefix("source/") for name in names if name.startswith("source/")}
            if source_names != set(source):
                errors.append("source member set differs from frozen source manifest")
            for name, entry in source.items():
                if entry != bundle["files"].get("source/" + name):
                    errors.append("source/bundle record mismatch: " + name)
            if source_sha != WINDOWS_MANIFEST_SHA:
                errors.append("frozen source manifest SHA mismatch")
            if bundle["source_archive_sha256"] != WINDOWS_ARCHIVE_SHA:
                errors.append("frozen source archive identity mismatch")
            if archive_sha != expected_sha:
                errors.append("ZIP SHA256SUMS mismatch")
            if bundle["source_file_count"] != len(source):
                errors.append("source count mismatch")
            if bundle["source_bytes"] != sum(entry["bytes"] for entry in source.values()):
                errors.append("source byte count mismatch")
            for name, expected in manifest["baseline"]["headers"].items():
                if source[name]["sha256"] != expected:
                    errors.append("frozen F header mismatch: " + name)
            crc_bad = archive.testzip()
            if crc_bad:
                errors.append("ZIP CRC failure: " + crc_bad)
        return {"status": "passed" if not errors else "failed", "errors": errors,
                "path": ZIP_REL, "sha256": archive_sha, "expected_sha256": expected_sha,
                "bytes": path.stat().st_size, "member_count": len(members), "members": members,
                "source_manifest_sha256": source_sha, "source_archive_sha256": bundle["source_archive_sha256"],
                "source_file_count": bundle["source_file_count"], "source_bytes": bundle["source_bytes"],
                "bundle_status": bundle["status"], "exact_members": set(names) == expected_names,
                "crc_bad_member": crc_bad, "compile_status": "unexecuted", "test_run_status": "unexecuted"}

    def local_virtualization(self, tools, manifest):
        result = {"native_independent_linux_performance": "not_verified",
                  "linux_arm64_vm": "not_probed", "amd64_image_execution": "unexecuted"}
        orb = tools["paths"]["orbctl"]
        if orb:
            result["orb_status"] = self.command([orb, "status"])
            result["orb_list"] = self.command([orb, "list"])
            result["linux_arm64_vm"] = "inventory_only_virtualized_not_entered"
        docker = tools["paths"]["docker"]
        if not docker:
            result["docker_status"] = "cli_unavailable"
            return result
        context = self.command([docker, "context", "show"])
        if context["returncode"] != 0:
            result["docker_status"] = "context_unavailable"
            return result
        endpoint = self.command([docker, "context", "inspect", context["stdout"].strip(),
                                 "--format", "{{json .Endpoints.docker.Host}}"])
        if endpoint["returncode"] != 0:
            result["docker_status"] = "endpoint_unavailable"
            return result
        address = os.environ.get("DOCKER_HOST") or json.loads(endpoint["stdout"])
        if not address.startswith("unix://"):
            result["docker_status"] = "skipped_non_unix_endpoint_no_remote_contact"
            return result
        socket_path = Path(address.removeprefix("unix://"))
        if not socket_path.exists() or not stat.S_ISSOCK(socket_path.stat().st_mode):
            result["docker_status"] = "local_socket_unavailable"
            return result
        local_env = {key: value for key, value in os.environ.items() if key not in ("DOCKER_HOST", "DOCKER_CONTEXT")}
        prefix = [docker, "--host", address]
        info_format = ('{"ID":{{json .ID}},"OSType":{{json .OSType}},"Architecture":{{json .Architecture}},'
                       '"OperatingSystem":{{json .OperatingSystem}},"KernelVersion":{{json .KernelVersion}},'
                       '"NCPU":{{json .NCPU}},"MemTotal":{{json .MemTotal}},"ServerVersion":{{json .ServerVersion}}}')
        info = self.command(prefix + ["info", "--format", info_format], env=local_env)
        result["docker_status"] = "local_unix_socket_inventory_only" if info["returncode"] == 0 else "daemon_unavailable"
        if info["returncode"] == 0:
            result["docker_identity"] = json.loads(info["stdout"])
        image_format = '{"Id":{{json .Id}},"Os":{{json .Os}},"Architecture":{{json .Architecture}},"RepoDigests":{{json .RepoDigests}}}'
        image = self.command(prefix + ["image", "inspect", manifest["profiles"]["linux-p2996"]["image"],
                                      "--format", image_format], env=local_env)
        if image["returncode"] == 0:
            result["known_p2996_image"] = json.loads(image["stdout"])
            result["known_p2996_image"]["classification"] = "amd64 image on ARM64 host; cross-architecture emulation if executed"
        ps_format = '{"ID":{{json .ID}},"Names":{{json .Names}},"Image":{{json .Image}},"State":{{json .State}}}'
        result["running_container_inventory"] = self.command(prefix + ["ps", "--format", ps_format], env=local_env)
        return result

    def ssh_alias(self):
        alias = "sfcdev"
        config = Path.home() / ".ssh/config"
        allowed_include = Path.home() / ".orbstack/ssh/config"
        review = {"alias": alias, "config_contents_archived": False, "credential_files_read": False,
                  "explicit_alias_present": False, "blocked_directives": [], "included_config_count": 0}
        if not config.is_file():
            return {"status": "prerequisite_unavailable", "reason": "existing SSH config missing", "review": review}
        active = True

        def inspect(path, depth=0):
            nonlocal active
            if depth > 1:
                review["blocked_directives"].append("nested_include")
                return
            for line in path.read_text().splitlines():
                fields = shlex.split(line, comments=True)
                if not fields:
                    continue
                key = fields[0].lower()
                if key == "include":
                    for value in fields[1:]:
                        include = Path(os.path.expanduser(value))
                        if include != allowed_include or not include.is_file():
                            review["blocked_directives"].append("unreviewed_include")
                        else:
                            review["included_config_count"] += 1
                            inspect(include, depth + 1)
                elif key == "host":
                    patterns = fields[1:]
                    if alias in patterns:
                        review["explicit_alias_present"] = True
                    positive = any(not p.startswith("!") and fnmatch.fnmatchcase(alias, p) for p in patterns)
                    negative = any(p.startswith("!") and fnmatch.fnmatchcase(alias, p[1:]) for p in patterns)
                    active = positive and not negative
                elif key == "match":
                    review["blocked_directives"].append("match_not_executed")
                elif active and key in ("proxycommand", "proxyjump", "localcommand", "remotecommand"):
                    review["blocked_directives"].append(key)

        inspect(config)
        if review["blocked_directives"] or not review["explicit_alias_present"]:
            return {"status": "prerequisite_unavailable", "reason": "static config review did not authorize a direct bounded probe",
                    "review": review}
        ssh = shutil.which("ssh")
        if not ssh:
            return {"status": "prerequisite_unavailable", "reason": "SSH client missing", "review": review}
        remote = ('uname -a\nif [ -r /etc/os-release ]; then cat /etc/os-release; fi\n'
                  'for tool in clang++ clang g++ gcc c++ cmake ctest ninja make python3; do\n'
                  'printf "%s=" "$tool"\ncommand -v "$tool" || :\ndone')
        argv = [ssh, "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", "-o", "ConnectionAttempts=1",
                "-o", "StrictHostKeyChecking=yes", "-o", "UpdateHostKeys=no", "-o", "PermitLocalCommand=no",
                "-o", "ClearAllForwardings=yes", "-o", "ForwardAgent=no", "-o", "ForwardX11=no",
                "-o", "ControlMaster=no", "-o", "ControlPath=none", "-o", "AddKeysToAgent=no",
                "-o", "UseKeychain=no", alias, "sh -c " + shlex.quote(remote)]
        record = self.command(argv, timeout=12)
        return {"status": "identity_tool_paths_observed" if record["returncode"] == 0 else "prerequisite_unavailable",
                "reason": "one bounded noninteractive attempt; no retry or credential retrieval",
                "review": review, "command_record": len(self.commands), "returncode": record["returncode"],
                "compile_status": "unexecuted", "test_run_status": "unexecuted",
                "native_independent_linux_performance": "not_verified"}

    def run(self):
        before = self.capture_inputs()
        manifest = json.loads((ROOT / "toolchains/vm-validation.json").read_text())
        result = {"schema_version": 1, "status": "ready_for_review", "started_at": now(),
                  "evidence_kind": "read_only_platform_inventory", "compile_status": "unexecuted",
                  "test_run_status": "unexecuted", "performance_status": "unexecuted", "g4": "waiting_for_orchestrator",
                  "local_platform": {"uname": dict(platform.uname()._asdict()), "python": sys.version,
                                     "hostname": socket.gethostname(), "process_architecture": platform.machine()},
                  "inputs_before": before}
        for argv in (["uname", "-a"], ["sw_vers"], ["sysctl", "-n", "hw.machine", "hw.model", "hw.ncpu"]):
            self.command(argv)
        result["tools"] = self.tools(manifest)
        result["dependencies"] = self.dependencies()
        result["windows_zip"] = self.windows_zip(manifest)
        result["local_virtualization"] = self.local_virtualization(result["tools"], manifest)
        result["configured_ssh_alias"] = self.ssh_alias()
        result["windows"] = {
            "status": "prerequisite_unavailable", "compile_status": "unexecuted", "test_run_status": "unexecuted",
            "reason": "current environment is macOS; native Windows x64 execution has not been provided or demonstrated",
            "required": ["native Windows x64 access", "MSVC x64 Developer PowerShell with C++ workload and Windows SDK",
                         "Python >=3.9", "CMake and CTest >=3.21", "NMake, or preinstalled Ninja when selected",
                         "new results directory outside the frozen package and complete evidence return"],
            "expected_test_counts": {"storage": 2, "installed_consumer": 3, "subproject_consumer": 3},
            "full_p2996_status": "separate prerequisite_unavailable; this storage package omits full-library dependencies"}
        after = {name: file_identity(ROOT / name) for name in before}
        result["inputs_after"] = after
        result["input_changes"] = [name for name in before if before[name] != after[name]]
        result["current_f_matches"] = all(after[name].get("sha256") == expected
                                           for name, expected in manifest["baseline"]["headers"].items())
        if (result["windows_zip"]["status"] != "passed" or not result["dependencies"]["all_match"]
                or not result["current_f_matches"] or result["input_changes"]):
            result["status"] = "review_required"
        result.update(finished_at=now(), command_count=len(self.commands),
                      integrity_claim="only ZIP/source/dependency bytes audited; no platform compile/run result")
        write_new(self.out / "result.json", result)
        write_new(self.out / "manifest.json", {"schema_version": 1, "evidence_kind": "read_only_platform_inventory",
                                                "files": all_files(self.out)})
        return {"status": result["status"], "out": str(self.out), "command_count": len(self.commands),
                "windows_zip": result["windows_zip"]["status"], "compile_status": "unexecuted",
                "test_run_status": "unexecuted", "manifest_sha256": sha(self.out / "manifest.json")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--out", type=Path)
    group.add_argument("--audit", type=Path)
    args = parser.parse_args()
    if args.audit:
        print(json.dumps(audit(args.audit.resolve()), sort_keys=True))
        return 0
    out = args.out.expanduser().absolute()
    if os.path.lexists(out):
        parser.error("--out must be a new directory; existing evidence is never overwritten")
    out.mkdir(parents=True, exist_ok=False)
    print(json.dumps(Probe(out).run(), sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
