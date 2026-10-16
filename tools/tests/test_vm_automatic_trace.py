"""Evidence must reject incomplete runs, mutations and unequal operation traces."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('automatic_runner', Path(__file__).parents[1] / 'run_vm_automatic_trace.py')
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.case = dict(id='small', count=100, initial=4096, maximum=65536, cadence=256, burst=1, lifetime=0)

    def tearDown(self):
        self.temp.cleanup()

    def sample(self, mode):
        return dict(**{k: v for k, v in self.case.items() if k != 'id'},
                    case_id=self.case['id'], repetition=0, mode=mode,
                    trace_checksum=42, semantic_checksum=23, diagnostic=True)

    def evidence(self, rows=None, status='complete'):
        out = self.root / 'run'; out.mkdir()
        rows = rows or [self.sample(mode) for mode in RUNNER.MODES]
        (out / 'raw.jsonl').write_text(''.join(json.dumps(r) + '\n' for r in rows))
        result = dict(status=status, cases=[self.case], runs=1, expected_samples=3,
                      timing=False, files={'raw.jsonl': RUNNER.sha(out / 'raw.jsonl')})
        RUNNER.write(out / 'result.json', result)
        (out / 'COMPLETE').write_text(RUNNER.sha(out / 'result.json') + '\n')
        return out

    def prepared(self):
        out = self.root / 'prepared'; (out / 'source').mkdir(parents=True)
        shutil.copy2(RUNNER.__file__, out / 'source/run_vm_automatic_trace.py')
        (out / 'instrumented').write_text('original binary')
        files = {str(p.relative_to(out)): RUNNER.sha(p) for p in out.rglob('*') if p.is_file()}
        RUNNER.write(out / 'prepared.json', dict(status='prepared', cases=[self.case], files=files))
        return out

    def test_complete_matrix(self):
        self.assertEqual(len(RUNNER.audit(self.evidence())[1]), 3)

    def test_incomplete_even_with_marker(self):
        with self.assertRaisesRegex(RuntimeError, 'incomplete'):
            RUNNER.audit(self.evidence(status='running'))

    def test_marker_required(self):
        out = self.evidence(); (out / 'COMPLETE').unlink()
        with self.assertRaisesRegex(RuntimeError, 'incomplete'):
            RUNNER.audit(out)

    def test_changed_raw(self):
        out = self.evidence(); (out / 'raw.jsonl').write_text('changed')
        with self.assertRaisesRegex(RuntimeError, 'evidence changed'):
            RUNNER.audit(out)

    def test_unequal_trace(self):
        rows = [self.sample(mode) for mode in RUNNER.MODES]; rows[-1]['trace_checksum'] = 99
        with self.assertRaisesRegex(RuntimeError, 'same-trace'):
            RUNNER.audit(self.evidence(rows))

    def test_same_trace_but_wrong_values(self):
        rows = [self.sample(mode) for mode in RUNNER.MODES]; rows[-1]['semantic_checksum'] = 99
        with self.assertRaisesRegex(RuntimeError, 'semantic'):
            RUNNER.audit(self.evidence(rows))

    def test_duplicate_policy(self):
        rows = [self.sample('explicit')] * 3
        with self.assertRaisesRegex(RuntimeError, 'duplicate'):
            RUNNER.audit(self.evidence(rows))

    def test_changed_parameter(self):
        rows = [self.sample(mode) for mode in RUNNER.MODES]; rows[-1]['cadence'] = 1024
        with self.assertRaisesRegex(RuntimeError, 'parameters'):
            RUNNER.audit(self.evidence(rows))

    def test_diagnostic_not_timing(self):
        rows = [self.sample(mode) for mode in RUNNER.MODES]; rows[-1]['diagnostic'] = False
        with self.assertRaisesRegex(RuntimeError, 'mode mismatch'):
            RUNNER.audit(self.evidence(rows))

    def test_modified_prepared_binary(self):
        out = self.prepared(); (out / 'instrumented').write_text('mutated binary')
        with self.assertRaisesRegex(RuntimeError, 'input changed'):
            RUNNER.verify_prepared(out)

    def test_runtime_mutation_does_not_publish_completion(self):
        prepared = self.prepared(); out = self.root / 'runtime'
        args = argparse.Namespace(prepared=prepared, out=out, case=None, runs=1, timing=False, compare_prepared=None)
        def fake_command(cmd):
            (prepared / 'instrumented').write_text('changed during execution')
            return argparse.Namespace(stdout=json.dumps(self.sample(str(cmd[cmd.index('--mode') + 1]))))
        with mock.patch.object(RUNNER, 'command', side_effect=fake_command):
            with self.assertRaisesRegex(RuntimeError, 'input changed'):
                RUNNER.run(args)
        self.assertEqual(json.loads((out / 'result.json').read_text())['status'], 'failed')
        self.assertFalse((out / 'COMPLETE').exists())

    def test_instrumentation_refuses_unrecognized_source(self):
        with self.assertRaisesRegex(RuntimeError, 'anchor count'):
            RUNNER.instrument('unrecognized header')

    def test_manifest_and_binary_cannot_change_together(self):
        prepared = self.prepared(); out = self.root / 'runtime'
        args = argparse.Namespace(prepared=prepared, out=out, case=None, runs=1, timing=False, compare_prepared=None)
        def fake_command(cmd):
            (prepared / 'instrumented').write_text('changed binary')
            meta = json.loads((prepared / 'prepared.json').read_text())
            meta['files']['instrumented'] = RUNNER.sha(prepared / 'instrumented')
            RUNNER.write(prepared / 'prepared.json', meta)
            return argparse.Namespace(stdout=json.dumps(self.sample(str(cmd[cmd.index('--mode') + 1]))))
        with mock.patch.object(RUNNER, 'command', side_effect=fake_command):
            with self.assertRaisesRegex(RuntimeError, 'manifest changed'):
                RUNNER.run(args)
        self.assertEqual(json.loads((out / 'result.json').read_text())['status'], 'failed')
        self.assertFalse((out / 'COMPLETE').exists())

    def test_child_timeout_fails_run(self):
        prepared = self.prepared(); out = self.root / 'runtime'
        args = argparse.Namespace(prepared=prepared, out=out, case=None, runs=1, timing=False, compare_prepared=None)
        with mock.patch.object(RUNNER, 'command', side_effect=subprocess.TimeoutExpired('instrumented', 120)):
            with self.assertRaises(subprocess.TimeoutExpired): RUNNER.run(args)
        self.assertEqual(json.loads((out / 'result.json').read_text())['status'], 'failed')
        self.assertFalse((out / 'COMPLETE').exists())

    def test_child_command_has_timeout(self):
        with mock.patch.object(RUNNER.subprocess, 'run', return_value=argparse.Namespace(stdout='')) as process:
            RUNNER.command(['example'])
        self.assertEqual(process.call_args.kwargs['timeout'], 120)


if __name__ == '__main__': unittest.main()
