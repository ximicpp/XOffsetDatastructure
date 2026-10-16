"""D031 byte-preservation regressions and small offline negative controls.

The collect integration executes one synthetic slot through the real collector.
Native launch is replaced with a file-writing mock, and Popen is forbidden.
All temporary artifacts live under the D023 test scratch directory.
"""
from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SCRATCH = REPO / 'build/vm-owner-measurement/test-scratch'
REAL_APPROVAL = REPO / 'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json'
REAL_APPROVAL_SHA = '0157b0d310d7388729f51a4031c6ca552ca32f9d15982972d7e0c5aebe1d9e12'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


collector = load('d031_collector_byte_tests', HERE / 'collect_owner_measurement.py')


def digest(data):
    return hashlib.sha256(data).hexdigest()


class ScratchTest(unittest.TestCase):
    def setUp(self):
        SCRATCH.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(prefix='approval-bytes-', dir=SCRATCH)
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        guard = mock.patch.object(collector.subprocess, 'Popen', side_effect=AssertionError('native execution forbidden in D031 tests'))
        self.popen = guard.start()
        self.addCleanup(guard.stop)

    def tearDown(self):
        self.popen.assert_not_called()


class PreserveApprovalTests(ScratchTest):
    def test_real_d030_noncanonical_bytes_are_preserved(self):
        raw = REAL_APPROVAL.read_bytes()
        self.assertEqual(digest(raw), REAL_APPROVAL_SHA)
        self.assertNotEqual(raw, collector.dump(json.loads(raw)).encode('utf-8'))
        source, target = self.directory / 'source.json', self.directory / 'copy.json'
        source.write_bytes(raw)
        collector.preserve_approval(source, target, REAL_APPROVAL_SHA)
        self.assertEqual(target.read_bytes(), raw)
        self.assertEqual(collector.sha(target), REAL_APPROVAL_SHA)
        self.assertEqual(json.loads(target.read_bytes()), json.loads(raw))

    def test_utf8_unsorted_crlf_and_indentation_are_preserved_from_the_verified_buffer(self):
        raw = '{\r\n    "z": "批准原字节",\r\n    "a": [true, 1]\r\n}\r\n'.encode('utf-8')
        source, target = self.directory / 'source.json', self.directory / 'copy.json'
        source.write_bytes(b'invalid after verification')
        collector.preserve_approval(source, target, digest(raw), data=raw)
        self.assertEqual(target.read_bytes(), raw)
        self.assertEqual(json.loads(target.read_bytes()), {'z': '批准原字节', 'a': [True, 1]})
        self.assertNotEqual(target.read_bytes(), collector.dump(json.loads(raw)).encode('utf-8'))

    def test_mismatched_buffer_sha_and_existing_target_are_rejected(self):
        source, target = self.directory / 'source.json', self.directory / 'copy.json'
        source.write_bytes(b'{"a": 1}\n')
        with self.assertRaises(ValueError):
            collector.preserve_approval(source, target, '0' * 64)
        self.assertFalse(target.exists())
        collector.preserve_approval(source, target, collector.sha(source))
        with self.assertRaises(FileExistsError):
            collector.preserve_approval(source, target, collector.sha(source))


