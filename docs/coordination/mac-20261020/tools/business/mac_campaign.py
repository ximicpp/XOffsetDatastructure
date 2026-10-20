#!/usr/bin/env python3
"""macOS arm64 adaptation layer for the restore serialization_comparison campaign.py.

This file never edits campaign.py. It loads the byte-identical copy under
meta-<id>/tools/serialization_comparison/ (only the xoffset identity label was
changed in protocol.py, see meta.patch) via importlib, then applies a short,
explicit list of environment patches before delegating to campaign.main().

Untouched campaign logic (by design): prepare, fixture_check, functional_jobs,
functional_argv, functional_output, execute, launch_preflight, native_result,
functional, queue_audit, _receipt, audit, seal/check_inventory, postrun_pin,
freeze/run/report.

Selection of the meta copy is by environment variable, because campaign's
queue checks compare argv exactly (no extra flags are allowed):
    SC_MAC_META=c14   -> meta-c14   (label xoffset-receive-completion-v1)
    SC_MAC_META=r1m2  -> meta-r1m2  (label xoffset-c14-r1-m2-combined-v1)

Interpreter: Python >= 3.10 is required (model/reference.py uses
bisect_left(key=...)); /usr/bin/python3 (3.9) is rejected. Launch with -B only
(campaign.active_queue accepts exactly [python, (-B), adapter, argv...]).

PATCH LIST (each also recorded in MAC_PATCHES and printed by `--mac-patches`):
  M1 RUNNER -> runner/experiments/vm_coordination/run_job.py (native stand-in for
     the absent WSL queue supervisor; same request/lock/result/manifest
     protocol). Its location makes RUNNER.parents[2]/build/vm-coordination the
     lock_dir, so the campaign's lock_dir equality check is left unmodified.
  M2 campaign.__file__ -> this file. active_queue() requires the queue command to
     be [python, -B, <campaign __file__>, *argv]; the real entry point is this
     adapter, so the recorded adapter_path is honest. HERE (tool directory) was
     already bound at import time and is unaffected.
  M3 tools_identity()/save_tools(): the tool tuple additionally contains this
     file (MAC_ADAPTER), so the patches themselves are hashed into
     runtime-before/after, execution-context and archived under protocol/.
     queue_audit compares context tools with protocol/ members, so it stays
     consistent without modification.
  M4 active_queue(): platform gate 'Linux + microsoft kernel (WSL2)' replaced by
     'Darwin + arm64'. Everything else in the lease check is unchanged.
  M5 read_profile(): platform_class 'wsl2-vm-x86_64' replaced by
     'macos-arm64-native'. All other profile validation unchanged.
  Not patched because campaign.py does not contain them (they lived in the
  20261020-v2 orchestration workers and are replaced by make_profiles.py /
  build_bench.sh): taskset CPU pinning (no macOS equivalent -> prefix []),
  ld-linux loader prefix (dyld + LC_RPATH -> prefix []), ldd/readelf closure
  discovery (-> otool -L / otool -l in make_profiles.py).
  A1 (additive, outside campaign logic) functional(): before delegating, verify
     the live xoffset production files named by the profile closure
     ('candidate-file/<name>') reproduce the expected source_content_sha256 for
     the protocol label; afterwards write <out>.mac-identity.json NEXT TO (not
     inside) the sealed bundle.
  A2 (additive) audit(): for kind sc-functional, additionally recompute the
     content digest from the *captured* runtime-before/after closure records
     (no original path is opened), mirroring final-v4's
     candidate_runtime_identity. The original audit runs first and unchanged.
"""
from __future__ import annotations

import hashlib
import importlib.util
import inspect
import json
import os
from pathlib import Path
import sys
import textwrap

sys.dont_write_bytecode = True

