#!/usr/bin/env python3
"""Lightweight source-integrity checks. No C++ or benchmark execution."""
from __future__ import annotations

import copy
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
import paired_measurement as P
import prepare_default as D

ROOT = Path(__file__).resolve().parents[2]


class DefaultPreparationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="default-preparation-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.original = ROOT / "docs/perf/2026-10-15-vm-stage1"
        self.out = self.root / "prepared"
        D.prepare(self.original, self.out)

    def mutate(self, key, value):
        record = P.read(self.out / "preparation.json")
        record[key] = value
        (self.out / "preparation.json").write_bytes(P.canonical(record))

    def test_actual_eight_sources_prepare_and_audit_without_execution(self):
        result = D.audit(self.out)
        self.assertEqual(result["source_files"], 8)
        self.assertIs(result["executed_cpp"], False)
        self.assertIs(result["performance_evidence"], False)

    def test_status_schema_and_execution_claim_tampering_rejected(self):
        original = (self.out / "preparation.json").read_bytes()
        for key, value in (("status", "complete"), ("schema_version", True),
                           ("historical_evidence_reexecuted", True), ("executed_cpp", True),
                           ("performance_evidence", True)):
            with self.subTest(key=key):
                self.mutate(key, value)
                with self.assertRaises(P.ProtocolError):
                    D.audit(self.out)
                (self.out / "preparation.json").write_bytes(original)

    def test_inherited_commands_and_case_type_tampering_rejected(self):
        original = (self.out / "preparation.json").read_bytes()
        record = P.parse(original)
        commands = copy.deepcopy(record["inherited_build_commands"])
        commands["D"][0] = "wrong-compiler"
        self.mutate("inherited_build_commands", commands)
        with self.assertRaises(P.ProtocolError):
            D.audit(self.out)
        (self.out / "preparation.json").write_bytes(original)
        cases = copy.deepcopy(record["historical_cases"])
        cases[0]["cycles"] = True
        self.mutate("historical_cases", cases)
        with self.assertRaises(P.ProtocolError):
            D.audit(self.out)

    def test_noncanonical_metadata_bytes_preserved_and_portable(self):
        alternate = self.root / "historical"
        alternate.mkdir()
        import json
        data = json.dumps(P.read(self.original / "metadata.json"), separators=(",", ":")).encode()
        (alternate / "metadata.json").write_bytes(data)
        shutil.copy2(self.original / "source_snapshots.tar.gz", alternate / "source_snapshots.tar.gz")
        target = self.root / "alternate-prepared"
        D.prepare(alternate, target)
        self.assertEqual((target / "historical-metadata.json").read_bytes(), data)
        moved = self.root / "moved"
        shutil.copytree(target, moved)
        shutil.rmtree(alternate)
        shutil.rmtree(target)
        before = P.inventory(moved)
        D.audit(moved)
        self.assertEqual(P.inventory(moved), before)

    def test_existing_output_never_replaced_and_source_tamper_rejected(self):
        before = P.inventory(self.out)
        with self.assertRaises(FileExistsError):
            D.prepare(self.original, self.out)
        self.assertEqual(P.inventory(self.out), before)
        source = self.out / "source/F/xoffsetdatastructure.hpp"
        source.write_bytes(source.read_bytes() + b"\n")
        with self.assertRaises(P.ProtocolError):
            D.audit(self.out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