class RealCollectPathTests(ScratchTest):
    def run_collect(self, *, actual_approval):
        repo = self.directory / 'repo'
        root = repo / 'build/vm-owner-measurement/proposal'
        (root / 'producer').mkdir(parents=True)
        (root / 'tools').mkdir()
        approval_path = repo / 'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json'
        approval_path.parent.mkdir(parents=True)
        binary = root / 'producer/synthetic-never-executed'
        binary.write_bytes(b'not a native executable\n')
        collector.write(root / 'producer/binding.json', {})
        collector.write(root / 'producer/build_result.json', {'runtime': {}})
        if actual_approval:
            # Copy only the small immutable proposal JSON. Its authority and
            # runtime verification are mocked below; the actual approval reader,
            # digest check, semantic approval gates, and byte save remain real.
            proposal_bytes = (REPO / 'build/vm-owner-measurement/proposal-v1/proposal.json').read_bytes()
            (root / 'proposal.json').write_bytes(proposal_bytes)
            proposal = json.loads(proposal_bytes)
            approval_bytes = REAL_APPROVAL.read_bytes()
            self.assertEqual(digest(approval_bytes), REAL_APPROVAL_SHA)
        else:
            collector.write(root / 'proposal.json', {'synthetic': 'D031 real collect path'})
            proposal = {'run_id': 'synthetic-D031-run', 'producer': {
                'binding_sha256': '1' * 64, 'parent_acceptance_sha256': '2' * 64}}
        proposal['python'] = {'synthetic': 'no interpreter identity claim'}
        proposal['producer'].update(binary=str(binary), binary_sha256=collector.sha(binary), external_files={})
        proposal['execution'] = {
            'repository': str(repo), 'collector': str(Path(collector.__file__).resolve()),
            'proposal': str(root / 'proposal.json'), 'approval': str(approval_path),
            'window': str(repo / 'build/vm-owner-measurement/window-v1'),
            'queue_receipt': str(repo / 'build/vm-owner-measurement/receipts/window-v1'),
        }
        if not actual_approval:
            approval = dict(schema='owner-measurement-approval-v1', decision='D031-合成批准',
                status='approved', approved_by='parent', native_execution_authorized=True,
                producer_functional_parent_accepted=True, proposal_sha256=collector.sha(root / 'proposal.json'),
                design_sha256=collector.DESIGN_SHA,
                producer_binding_sha256=proposal['producer']['binding_sha256'],
                producer_parent_acceptance_sha256=proposal['producer']['parent_acceptance_sha256'],
                run_id=proposal['run_id'], formal_execution_role='W3', max_windows=1)
            # Insertion order differs from old sorted serialization, with real
            # UTF-8 and CRLF. All fields still pass the real approval validator.
            approval_bytes = (json.dumps(approval, ensure_ascii=False, indent=4) + '\n').replace('\n', '\r\n').encode('utf-8')
        approval_path.write_bytes(approval_bytes)
        approval_sha = digest(approval_bytes)
        self.assertNotEqual(approval_bytes, collector.dump(json.loads(approval_bytes)).encode('utf-8'))
        slot = dict(index=0, owners=8, threads=1, requests=8192, control='none')
        records = []
        stats = SimpleNamespace(schedule=lambda: [copy.deepcopy(slot)],
            audit_native=lambda report, requested_slot, checker: {'synthetic': True},
            summarize=lambda values: {'synthetic': True, 'records': len(values)})
        queue = {'metadata': {'run_id': 'synthetic-queue-D031'}, 'request': {'synthetic': True}}
        invalid_replacement = b'the approval source changed after successful verification\n'

        def bound_after_verification(repository, binding):
            # This occurs after verify_approval and before preserve_approval.
            # A collector that reopens the approval during preservation fails.
            approval_path.write_bytes(invalid_replacement)
            return {'binary': str(binary), 'binary_sha256': collector.sha(binary)}

        def modules(path, name):
            if path == root / 'tools/audit_owner_measurement.py':
                return stats
            if path == root / 'producer/checker.py':
                return SimpleNamespace()
            raise AssertionError('unexpected module import: ' + str(path))

        def fake_child(argv, directory):
            self.assertEqual(approval_path.read_bytes(), invalid_replacement)
            self.assertEqual((Path(proposal['execution']['window']) / 'approval.json').read_bytes(), approval_bytes)
            report = {'synthetic': True, 'timing_evidence': False}
            collector.write(directory / 'stdout.json', report, exclusive=True)
            (directory / 'stderr.log').write_bytes(b'')
            records.append((argv, directory))
            return {'returncode': 0, 'timed_out': False}

        with mock.patch.object(collector, 'audit_proposal', return_value=proposal) as proposal_audit, \
                mock.patch.object(collector, 'verify_binding', side_effect=bound_after_verification), \
                mock.patch.object(collector, 'python_identity', return_value=proposal['python']), \
                mock.patch.object(collector, 'active_queue', return_value=queue), \
                mock.patch.object(collector, 'module', side_effect=modules), \
                mock.patch.object(collector, 'execute_child', side_effect=fake_child) as child, \
                mock.patch.dict(collector.os.environ, {}, clear=True):
            state = collector.collect(root, approval_path, approval_sha)
        proposal_audit.assert_called_once_with(root, True)
        child.assert_called_once()
        self.assertEqual(len(records), 1)
        window = Path(proposal['execution']['window'])
        self.assertEqual((window / 'approval.json').read_bytes(), approval_bytes)
        self.assertEqual(collector.sha(window / 'approval.json'), approval_sha)
        self.assertEqual(json.loads((window / 'approval.json').read_bytes()), json.loads(approval_bytes))
        self.assertEqual(state['approval_sha256'], approval_sha)
        self.assertEqual(state['status'], 'native_complete_pending_receipt')
        self.assertEqual(len(state['attempts']), 1)
        self.assertEqual(collector.read(window / 'summary.json'), {'synthetic': True, 'records': 1})

    def test_actual_d030_bytes_through_collect_survive_source_change_after_verification(self):
        self.run_collect(actual_approval=True)

    def test_unsorted_utf8_bytes_through_collect_survive_source_change_after_verification(self):
        self.run_collect(actual_approval=False)