MAC_ADAPTER = Path(__file__).resolve()
BUSINESS = MAC_ADAPTER.parent
MAC_RUNNER = BUSINESS / "runner/experiments/vm_coordination/run_job.py"
METAS = {"c14": BUSINESS / "meta-c14", "r1m2": BUSINESS / "meta-r1m2", "w9a": BUSINESS / "meta-w9a"}
# Expected xoffset production-content identity per protocol label (see
# identities/*.json; algorithm from combined_full/business/prepare.py).
EXPECTED_CONTENT = {
    "xoffset-receive-completion-v1": "26299f796e2ba289ab15b38b000a785ad1877fbc65ce29cf3071d2118804c3b4",
    "xoffset-c14-r1-m2-combined-v1": "8cd90345f109308d150caa141e49fa65e2e7b1fac957957e8e45bc8ee8eb5518",
    "xoffset-mac-w9-flat-v1": "6ce627e5bae69329ac928b6c80b9920930b693b6543449464998778cdfcc89fb",
}
EXPECTED_MAIN_HEADER = {
    "xoffset-receive-completion-v1": "66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3",
    "xoffset-c14-r1-m2-combined-v1": "155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501",
    "xoffset-mac-w9-flat-v1": "a1593020d4a928086f7788ed4ff06c0a32ce2ee777c15fbbbf875f3999273f5d",
}
PLATFORM_CLASS = "macos-arm64-native"
CANDIDATE_PREFIX = "candidate-file/"
MAC_PATCHES = []


def _fail(message):
    print(json.dumps(dict(status="rejected", reason="mac_campaign: " + message, performance_accepted=False), sort_keys=True))
    raise SystemExit(1)


