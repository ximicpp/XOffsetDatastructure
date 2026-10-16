#!/usr/bin/env python3
"""Lightweight runner tests. No compiler, network, or native binary is executed.

Synthetic archive fixtures exercise audit rejection and do not constitute
native C++ functional/build evidence.
"""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("network_runner", Path(__file__).with_name("run_network.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.root = self.base / "repository"
        self.network = self.root / "experiments/vm_network"
        self.network.mkdir(parents=True)
        self.baseline = {}
        for name in runner.BASELINE:
            path = self.root / name
            path.write_text("synthetic audit fixture for " + name)
            self.baseline[name] = runner.sha(path)
        self.patch = mock.patch.object(runner, "BASELINE", self.baseline)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        (self.root / "LICENSE").write_text("synthetic fixture license")
        (self.root / "toolchains").mkdir()
        runner.write(self.root / "toolchains/vm-validation.json",
                     {"baseline": {"headers": self.baseline}, "dependencies": {}, "profiles": {"macos-p2996": {
                         "compiler_binary_sha256_observed": "a" * 64, "compiler_commit": "synthetic-commit",
                         "target": "synthetic-target", "sdk": "Synthetic.sdk", "runtime_sha256_observed": {}}}})
        for name in runner.NETWORK_FILES:
            (self.network / name).write_text("synthetic fixture " + name)
        shutil.copy2(runner.__file__, self.network / "run_network.py")
        self.contract = {
            "schema": "vm-network-reference-contract", "schema_version": 1, "contract_version": "1.0.0",
            "baseline_headers": self.baseline,
            "profiles": {"macro": "N7_NET_DIAGNOSTICS", "timing": {"value": 0}, "diagnostic": {"value": 1}},
            "smoke": {"checks": 3, "model_case_ids": ["model.fixture"],
                      "tcp_case_ids": ["tcp.fixture", "fixture.existing_output"],
                      "case_ids": ["model.fixture", "tcp.fixture", "fixture.existing_output"]}}
        runner.write(self.network / "contract.json", self.contract)

    def tearDown(self):
        self.temp.cleanup()

    def record(self):
        return {"schema_version": 1, "status": "running", "timing_evidence": False,
                "executed_runner_sha256": runner.sha(runner.__file__)}

    def prepare(self, name="prepared"):
        out = runner.fresh(self.base / name)
        record = self.record()
        runner.prepare(argparse.Namespace(source_root=self.root), out, record)
        return out

    def receipt(self, argv):
        return {"returncode": 0, "argv": argv,
                "coordination": {"explicit_flag": True, "lock_was_held": True,
                                 "lock_metadata": {"status": "running", "owner": "W1", "supervisor_pid": 101},
                                 "ancestor_pids": [103, 102, 101]}}

    def synthetic_build(self, profile="timing", sanitizer="none", sanitizer_resource=None):
        prepared = self.prepare()
        out = runner.fresh(self.base / "build")
        shutil.copytree(prepared / "source", out / "source")
        shutil.copy2(prepared / "source-inventory.json", out / "source-inventory.json")
        shutil.copy2(prepared / "result.json", out / "prepared-result.json")
        runner.write(out / "dependencies.json", {"root": "/synthetic/deps", "commits": {}, "files": {}})
        runner.write(out / "toolchain.json", {"profile": "macos-p2996", "host_system": "Darwin", "runtime": {},
                     "compiler": "/synthetic/compiler", "compiler_sha256": "a" * 64,
                     "version": "synthetic-commit\nTarget: synthetic-target\n", "sdk": "/synthetic/Synthetic.sdk",
                     "library": "/synthetic/lib", "sanitizer_resource": sanitizer_resource})
        (out / "net_reference").write_bytes(b"synthetic audit-only binary; not executable")
        (out / "compile.stdout").write_text("")
        (out / "compile.stderr").write_text("")
        record = self.record()
        record.update(baseline_headers=self.baseline, measurement_profile=profile, sanitizer=sanitizer, build_root="/synthetic",
                      prepared_result_sha256=runner.sha(prepared / "result.json"),
                      contract_sha256=runner.sha(out / "source/contract.json"), contract_version="1.0.0",
                      binary_sha256=runner.sha(out / "net_reference"))
        argv = ["/synthetic/compiler", "-O2", "-std=c++26", "-freflection", "-fexpansion-statements", "-stdlib=libc++",
                "-DN7_NET_DIAGNOSTICS=" + ("1" if profile == "diagnostic" else "0"), "-pthread",
                "-isysroot", "/synthetic/Synthetic.sdk", "-mmacosx-version-min=10.15", "-I", "/synthetic/source"]
        if sanitizer == "address-undefined":
            argv += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
            if sanitizer_resource is not None:
                argv += ["-resource-dir", sanitizer_resource["root"]]
        for name in ("container", "core", "config", "throw_exception", "static_assert", "container_hash",
                     "type_traits", "winapi", "intrusive", "predef"):
            argv += ["-isystem", "/synthetic/deps/boost/libs/" + name + "/include"]
        argv += ["-isystem", "/synthetic/deps/typelayout/include", "/synthetic/source/net_reference.cpp",
                 "-L/synthetic/lib", "-Wl,-rpath,/synthetic/lib", "-o", "/synthetic/net_reference"]
        record["commands"] = [self.receipt(argv)]
        runner.finish(out, record, "build")
        return out

    def report(self, diagnostic=False):
        rows = []
        for case_id, scope in (("model.fixture", "injected_model"), ("tcp.fixture", "actual_tcp"),
                               ("fixture.existing_output", "filesystem")):
            rows.append({"id": case_id, "scope": scope, "reason": "ok", "read_bytes": 0,
                         "written_bytes": 0, "receive_allocations": 0, "published": False})
        return {"schema_version": 1, "contract_version": "1.0.0", "command": "smoke", "status": "passed",
                "diagnostic": diagnostic, "timing_evidence": False, "checks": 3,
                "case_ids": list(self.contract["smoke"]["case_ids"]), "cases": rows}

    def reseal(self, out, record=None):
        record = record or runner.read(out / "result.json")
        runner.finish(out, record, record["kind"])

    def test_prepare_and_relocated_audit_without_live_source(self):
        prepared = self.prepare()
        runner.audit(prepared, "prepared")
        moved = self.base / "moved"
        shutil.move(prepared, moved)
        shutil.rmtree(self.root)
        before = runner.files(moved)
        self.assertEqual(runner.audit(moved)["kind"], "prepared")
        self.assertEqual(runner.files(moved), before)

    def test_current_non_f_header_rejected_before_copy(self):
        (self.root / "xoffsetdatastructure.hpp").write_text("wrong F")
        with self.assertRaisesRegex(ValueError, "not accepted F"):
            self.prepare()

    def test_output_must_be_new_including_dangling_symlink(self):
        existing = self.base / "existing"
        existing.mkdir()
        with self.assertRaises(ValueError):
            runner.fresh(existing)
        dangling = self.base / "dangling"
        dangling.symlink_to(self.base / "missing")
        with self.assertRaises(ValueError):
            runner.fresh(dangling)

    def test_unsafe_manifest_paths_rejected(self):
        for name in ("../outside", "/outside", "a/../outside", "a\\outside", "a:outside", "a//b", "", "."):
            with self.subTest(name=name), self.assertRaises(ValueError):
                runner.safe_relative(name)

    def test_source_symlink_rejected(self):
        source = self.network / "frame_model.hpp"
        source.unlink()
        source.symlink_to(self.network / "net_reference.cpp")
        with self.assertRaisesRegex(ValueError, "source unavailable"):
            self.prepare()

    def test_extra_missing_changed_files_rejected(self):
        for mutation in ("extra", "missing", "changed"):
            with self.subTest(mutation=mutation):
                out = self.prepare(mutation)
                if mutation == "extra":
                    (out / "unexpected").write_text("unexpected")
                elif mutation == "missing":
                    (out / "source/LICENSE").unlink()
                else:
                    (out / "source/LICENSE").write_text("changed")
                with self.assertRaises(ValueError):
                    runner.audit(out)

    def test_completion_digest_rejected(self):
        out = self.prepare()
        (out / "COMPLETE").write_text("0" * 64 + "\n")
        with self.assertRaisesRegex(ValueError, "completion identity"):
            runner.audit(out)

    def test_inventory_reseal_cannot_hide_extra_source(self):
        out = self.prepare()
        (out / "source/extra.hpp").write_text("unexpected source")
        runner.write(out / "source-inventory.json", runner.files(out / "source"))
        self.reseal(out)
        with self.assertRaisesRegex(ValueError, "source inventory"):
            runner.audit(out)

    def test_duplicate_contract_ids_rejected(self):
        self.contract["smoke"]["case_ids"][1] = "model.fixture"
        runner.write(self.network / "contract.json", self.contract)
        with self.assertRaisesRegex(ValueError, "duplicate contract"):
            self.prepare()

    def test_model_tcp_partition_rejected(self):
        self.contract["smoke"]["tcp_case_ids"].append("model.fixture")
        runner.write(self.network / "contract.json", self.contract)
        with self.assertRaisesRegex(ValueError, "partition"):
            self.prepare()

    def test_all_profiles_have_relocation_safe_build_audit(self):
        out = self.synthetic_build("diagnostic", "address-undefined")
        self.assertEqual(runner.audit(out, "build")["measurement_profile"], "diagnostic")
        moved = self.base / "moved-build"
        shutil.move(out, moved)
        shutil.rmtree(self.root)
        shutil.rmtree(self.base / "prepared")
        before = runner.files(moved)
        runner.audit(moved, "build")
        self.assertEqual(runner.files(moved), before)

    def test_profile_mismatch_rejected_despite_valid_manifest(self):
        out = self.synthetic_build()
        record = runner.read(out / "result.json")
        record["measurement_profile"] = "diagnostic"
        self.reseal(out, record)
        with self.assertRaisesRegex(ValueError, "profile differs"):
            runner.audit(out)

    def test_compiler_optimization_and_profile_overrides_rejected(self):
        out = self.synthetic_build()
        original = runner.read(out / "result.json")
        for override in (["-O0"], ["-O3"], ["-O2"], ["-std=c++23"], ["-stdlib=libstdc++"],
                         ["-UN7_NET_DIAGNOSTICS"], ["-U", "N7_NET_DIAGNOSTICS"],
                         ["-D", "N7_NET_DIAGNOSTICS=1"], ["-Xclang", "-UN7_NET_DIAGNOSTICS"],
                         ["-Wp,-UN7_NET_DIAGNOSTICS"], ["-fno-reflection"], ["-fno-sanitize=all"]):
            with self.subTest(override=override):
                changed = copy.deepcopy(original)
                changed["commands"][0]["argv"][-2:-2] = override
                self.reseal(out, changed)
                with self.assertRaises(ValueError):
                    runner.audit(out)

    def test_other_directory_source_or_output_cannot_use_frozen_identity(self):
        out = self.synthetic_build()
        original = runner.read(out / "result.json")
        source_index = original["commands"][0]["argv"].index("/synthetic/source/net_reference.cpp")
        mutations = (
            lambda argv: argv.__setitem__(source_index, "/another/source/net_reference.cpp"),
            lambda argv: argv.__setitem__(-1, "/another/net_reference"),
            lambda argv: argv.__setitem__(source_index, "/synthetic/source/../other/net_reference.cpp"),
        )
        for mutate in mutations:
            changed = copy.deepcopy(original)
            mutate(changed["commands"][0]["argv"])
            with self.subTest(argv=changed["commands"][0]["argv"]):
                self.reseal(out, changed)
                with self.assertRaises(ValueError):
                    runner.audit(out)

    def test_report_controlled_sizes_are_bound_to_archived_fixture_files(self):
        cases = self.base / "cases"
        cases.mkdir()
        (cases / "input.wire").write_bytes(b"x" * 100)
        (cases / "backpressure.wire").write_bytes(b"y" * 200)
        good = {"fixture_frame_bytes": 100, "invalid_graph_frame_bytes": 100, "backpressure_frame_bytes": 200}
        runner.validate_smoke_files(good, cases)
        for field in good:
            changed = dict(good)
            changed[field] += 1
            with self.subTest(field=field), self.assertRaises(ValueError):
                runner.validate_smoke_files(changed, cases)
        # The frame sizes are verified from the relocated archive, without
        # following paths named by report-controlled symbols.
        moved = self.base / "moved-cases"
        shutil.move(cases, moved)
        runner.validate_smoke_files(good, moved)

    def test_binary_stdout_requires_one_json_and_complete_failure_fields(self):
        good = {"schema_version": 1, "contract_version": "1.0.0", "status": "failed", "command": "smoke",
                "diagnostic": False, "timing_evidence": False, "reason": "eof_header", "error": "expected error detail"}
        runner.parse_binary_stdout(json.dumps(good), self.contract, "smoke", False, 2)
        for key in good:
            changed = dict(good)
            changed.pop(key)
            with self.subTest(missing=key), self.assertRaises(ValueError):
                runner.parse_binary_stdout(json.dumps(changed), self.contract, "smoke", False, 2)
        for raw in (json.dumps(good) + json.dumps(good), "diagnostic line\n" + json.dumps(good),
                    json.dumps(good)[:-1] + ',"status":"failed"}',
                    json.dumps(good)[:-1] + ',"invalid":NaN}'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                runner.parse_binary_stdout(raw, self.contract, "smoke", False, 2)

    def test_explicit_sanitizer_resource_is_frozen_auditable_and_checked_live(self):
        resource = self.base / "compiler-rt"
        (resource / "include").mkdir(parents=True)
        (resource / "include/stddef.h").write_text("synthetic builtin header")
        for name in runner.SANITIZER_RUNTIMES:
            target = resource / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"synthetic compiler-rt identity only")
        identity = runner.capture_sanitizer_resource(resource)
        runner.check_sanitizer_resource_live(identity)
        with self.assertRaisesRegex(ValueError, "requires address-undefined"):
            runner.validate_sanitizer_resource(identity, "none")
        out = self.synthetic_build("diagnostic", "address-undefined", identity)
        runner.audit(out)
        original = runner.read(out / "result.json")
        changed = copy.deepcopy(original)
        index = changed["commands"][0]["argv"].index("-resource-dir") + 1
        changed["commands"][0]["argv"][index] = "/other/compiler-rt"
        self.reseal(out, changed)
        with self.assertRaisesRegex(ValueError, "compiler invocation differs"):
            runner.audit(out)
        self.reseal(out, original)
        (resource / runner.SANITIZER_RUNTIMES[0]).write_text("runtime changed")
        with self.assertRaisesRegex(ValueError, "changed or unavailable"):
            runner.check_sanitizer_resource_live(identity)
        (resource / runner.SANITIZER_RUNTIMES[1]).unlink()
        with self.assertRaisesRegex(ValueError, "prerequisite runtime unavailable"):
            runner.capture_sanitizer_resource(resource)
        shutil.rmtree(resource)
        shutil.rmtree(self.root)
        runner.audit(out)  # Offline audit never follows the old resource path.

    def test_sanitizer_resource_option_rejected_before_queue_or_output(self):
        out = self.base / "must-not-exist"
        with self.assertRaisesRegex(ValueError, "requires --sanitizer address-undefined"):
            runner.main(["build", "--prepared", "unused", "--out", str(out), "--profile", "timing",
                         "--sanitizer-resource-dir", "/compiler-rt"])
        self.assertFalse(out.exists())

    def test_missing_coordination_receipt_rejected(self):
        out = self.synthetic_build()
        record = runner.read(out / "result.json")
        record["commands"][0]["coordination"]["ancestor_pids"] = [999]
        self.reseal(out, record)
        with self.assertRaisesRegex(ValueError, "queue ownership"):
            runner.audit(out)

    def test_smoke_exact_case_and_scope_validation(self):
        runner.validate_smoke(self.report(), self.contract, False)
        mutations = (
            lambda r: r["case_ids"].append("model.fixture"),
            lambda r: r["cases"].pop(),
            lambda r: r["cases"][0].update(scope="actual_tcp"),
            lambda r: r["cases"][0].update(reason=""),
            lambda r: r["cases"][0].update(read_bytes=True),
            lambda r: r["cases"][0].update(published=1),
            lambda r: r.update(diagnostic=True),
            lambda r: r.update(timing_evidence=True),
            lambda r: r["cases"][0].update(status="failed"),
            lambda r: r["cases"][1].update(id="model.fixture"),
        )
        for mutate in mutations:
            report = self.report()
            mutate(report)
            with self.subTest(report=report), self.assertRaises(ValueError):
                runner.validate_smoke(report, self.contract, False)

    def test_expected_negative_reasons_validated_when_frozen(self):
        self.contract["smoke"]["expected_reasons"] = {x: "ok" for x in self.contract["smoke"]["case_ids"]}
        report = self.report()
        runner.validate_smoke(report, self.contract, False)
        report["cases"][0]["reason"] = "arbitrary exception"
        with self.assertRaisesRegex(ValueError, "reason mismatch"):
            runner.validate_smoke(report, self.contract, False)

    def subcase_fixture(self):
        record = {"id": "model.fixture.header.7", "parent_id": "model.fixture", "scope": "injected_model",
                  "expected_reason": "eof_header", "actual_reason": "eof_header"}
        for field in runner.PROGRESS_FIELDS:
            record[field] = record["expected_" + field] = False if field == "published" else (
                1 if field == "terminal_count" else 7 if field == "read_bytes" else 0)
        expected = {key: value for key, value in record.items() if key not in runner.PROGRESS_FIELDS and key != "actual_reason"}
        expected["expected_read_bytes"] = "wire_bytes - 81"
        self.contract["smoke"]["progress_symbols"] = {"wire_bytes": "smoke.fixture_frame_bytes"}
        self.contract["smoke"]["subcases"] = [expected]
        report = self.report()
        report.update(fixture_frame_bytes=88, subcases=[record], owned_sockets_after_cases=0)
        for group in report["cases"]:
            if group["id"] != "model.fixture":
                extra = copy.deepcopy(record)
                extra.update(id=group["id"] + ".subcase", parent_id=group["id"], scope=group["scope"])
                report["subcases"].append(extra)
                self.contract["smoke"]["subcases"].append({key: value for key, value in extra.items()
                    if key not in runner.PROGRESS_FIELDS and key != "actual_reason"})
        self.contract["smoke"]["subcase_count"] = len(report["subcases"])
        report["cases"] = [{"id": row["id"], "scope": row["scope"], "status": "passed", "subcase_count": 1}
                           for row in report["cases"]]
        return report

    def test_subcase_exact_frozen_reasons_and_progress(self):
        report = self.subcase_fixture()
        runner.validate_smoke(report, self.contract, False)
        mutations = (
            lambda r: r["subcases"].clear(),
            lambda r: r["subcases"].append(copy.deepcopy(r["subcases"][0])),
            lambda r: r["subcases"][0].update(parent_id="tcp.fixture"),
            lambda r: r["subcases"][0].update(scope="actual_tcp"),
            lambda r: r["subcases"][0].update(actual_reason="unrelated exception"),
            lambda r: r["subcases"][0].update(expected_reason="unrelated exception", actual_reason="unrelated exception"),
            lambda r: r["subcases"][0].update(read_bytes=8, expected_read_bytes=8),
            lambda r: r["subcases"][0].update(active_bytes=7),
            lambda r: r["subcases"][0].update(terminal_count=True),
            lambda r: r["subcases"][0].update(published=0),
            lambda r: r.update(fixture_frame_bytes=True),
            lambda r: r.pop("fixture_frame_bytes"),
            lambda r: r.update(owned_sockets_after_cases=1),
            lambda r: r["cases"][0].update(subcase_count=99),
            lambda r: r["cases"][0].update(reason="manufactured total"),
        )
        for mutate in mutations:
            changed = copy.deepcopy(report)
            mutate(changed)
            with self.subTest(report=changed), self.assertRaises(ValueError):
                runner.validate_smoke(changed, self.contract, False)

    def test_counter_expression_parser_rejects_code(self):
        self.assertEqual(runner.expected_counter("2*wire_bytes + 10 // 2 - 1", {"wire_bytes": 7}), 18)
        for value in ("__import__('os').getpid()", "wire_bytes.attr", "unknown", "2**8", "1//0"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                runner.expected_counter(value, {"wire_bytes": 7})

    def test_contract_rejects_duplicate_or_incomplete_subcase(self):
        self.subcase_fixture()
        frozen = self.contract["smoke"]["subcases"]
        frozen.append(copy.deepcopy(frozen[0]))
        runner.write(self.network / "contract.json", self.contract)
        with self.assertRaisesRegex(ValueError, "duplicate contract subcase"):
            self.prepare("duplicate-subcase")
        frozen.pop()
        frozen[0].pop("expected_terminal_count")
        runner.write(self.network / "contract.json", self.contract)
        with self.assertRaisesRegex(ValueError, "expected counter missing"):
            self.prepare("incomplete-subcase")

    def test_portable_standalone_smoke_archive(self):
        build = self.synthetic_build()
        out = runner.fresh(self.base / "smoke")
        shutil.copytree(build, out / "build")
        report = self.report()
        runner.write(out / "smoke.json", report)
        runner.write(out / "smoke.stdout", report)
        (out / "smoke.stderr").write_text("")
        parent = runner.read(build / "result.json")
        record = self.record()
        record.update(build_result_sha256=runner.sha(build / "result.json"), binary_sha256=parent["binary_sha256"],
                      executed_build_root="/synthetic", smoke_root="/synthetic-smoke",
                      executed_binary_sha256_before=parent["binary_sha256"], executed_binary_sha256_after=parent["binary_sha256"],
                      commands=[self.receipt(["/synthetic/net_reference", "smoke", "--out-dir", "/synthetic-smoke/cases"])])
        runner.finish(out, record, "smoke")
        shutil.rmtree(build)
        shutil.rmtree(self.root)
        shutil.rmtree(self.base / "prepared")
        moved = self.base / "relocated-smoke"
        shutil.move(out, moved)
        before = runner.files(moved)
        self.assertEqual(runner.audit(moved)["kind"], "smoke")
        self.assertEqual(before, runner.files(moved))
        original = runner.read(moved / "result.json")
        for change in (lambda r: r["commands"][0]["argv"].__setitem__(0, "/other/net_reference"),
                       lambda r: r["commands"][0]["argv"].__setitem__(3, "/other/cases"),
                       lambda r: r.update(executed_binary_sha256_before="b" * 64),
                       lambda r: r.update(executed_binary_sha256_after="b" * 64)):
            changed = copy.deepcopy(original)
            change(changed)
            self.reseal(moved, changed)
            with self.subTest(record=changed), self.assertRaises(ValueError):
                runner.audit(moved)
        self.reseal(moved, original)
        # Even a resealed outer archive cannot hide changed raw stdout.
        report["checks"] = 99
        runner.write(moved / "smoke.stdout", report)
        self.reseal(moved)
        with self.assertRaisesRegex(ValueError, "differs from stdout"):
            runner.audit(moved)

    def test_queue_flag_and_idle_queue_cannot_authorize_execution(self):
        args = argparse.Namespace(coordinated_execution=False, queue_lock=self.base / "queue.lock")
        with self.assertRaisesRegex(ValueError, "require run_job"):
            runner.coordinated(args)
        args.coordinated_execution = True
        args.queue_lock.write_text(json.dumps({"status": "running", "owner": "W1", "supervisor_pid": os.getpid()}))
        with self.assertRaisesRegex(ValueError, "queue is idle"):
            runner.coordinated(args)

    def test_authorization_rejected_before_output_creation(self):
        out = self.base / "must-not-exist"
        with self.assertRaisesRegex(ValueError, "require run_job"):
            runner.main(["build", "--prepared", "unused", "--out", str(out), "--profile", "timing"])
        self.assertFalse(out.exists())

    @unittest.skipUnless(os.name == "posix", "queue uses POSIX flock")
    def test_real_lock_and_ancestor_proof_accept_owned_child_reject_other_tree(self):
        import fcntl
        path = self.base / "held-queue.lock"
        metadata = {"status": "running", "owner": "W1", "supervisor_pid": os.getpid()}
        runner.write(path, metadata)
        fd = os.open(path, os.O_RDWR)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            code = ("import argparse,importlib.util,json,pathlib,sys; "
                    "s=importlib.util.spec_from_file_location('runner',sys.argv[1]); "
                    "m=importlib.util.module_from_spec(s); s.loader.exec_module(m); "
                    "print(json.dumps(m.coordinated(argparse.Namespace(coordinated_execution=True,"
                    "queue_lock=pathlib.Path(sys.argv[2])))))")
            argv = [sys.executable, "-B", "-c", code, runner.__file__, str(path)]
            process = subprocess.run(argv, capture_output=True, text=True, timeout=10)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(process.stdout)["lock_metadata"], metadata)
            metadata["supervisor_pid"] = 2147483647
            runner.write(path, metadata)
            rejected = subprocess.run(argv, capture_output=True, text=True, timeout=10)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("another process tree", rejected.stderr)
        finally:
            os.close(fd)

    def test_timeout_positive_finite(self):
        for value in ("0", "-1", "nan", "inf"):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                runner.positive(value)
        self.assertEqual(runner.positive("1.5"), 1.5)


if __name__ == "__main__":
    unittest.main()
