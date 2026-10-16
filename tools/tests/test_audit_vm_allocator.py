"""Offline front end must reject unsafe archives and incomplete evidence."""
import hashlib
import gzip
import importlib.util
import io
import json
import marshal
from pathlib import Path
import struct
import tarfile
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location("audit_vm_allocator", Path(__file__).parents[1] / "audit_vm_allocator.py")
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, sort_keys=True) + "\n")


RUNNER = '''import hashlib,json
def check(path,prepared):
    state=json.loads((path/'result.json').read_text())
    if state['status']!='complete':raise RuntimeError('not complete')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    if (path/'COMPLETE').read_text().strip()!=sha(path/'result.json'):raise RuntimeError('bad completion')
    if state['prepared_sha256']!=sha(prepared/'prepared.json'):raise RuntimeError('wrong prepared override')
    return state
def audit(path,prepared_override):
    check(path,prepared_override)
    return [{'case':'synthetic'}]
def audit_validation(path,prepared_override):
    check(path,prepared_override)
    return {'status':'audited','commands':2}
'''


class OfflineAuditTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.bundle = self.root / "bundle"
        self.bundle.mkdir()
        self.tree = self.root / "tree"
        prepared = self.tree / "experiments/prepared"
        prepared.mkdir(parents=True)
        (prepared / "input.txt").write_text("frozen source\n")
        write(prepared / "prepared.json", {"status": "prepared", "files": {"input.txt": digest(prepared / "input.txt")}})
        identity = digest(prepared / "prepared.json")
        for directory in ("windows/one", "validations/one", "diagnostics"):
            path = self.tree / "experiments" / directory
            write(path / "result.json", {"status": "complete", "prepared_sha256": identity})
            (path / "COMPLETE").write_text(digest(path / "result.json") + "\n")
        self.failed = self.tree / "experiments/failed/one"
        write(self.failed / "result.json", {"status": "failed", "error": "expected counterexample",
              "failed_command": {"stdout": "counterexample\n", "stderr": ""}})
        (self.failed / "failed-command.log").write_text("counterexample\n")
        self.index = {"archive": "experiments.tar.gz", "prepared": {"main": "experiments/prepared"},
                      "timing": [{"path": "experiments/windows/one", "prepared": "main"}],
                      "correctness": [{"path": "experiments/validations/one", "prepared": "main"}],
                      "diagnostics": [{"path": "experiments/diagnostics", "prepared": "main"}],
                      "failed_runs": [{"path": "experiments/failed/one", "status": "failed", "reason": "known counterexample"}]}
        (self.bundle / "runner.py").write_text(RUNNER)
        (self.bundle / "collect_vm_allocator_diagnostics.py").write_text(
            "import run_vm_allocator as R\n"
            "def audit(path,prepared_override):\n"
            "    if not (path/'COMPLETE').is_file() or not (prepared_override/'prepared.json').is_file():raise RuntimeError('missing diagnostic input')\n"
            "    R.check(path,prepared_override)\n"
            "    return {'status':'audited','samples':1,'formal_timing_forbidden':True}\n"
            "def compare_timing(path,timing_records,prepared_override=None):\n"
            "    audit(path,prepared_override)\n"
            "    if len(timing_records)!=1:raise RuntimeError('wrong timing inventory')\n"
            "    for window,prepared in timing_records:R.check(window,prepared)\n"
            "    return {'status':'audited','diagnostic_samples':1,'matched_timing_samples':1,'timing_values_compared':False}\n")
        self.package()

    def tearDown(self):
        self.temporary.cleanup()

    def seal(self, directory=None):
        directory = directory or self.bundle
        files = {path.relative_to(directory).as_posix(): digest(path)
                 for path in directory.rglob("*") if path.is_file() and path.name != "manifest.json"}
        write(directory / "manifest.json", files)

    def package(self):
        with tarfile.open(self.bundle / "experiments.tar.gz", "w:gz") as archive:
            archive.add(self.tree / "experiments", arcname="experiments")
        write(self.bundle / "index.json", self.index)
        self.seal()

    def test_complete_bundle_invokes_every_frozen_auditor(self):
        result = AUDIT.audit(self.bundle)
        self.assertEqual(result["status"], "audited")
        self.assertEqual(result["timing_windows"], 1)
        self.assertEqual(result["correctness_commands"], 2)
        self.assertEqual(result["diagnostic_runs"], 1)
        self.assertEqual(result["crosschecks"][0]["matched_timing_samples"], 1)
        self.assertIs(result["crosschecks"][0]["timing_values_compared"], False)
        self.assertEqual(result["failed_runs"], 1)
        self.assertFalse((self.bundle / "__pycache__").exists())

    def test_ignored_bytecode_cannot_replace_frozen_source(self):
        source = self.bundle / "runner.py"
        cache = Path(importlib.util.cache_from_source(str(source)))
        cache.parent.mkdir()
        metadata = source.stat()
        malicious = compile("raise RuntimeError('unhashed bytecode ran')", str(source), "exec")
        cache.write_bytes(importlib.util.MAGIC_NUMBER + struct.pack("<III", 0, int(metadata.st_mtime), metadata.st_size)
                          + marshal.dumps(malicious))
        self.assertEqual(AUDIT.audit(self.bundle)["status"], "audited")

    def test_diagnostic_crosschecks_are_mandatory_and_not_time_comparisons(self):
        path = self.bundle / "collect_vm_allocator_diagnostics.py"
        original = path.read_text()
        path.write_text(original.split("def compare_timing", 1)[0])
        self.seal()
        with self.assertRaisesRegex(AUDIT.AuditError, "lacks mandatory timing crosschecks"):
            AUDIT.audit(self.bundle)
        path.write_text(original.replace("'timing_values_compared':False", "'timing_values_compared':True"))
        self.seal()
        with self.assertRaisesRegex(AUDIT.AuditError, "crosscheck did not confirm"):
            AUDIT.audit(self.bundle)

    def test_modified_missing_extra_and_unsafe_manifest_entries_rejected(self):
        original = (self.bundle / "runner.py").read_text()
        (self.bundle / "runner.py").write_text("changed")
        with self.assertRaisesRegex(AUDIT.AuditError, "missing/changed"):
            AUDIT.audit(self.bundle)
        (self.bundle / "runner.py").write_text(original)
        (self.bundle / "unlisted.txt").write_text("extra")
        with self.assertRaisesRegex(AUDIT.AuditError, "inventory differs"):
            AUDIT.audit(self.bundle)
        (self.bundle / "unlisted.txt").unlink()
        manifest = json.loads((self.bundle / "manifest.json").read_text())
        manifest["../outside"] = "0" * 64
        write(self.bundle / "manifest.json", manifest)
        with self.assertRaisesRegex(AUDIT.AuditError, "unsafe relative"):
            AUDIT.audit(self.bundle)

    def test_path_escape_link_and_duplicate_tar_members_rejected(self):
        for kind in ("escape", "symlink", "hardlink", "duplicate"):
            with self.subTest(kind=kind):
                with tarfile.open(self.bundle / "experiments.tar.gz", "w:gz") as archive:
                    info = tarfile.TarInfo("../escaped" if kind == "escape" else "entry")
                    if kind in ("symlink", "hardlink"):
                        info.type = tarfile.SYMTYPE if kind == "symlink" else tarfile.LNKTYPE
                        info.linkname = "../escaped"
                        archive.addfile(info)
                    else:
                        info.size = 1
                        archive.addfile(info, io.BytesIO(b"x"))
                        if kind == "duplicate":
                            archive.addfile(info, io.BytesIO(b"x"))
                self.seal()
                with self.assertRaises(AUDIT.AuditError):
                    AUDIT.audit(self.bundle)
                self.assertFalse((self.root / "escaped").exists())

    def test_changed_prepared_and_unindexed_runs_rejected(self):
        source = self.tree / "experiments/prepared/input.txt"
        source.write_text("mutated")
        self.package()
        with self.assertRaisesRegex(AUDIT.AuditError, "prepared"):
            AUDIT.audit(self.bundle)
        source.write_text("frozen source\n")
        write(self.tree / "experiments/windows/unlisted/result.json", {"status": "complete"})
        self.package()
        with self.assertRaisesRegex(AUDIT.AuditError, "unindexed result"):
            AUDIT.audit(self.bundle)

    def test_failed_state_logs_and_completion_are_checked(self):
        original = (self.failed / "failed-command.log").read_text()
        (self.failed / "failed-command.log").write_text("truncated")
        self.package()
        with self.assertRaisesRegex(AUDIT.AuditError, "failed-command log"):
            AUDIT.audit(self.bundle)
        (self.failed / "failed-command.log").write_text(original)
        (self.failed / "COMPLETE").write_text("misleading")
        self.package()
        with self.assertRaisesRegex(AUDIT.AuditError, "failed run incorrectly"):
            AUDIT.audit(self.bundle)

    def test_index_must_resolve_and_not_duplicate_records(self):
        self.index["timing"][0]["prepared"] = "absent"
        self.package()
        with self.assertRaisesRegex(AUDIT.AuditError, "unresolved prepared"):
            AUDIT.audit(self.bundle)
        self.index["timing"][0]["prepared"] = "main"
        self.index["timing"].append(dict(self.index["timing"][0]))
        self.package()
        with self.assertRaisesRegex(AUDIT.AuditError, "duplicate indexed"):
            AUDIT.audit(self.bundle)

    def test_validation_and_relocated_worktree_manifest(self):
        worktree = self.root / "worktree"
        worktree.mkdir()
        headers = {}
        for name in AUDIT.REQUIRED_HEADERS:
            (worktree / name).write_text(name)
            headers[name] = digest(worktree / name)
        self.index["baseline_headers"] = headers
        self.package()
        validation = self.root / "validation"
        write(validation / "final-worktree-manifest.json", {
            "worktree_root": "/original/project",
            "files": {"/original/project/" + name: value for name, value in headers.items()}})
        self.seal(validation)
        result = AUDIT.audit(self.bundle, validation, True, worktree)
        self.assertEqual(result["worktree_files"], 2)
        (worktree / "xoffsetdatastructure.hpp").write_text("changed")
        with self.assertRaisesRegex(AUDIT.AuditError, "baseline header"):
            AUDIT.audit(self.bundle, validation, True, worktree)

    def test_archive_byte_budget_and_duplicate_json_keys_rejected(self):
        with self.assertRaisesRegex(AUDIT.AuditError, "byte budget"):
            AUDIT.audit(self.bundle, maximum_extract_bytes=1)
        (self.bundle / "index.json").write_text('{"archive":"a","archive":"b"}')
        self.seal()
        with self.assertRaisesRegex(AUDIT.AuditError, "duplicate JSON"):
            AUDIT.audit(self.bundle)

    def add_random_inputs(self):
        seeds = [7, 42]
        operations = 3
        chunks = []
        for seed in seeds:
            state = seed
            words = []
            for _ in range(operations):
                state ^= (state << 13) & 0xffffffffffffffff
                state ^= state >> 7
                state ^= (state << 17) & 0xffffffffffffffff
                words.append(state)
            chunks.append(struct.pack("<3Q", *words))
        packed = b"".join(chunks)
        (self.bundle / "random-inputs.u64le.gz").write_bytes(gzip.compress(packed))
        write(self.bundle / "random-inputs.json", {
            "algorithm": "xorshift64(13,7,17)", "byte_order": "little", "seeds": seeds,
            "operations_per_seed": operations, "word_bytes": 8, "uncompressed_bytes": len(packed),
            "sha256_per_seed": [hashlib.sha256(chunk).hexdigest() for chunk in chunks]})
        self.index["random_inputs"] = {"file": "random-inputs.u64le.gz", "manifest": "random-inputs.json"}
        self.package()
        return packed

    def test_streamed_random_input_words_and_seed_digests(self):
        self.add_random_inputs()
        result = AUDIT.audit(self.bundle)
        self.assertEqual(result["random_inputs"], {"status": "audited", "seeds": 2,
                         "operations_per_seed": 3, "words": 6})

    def test_random_input_mutation_truncation_extra_and_seed_order_rejected(self):
        packed = self.add_random_inputs()
        data = self.bundle / "random-inputs.u64le.gz"
        cases = (bytes([packed[0] ^ 1]) + packed[1:], packed[:-1], packed + b"x", packed[24:] + packed[:24])
        for bad in cases:
            with self.subTest(length=len(bad)):
                data.write_bytes(gzip.compress(bad))
                self.seal()
                with self.assertRaises(AUDIT.AuditError):
                    AUDIT.audit(self.bundle)

    def test_random_input_digest_encoding_and_gzip_integrity_rejected(self):
        self.add_random_inputs()
        metadata_path = self.bundle / "random-inputs.json"
        original = json.loads(metadata_path.read_text())
        for field, value in (("sha256_per_seed", ["0" * 64, "0" * 64]),
                             ("byte_order", "big"), ("uncompressed_bytes", 49),
                             ("seeds", [True, 42])):
            with self.subTest(field=field):
                write(metadata_path, dict(original, **{field: value}))
                self.seal()
                with self.assertRaises(AUDIT.AuditError):
                    AUDIT.audit(self.bundle)
        write(metadata_path, original)
        data = self.bundle / "random-inputs.u64le.gz"
        data.write_bytes(data.read_bytes()[:-2])
        self.seal()
        with self.assertRaisesRegex(AUDIT.AuditError, "gzip"):
            AUDIT.audit(self.bundle)


if __name__ == "__main__":
    unittest.main()
