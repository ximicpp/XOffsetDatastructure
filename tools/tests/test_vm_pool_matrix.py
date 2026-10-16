"""Fail-closed evidence tests; OS behavior is checked by the compiled harness."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('pool_runner', Path(__file__).parents[1] / 'run_vm_pool_matrix.py')
R = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(R)


class PoolEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prepared = self.root / 'prepared'
        self.prepared.mkdir()
        (self.prepared / 'timing-bench').write_text('frozen binary')
        R.write(self.prepared / 'metadata.json', dict(status='completed', runtime_identity={},
                artifacts={'timing-bench': R.sha(self.prepared / 'timing-bench')}))

    def case(self):
        return R.cases('matrix')[0]

    def row(self, label='shared', repeat=0):
        case = self.case()
        row = {k: case[k] for k in ('workload', 'distribution', 'cache', 'threads', 'cycles', 'idle_ms', 'measurement')}
        row.update(case_id=case['id'], policy=label, label=label, repeat=repeat, sequence=0, execution_position=0, checksum=42,
                   diagnostics=None, operations=16, samples={'count_budgets': [1], 'reserved_budgets': [262144], 'accessible_budgets': [65536]}, metrics={'timed_ms': 1,
                   'idle_blocks': 1, 'idle_reserved_bytes': 65536, 'idle_accessible_bytes': 65536,
                   'cleared_blocks': 0, 'cleared_reserved_bytes': 0, 'cleared_accessible_bytes': 0,
                   'idle_hits': 15, 'idle_misses': 1})
        return row

    def test_failed_and_running_windows_cannot_audit(self):
        for state in ('running', 'failed'):
            out = self.root / state
            out.mkdir()
            R.write(out / 'metadata.json', {'status': state})
            with self.assertRaisesRegex(RuntimeError, 'incomplete/failed'):
                R.audit(out)

    def test_tampered_binary_rejected(self):
        (self.prepared / 'timing-bench').write_text('different executable')
        with self.assertRaisesRegex(RuntimeError, 'artifact changed'):
            R.verify_prepared(self.prepared)

    def test_runtime_change_rejected_before_timing(self):
        runtime = self.root / 'runtime'
        runtime.write_text('library v1')
        identity = R.identity([runtime])
        runtime.write_text('library v2')
        with self.assertRaisesRegex(RuntimeError, 'identity mismatch'):
            R.verify_identity(identity)

    def test_child_failure_records_failed_window(self):
        args = type('Args', (), dict(prepared=self.prepared, out=self.root / 'failed-run', kind='matrix', repeats=1, case_filter='fixed-c1-t1'))()
        with mock.patch.object(R, 'environment', return_value={}), mock.patch.object(R, 'command', side_effect=subprocess.CalledProcessError(1, 'child')):
            with self.assertRaises(subprocess.CalledProcessError):
                R.run(args)
        meta = json.loads((args.out / 'metadata.json').read_text())
        self.assertEqual(meta['status'], 'failed')
        with self.assertRaisesRegex(RuntimeError, 'incomplete/failed'):
            R.audit(args.out)

    def test_existing_output_is_never_overwritten(self):
        out = self.root / 'existing'; out.mkdir()
        (out / 'sentinel').write_text('preserve me')
        args = type('Args', (), dict(prepared=self.prepared, out=out, kind='matrix', repeats=1, case_filter=None))()
        with self.assertRaises(FileExistsError):
            R.run(args)
        self.assertEqual((out / 'sentinel').read_text(), 'preserve me')

    def test_environment_capture_failure_records_failed_window(self):
        args = type('Args', (), dict(prepared=self.prepared, out=self.root / 'bad-environment', kind='matrix', repeats=1, case_filter=None))()
        with mock.patch.object(R, 'environment', side_effect=subprocess.CalledProcessError(1, 'ps')):
            with self.assertRaises(subprocess.CalledProcessError):
                R.run(args)
        self.assertEqual(json.loads((args.out / 'metadata.json').read_text())['status'], 'failed')

    def test_duplicate_pair_cannot_replace_missing_repeat(self):
        case = self.case(); case['policies'] = ['shared']
        with self.assertRaisesRegex(RuntimeError, 'duplicate or missing'):
            R.summarize([case], [self.row(), self.row()], 2)

    def test_different_trace_checksum_rejected(self):
        case = self.case(); case['policies'] = ['shared', 'local']
        rows = [self.row('shared'), self.row('local')]
        rows[1]['checksum'] += 1
        with self.assertRaisesRegex(RuntimeError, 'checksum differs'):
            R.summarize([case], rows, 1)

    def test_budget_and_acquisition_accounting_are_required(self):
        case = self.case()
        for key, value, message in [('idle_blocks', 2, 'budget'), ('idle_reserved_bytes', 262145, 'budget'),
                                    ('idle_accessible_bytes', 65537, 'budget'), ('cleared_blocks', 1, 'budget'),
                                    ('idle_hits', 14, 'accounting')]:
            row = self.row(); row['metrics'][key] = value
            with self.assertRaisesRegex(RuntimeError, message):
                R.validate_row(row, case, 'shared', False)

    def test_missing_individual_latencies_are_rejected(self):
        row = self.row(); case = self.case()
        row['measurement'] = case['measurement'] = 'latency'
        row['samples'].update({'acquire_ns': [1] * 15, 'release_ns': [1] * 16})
        with self.assertRaisesRegex(RuntimeError, 'missing independent latency'):
            R.validate_row(row, case, 'shared', False)

    def test_instrumentation_requires_exact_source_shape(self):
        with self.assertRaisesRegex(RuntimeError, 'pattern mismatch'):
            R.instrument('unrecognized header')

    def test_thread_local_budget_inflation_is_rejected(self):
        row = self.row('local'); case = self.case()
        row['samples']['count_budgets'] = [1, 1]
        with self.assertRaisesRegex(RuntimeError, 'partitioned budgets'):
            R.validate_row(row, case, 'local', False)

    def test_repeated_ab_control_order_is_rejected(self):
        case = R.controls()[0]
        rows = []
        for repeat in range(2):
            for position, label in enumerate(('a', 'b')):
                row = self.row('shared', repeat)
                row.update(case_id=case['id'], label=label, execution_position=position)
                rows.append(row)
        with self.assertRaisesRegex(RuntimeError, 'execution order'):
            R.summarize([case], rows, 2)

    def test_three_policies_each_occupy_each_position(self):
        case = self.case()
        orders = [R.schedule(case, repeat) for repeat in range(3)]
        for position in range(3):
            self.assertEqual({order[position][0] for order in orders}, set(case['policies']))


if __name__ == '__main__':
    unittest.main()