class SupplementProofTests(ScratchTest):
    def setUp(self):
        super().setUp()
        self.supplement = load('d031_supplement_byte_tests', HERE / 'supplement_approval_bytes.py')
        self.raw = b'{\n  "z": 2,\n  "a": "original approval"\n}\n'
        self.canonical = collector.dump(json.loads(self.raw)).encode('utf-8')

    def test_real_d030_canonical_copy_has_exact_legacy_origin(self):
        raw = REAL_APPROVAL.read_bytes()
        copied = collector.dump(json.loads(raw)).encode('utf-8')
        proof = self.supplement.prove_approval(raw, copied, collector.dump)
        self.assertEqual(proof['original_sha256'], REAL_APPROVAL_SHA)
        self.assertEqual(proof['stored_sha256'], digest(copied))
        self.assertTrue(proof['objects_equal'])
        self.assertTrue(proof['stored_equals_legacy_dump'])

    def test_changed_approval_object_or_nonlegacy_serialization_is_rejected(self):
        changed = json.loads(self.raw)
        changed['z'] = 3
        controls = [
            (self.raw, collector.dump(changed).encode('utf-8')),
            (json.dumps(changed).encode('utf-8'), self.canonical),
            (self.raw, json.dumps(json.loads(self.raw)).encode('utf-8')),
            (self.canonical, self.canonical),
            (b'{"z":1,"z":2}', self.canonical),
            (b'{"z":NaN}', self.canonical),
        ]
        for original, stored in controls:
            with self.subTest(original=original, stored=stored), self.assertRaises(ValueError):
                self.supplement.prove_approval(original, stored, collector.dump)

    def windows(self, suffix):
        original = self.directory / ('original-' + suffix)
        derived = self.directory / ('derived-' + suffix)
        for root in (original, derived):
            (root / 'samples/00').mkdir(parents=True)
            (root / 'window.json').write_bytes(b'{"same": "state"}\n')
            (root / 'summary.json').write_bytes(b'{"same": "summary"}\n')
            (root / 'samples/00/stdout.json').write_bytes(b'{"synthetic": true, "timing_evidence": false}\n')
        (original / 'approval.json').write_bytes(self.canonical)
        (derived / 'approval.json').write_bytes(self.raw)
        return original, derived

    def test_only_the_declared_approval_bytes_may_differ(self):
        original, derived = self.windows('exact')
        changes = self.supplement.compare_windows(original, derived)
        self.assertEqual(changes, [dict(path='approval.json',
            original_sha256=digest(self.canonical), derived_sha256=digest(self.raw),
            original_bytes=len(self.canonical), derived_bytes=len(self.raw))])

    def test_changed_original_or_derived_sample_and_summary_are_rejected(self):
        for endpoint in ('original', 'derived'):
            for filename in ('summary.json', 'window.json', 'samples/00/stdout.json'):
                with self.subTest(endpoint=endpoint, filename=filename):
                    original, derived = self.windows(endpoint + '-' + filename.replace('/', '-'))
                    root = original if endpoint == 'original' else derived
                    (root / filename).write_bytes(b'tampered evidence\n')
                    with self.assertRaises(ValueError):
                        self.supplement.compare_windows(original, derived)

    def test_extra_missing_and_nonregular_evidence_are_rejected(self):
        for endpoint in ('original', 'derived'):
            for control in ('extra-file', 'missing-file', 'symlink'):
                with self.subTest(endpoint=endpoint, control=control):
                    original, derived = self.windows(endpoint + '-' + control)
                    root = original if endpoint == 'original' else derived
                    if control == 'extra-file':
                        (root / 'unexpected.json').write_bytes(b'{}')
                    elif control == 'missing-file':
                        (root / 'samples/00/stdout.json').unlink()
                    else:
                        (root / 'summary.json').unlink()
                        (root / 'summary.json').symlink_to(root / 'window.json')
                    with self.assertRaises(ValueError):
                        self.supplement.compare_windows(original, derived)
        original, derived = self.windows('missing-original-root')
        with self.assertRaises(ValueError):
            self.supplement.compare_windows(original / 'missing-original', derived)
        original, derived = self.windows('no-restoration')
        (derived / 'approval.json').write_bytes(self.canonical)
        with self.assertRaises(ValueError):
            self.supplement.compare_windows(original, derived)


if __name__ == '__main__':
    unittest.main()
