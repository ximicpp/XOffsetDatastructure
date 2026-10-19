#!/usr/bin/env python3
"""Prepare the observed Linux x86_64 P2996 binary's private userspace.

Does not install packages, replace system libraries, or change sanitizer guards.
The pre-existing compiler is an explicit input, not a downloaded compiler claim.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import urllib.request

PACKAGES = {
    "libc6": ("https://archive.ubuntu.com/ubuntu/pool/main/g/glibc/libc6_2.39-0ubuntu8_amd64.deb", "af36c7ac770770fe3d3c10e85d6bc538e76e57570ba7db7d397fb9f654783ef3"),
    "libc6-dev": ("https://archive.ubuntu.com/ubuntu/pool/main/g/glibc/libc6-dev_2.39-0ubuntu8_amd64.deb", "1f03b79223d9cf1a6ed3a648f6942619dcb97b2884dba8fd12e262b6771e247a"),
    "libgcc-s1": ("https://archive.ubuntu.com/ubuntu/pool/main/g/gcc-14/libgcc-s1_14-20240412-0ubuntu1_amd64.deb", "a39efdcaa2245f026dc3254ce14fcff255fc430a17064632b6ba7c5da974a912"),
    "libstdc++6": ("https://archive.ubuntu.com/ubuntu/pool/main/g/gcc-14/libstdc++6_14-20240412-0ubuntu1_amd64.deb", "4863e880d6ee538e4734c430c5579c1ac933b88a42c25bc9980d0f148c54b21e"),
    "compiler-rt21": ("https://apt.llvm.org/noble/pool/main/l/llvm-toolchain-21/libclang-rt-21-dev_21.1.8~++20251221032922+2078da43e25a-1~exp1~20251221153059.70_amd64.deb", "d6f84c34953b0404ec9648ba7364004a82454f79cc447f67894b9a27317ef6ce"),
}
COMPILER_SHA256 = "c7fa55437d34c43201da37960251d7d3dab1e06fc92273efe6f6d88f6dae7c6e"


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def wrapper_text(loader, host_lib, compiler, wrapper, prefix):
    target_lib = prefix / "lib/x86_64-unknown-linux-gnu"
    invocation = [str(loader), "--argv0", str(wrapper), "--library-path", str(host_lib),
                  str(compiler), "-no-canonical-prefixes", "--driver-mode=g++",
                  "-resource-dir=" + str(prefix / "lib/clang/21"),
                  "-isystem", str(prefix / "include/x86_64-unknown-linux-gnu/c++/v1"),
                  "-isystem", str(prefix / "include/c++/v1"),
                  "-L" + str(target_lib), "-L" + str(host_lib),
                  "-Wl,--dynamic-linker=" + str(loader),
                  "-Wl,-rpath," + str(host_lib) + ":" + str(target_lib)]
    cc1 = [str(loader), "--library-path", str(host_lib), str(compiler)]
    return ("#!/bin/sh\n"
            'if [ "$1" = "-cc1" ] || [ "$1" = "-cc1as" ]; then\n'
            "  exec " + shlex.join(cc1) + ' "$@"\nfi\n'
            "exec " + shlex.join(invocation) + ' "$@"\n')


def linker_group(paths, as_needed=()):
    quoted = lambda path: json.dumps(str(path), ensure_ascii=False)
    optional = " AS_NEEDED ( " + " ".join(map(quoted, as_needed)) + " )" if as_needed else ""
    return "/* Private GNU ld script. */\nGROUP ( " + " ".join(map(quoted, paths)) + optional + " )\n"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--compiler-root", type=Path, required=True)
    p.add_argument("--package-dir", type=Path, help="Optional existing .deb cache")
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args(argv)
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        p.error("requires Linux x86_64")
    out = args.out.resolve()
    if out.exists():
        p.error("--out must be a new directory")
    prefix = args.compiler_root.resolve()
    compiler = prefix / "bin/clang++"
    if sha(compiler) != COMPILER_SHA256:
        p.error("compiler differs from this observed profile; do not relabel another compiler")
    out.mkdir(parents=True)
    record = {"schema_version": 1, "profile": "linux-wsl2-current-complete-20261016",
              "status": "running", "compiler": str(compiler), "compiler_sha256": sha(compiler),
              "packages": {}, "system_libraries_changed": False}
    try:
        for name, (url, expected) in PACKAGES.items():
            filename = url.rsplit("/", 1)[1]
            cached = args.package_dir / filename if args.package_dir else None
            target = out / filename
            if cached and cached.is_file():
                shutil.copy2(cached, target)
            else:
                with urllib.request.urlopen(url, timeout=60) as source, target.open("xb") as sink:
                    shutil.copyfileobj(source, sink)
            if sha(target) != expected:
                raise ValueError("package SHA-256 mismatch: " + name)
            destination = out / ("sanitizer-package" if name == "compiler-rt21" else "host-runtime")
            subprocess.run(["dpkg-deb", "-x", str(target), str(destination)], check=True)
            record["packages"][name] = {"url": url, "sha256": expected}
        lib = out / "host-runtime/usr/lib/x86_64-linux-gnu"
        loader = lib / "ld-linux-x86-64.so.2"
        # The distribution script contains system-absolute paths. Replace only
        # our extracted copy with paths to these same-version private inputs.
        (lib / "libc.so").write_text(linker_group([lib / "libc.so.6", lib / "libc_nonshared.a"], [loader]))
        (lib / "libm.so").write_text(linker_group([lib / "libm.so.6", lib / "libmvec.so.1"]))
        runtime = out / "sanitizer-resource"
        shutil.copytree(prefix / "lib/clang/21/include", runtime / "include", symlinks=False)
        shutil.copytree(out / "sanitizer-package/usr/lib/llvm-21/lib/clang/21/lib", runtime / "lib", symlinks=False)
        wrapper = out / "bin/clang++"
        wrapper.parent.mkdir()
        wrapper.write_text(wrapper_text(loader, lib, compiler, wrapper, prefix))
        wrapper.chmod(0o755)
        version = subprocess.check_output([str(wrapper), "--version"], text=True)
        record.update(wrapper=str(wrapper), wrapper_sha256=sha(wrapper), version=version,
                      resource_dir=str(prefix / "lib/clang/21"), sanitizer_resource=str(runtime),
                      host_runtime=str(lib), loader=str(loader),
                      files={str(path): sha(path) for path in [
                          compiler, wrapper, loader, lib / "libc.so.6", lib / "libc.so",
                          lib / "libc_nonshared.a", lib / "libm.so", lib / "libm.so.6",
                          lib / "libmvec.so.1", lib / "libstdc++.so.6", lib / "libgcc_s.so.1",
                          prefix / "lib/x86_64-unknown-linux-gnu/libc++.so.1.0",
                          prefix / "lib/x86_64-unknown-linux-gnu/libc++abi.so.1.0"]}, status="prepared")
        print(version)
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        record.update(status="failed", reason=str(exc))
        raise
    finally:
        (out / "toolchain.json").write_text(json.dumps(record, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