def _load_campaign():
    if sys.version_info < (3, 10):
        _fail("Python >= 3.10 required (reference.py uses bisect key=); got " + sys.version.split()[0])
    meta_name = os.environ.get("SC_MAC_META")
    if meta_name not in METAS:
        _fail("set SC_MAC_META to one of " + ",".join(sorted(METAS)))
    tools = METAS[meta_name] / "tools/serialization_comparison"
    for name in ("core", "protocol"):
        if name in sys.modules:
            _fail("module name collision: " + name)
    sys.path.insert(0, str(tools))
    spec = importlib.util.spec_from_file_location("sc_campaign_mac", tools / "campaign.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    if Path(module.P.__file__).resolve().parent != tools.resolve() or Path(module.C.__file__).resolve().parent != tools.resolve():
        _fail("campaign did not bind the selected meta core/protocol")
    return meta_name, module


def _rewrite(module, name, replacements, why):
    """Recompile one campaign function from its own source with exact, counted edits."""
    source = textwrap.dedent(inspect.getsource(getattr(module, name)))
    for old, new in replacements:
        if source.count(old) != 1:
            _fail("patch anchor not unique in %s: %r" % (name, old))
        source = source.replace(old, new)
    exec(compile(source, "<mac_campaign patched %s>" % name, "exec"), module.__dict__)
    MAC_PATCHES.append(dict(function=name, replacements=[list(r) for r in replacements], reason=why))


def _content_digest(records):
    return hashlib.sha256(json.dumps(records, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def _check_records(label, records, where):
    C = CAMPAIGN.C
    C.require(label in EXPECTED_CONTENT, "unknown xoffset protocol label: " + label)
    C.require(len(records) >= 14 and "xoffsetdatastructure.hpp" in records, where + ": incomplete production closure")
    C.require(records["xoffsetdatastructure.hpp"]["sha256"] == EXPECTED_MAIN_HEADER[label], where + ": main header differs")
    digest = _content_digest(records)
    C.require(digest == EXPECTED_CONTENT[label], where + ": source_content_sha256 differs: " + digest)
    return digest


def _live_records(profile):
    result = {}
    for key, path in profile["closure"].items():
        if key.startswith(CANDIDATE_PREFIX):
            identity = CAMPAIGN.C.file_id(path)
            result[key[len(CANDIDATE_PREFIX):]] = dict(bytes=int(identity["bytes"]), sha256=identity["sha256"])
    return result


def _captured_records(runtime):
    C = CAMPAIGN.C
    C.require(type(runtime) is dict and type(runtime.get("closure")) is dict, "captured closure missing")
    result = {}
    for key, tree in runtime["closure"].items():
        if key.startswith(CANDIDATE_PREFIX):
            C.require(type(tree) is dict and set(tree) == {"."}, "candidate closure entry must be one regular file: " + key)
            result[key[len(CANDIDATE_PREFIX):]] = dict(bytes=int(tree["."]["bytes"]), sha256=tree["."]["sha256"])
    return result


def _install_patches(module):
    global CAMPAIGN
    CAMPAIGN = module
    module.MAC_ADAPTER = MAC_ADAPTER
    # M1
    module.RUNNER = MAC_RUNNER
    MAC_PATCHES.append(dict(global_name="RUNNER", value=str(MAC_RUNNER), reason="M1 native queue supervisor stand-in"))
    # M3
    old_tuple = '(HERE/"core.py", HERE/"protocol.py", HERE/"campaign.py", RUNNER)'
    new_tuple = '(HERE/"core.py", HERE/"protocol.py", HERE/"campaign.py", RUNNER, MAC_ADAPTER)'
    _rewrite(module, "tools_identity", [(old_tuple, new_tuple)], "M3 bind mac_campaign.py into tool identity")
    _rewrite(module, "save_tools", [(old_tuple, new_tuple)], "M3 archive mac_campaign.py under protocol/")
    # M4
    _rewrite(module, "active_queue", [(
        'C.require(platform.system() == "Linux" and "microsoft" in platform.release().lower(), "native execution requires the registered WSL2 profile")',
        'C.require(platform.system() == "Darwin" and platform.machine() == "arm64", "native execution requires the registered macOS arm64 profile")')],
        "M4 platform gate WSL2 -> Darwin arm64")
    # M5
    _rewrite(module, "read_profile", [(
        'value["platform_class"] == "wsl2-vm-x86_64"', 'value["platform_class"] == %r' % PLATFORM_CLASS)],
        "M5 profile platform_class")
    # M2 (after all inspect.getsource calls; code objects keep their own filename)
    module.__file__ = str(MAC_ADAPTER)
    MAC_PATCHES.append(dict(global_name="__file__", value=str(MAC_ADAPTER), reason="M2 queue argv/adapter_path names the real entry"))

    original_functional = module.functional
    original_audit = module.audit

    def functional(prepared, profile_path, out, queue):  # A1
        C = module.C
        profile = module.read_profile(C.absolute(profile_path))
        label = module.P.IDENTITIES["xoffset"][0]
        C.require(profile["binaries"]["xoffset"]["library"] == label, "profile/protocol xoffset label differs")
        before = _live_records(profile)
        digest = _check_records(label, before, "live pre-run")
        status = "exception"
        try:
            result = original_functional(prepared, profile_path, out, queue)
            status = "returned"
            return result
        finally:
            after = _live_records(profile)
            sidecar = C.absolute(out).parent / (C.absolute(out).name + ".mac-identity.json")
            C.write(sidecar, dict(schema_version=1, kind="mac-business-identity-sidecar", label=label,
                                  source_content_sha256=digest, production_files=before,
                                  unchanged_after=after == before, campaign_call=status,
                                  platform_class=PLATFORM_CLASS, patches=MAC_PATCHES,
                                  adapter=C.file_id(MAC_ADAPTER), runner=C.file_id(MAC_RUNNER)))

    def audit(out, external_sha=None, bundle_sha=None):  # A2
        result = original_audit(out, external_sha, bundle_sha)
        C = module.C
        status = C.read(C.absolute(out) / "status.json")
        if status.get("kind") == "sc-functional":
            label = module.P.IDENTITIES["xoffset"][0]
            profile = C.read(C.absolute(out) / "profile.json")
            C.require(profile["binaries"]["xoffset"]["library"] == label, "archived profile label differs")
            records = _captured_records(C.read(C.absolute(out) / "runtime-before.json"))
            _check_records(label, records, "captured runtime-before")
            after = C.read(C.absolute(out) / "runtime-after.json")
            if not (type(after) is dict and set(after) == {"identity_error"}):
                C.require(_captured_records(after) == records, "captured runtime-after candidate closure differs")
        return result

    module.functional = functional
    module.audit = audit
    MAC_PATCHES.append(dict(function="functional", reason="A1 additive live content-identity precheck + external sidecar"))
    MAC_PATCHES.append(dict(function="audit", reason="A2 additive captured content-identity check for sc-functional"))


CAMPAIGN = None


def main():
    meta_name, module = _load_campaign()
    _install_patches(module)
    if sys.argv[1:] == ["--mac-patches"]:
        print(json.dumps(dict(meta=meta_name, label=module.P.IDENTITIES["xoffset"][0], patches=MAC_PATCHES), indent=2))
        return 0
    return module.main()


if __name__ == "__main__":
    raise SystemExit(main())
