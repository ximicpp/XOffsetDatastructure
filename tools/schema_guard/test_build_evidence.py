import json
from pathlib import Path
import tempfile
import unittest
import build_evidence as e

class BuildEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.sdk=self.root/"sdk"; self.sdk.mkdir()
        (self.sdk/"header").write_text("SDK 1")
        self.binary=self.root/"binary"; self.binary.write_text("fixture")
        self.receipt=self.root/"manifest.json"
        self.record=e.prepare("macos-arm64","source",[["compiler","-O1"]],[self.sdk])
        e.finish(self.record,self.binary,self.receipt)

    def validate(self,expected=None):
        return e.validate(self.receipt,self.binary,"macos-arm64","source",expected or self.record["build_id"])

    def test_valid_manifest(self):
        self.assertEqual(self.validate()["build_id"],self.record["build_id"])

    def test_changed_sdk_and_binary_are_rejected(self):
        (self.sdk/"header").write_text("SDK 2")
        with self.assertRaisesRegex(ValueError,"build input changed"): self.validate()
        self.binary.write_text("different fixture")
        with self.assertRaisesRegex(ValueError,"binary digest"): self.validate()

    def test_changed_flags_change_build_id(self):
        changed=e.prepare("macos-arm64","source",[["compiler","-fpack-struct"]],[self.sdk])
        with self.assertRaisesRegex(ValueError,"build id"): self.validate(changed["build_id"])

    def test_manifest_tampering_is_rejected(self):
        r=json.loads(self.receipt.read_text()); r["spec"]["commands"]=[["other-compiler"]]
        self.receipt.write_text(json.dumps(r))
        with self.assertRaisesRegex(ValueError,"build id"): self.validate()


    def test_link_check_binds_remote_inputs_to_binary(self):
        self.assertIn(self.record["build_id"], e.check_link_inputs(self.receipt,self.binary))
        (self.sdk/"header").write_text("changed runtime")
        with self.assertRaisesRegex(ValueError,"build input changed"):
            e.check_link_inputs(self.receipt,self.binary)
