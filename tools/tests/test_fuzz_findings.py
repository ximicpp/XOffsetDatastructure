import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fuzz_findings as ff  # noqa: E402

ASAN = '''INFO: Running with entropic power schedule (0xFF, 100).
Running: build/fuzz-artifacts/v2_wire-crash-1
=================================================================
==12==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000031 at pc 0x55 bp 0x7f sp 0x7f
READ of size 4 at 0x502000000031 thread T0
    #0 0x55d0c8a1 in xo::detail::read_u32(unsigned char const*) /workspace/include/xoffset/wire.hpp:120:12
    #1 0x55d0c8b2 in bool xo::validate<fz::Root>(std::span<unsigned char const, 18446744073709551615ul>) /workspace/include/xoffset/validate.hpp:40:5
    #2 0x55d0c8c3 in boost::foo() /workspace/external/boost/x.hpp:1:1
    #3 0x55d0c8d4 in (anonymous namespace)::run_case(std::string const&) /workspace/tests/fuzz/v2_fuzz_core.hpp:690:9
    #4 0x55d0c8e5 in LLVMFuzzerTestOneInput /workspace/tests/fuzz/fuzz_v2_wire.cpp:12:5
    #5 0x55d0c8f6 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/workspace/build/fuzz/bin/Debug/fuzz_v2_wire+0x5f0) (BuildId: abcdef01)

0x502000000031 is located 0 bytes after 1-byte region
'''

DIFF = '''v2 fuzz FAILED: case 4711: typed=accept, reload=reject(bad header 0x10)
==7== ERROR: libFuzzer: deadly signal
    #0 0x1 in __sanitizer_print_stack_trace (/workspace/build/fuzz/bin/Debug/fuzz_v2_wire+0x1)
    #1 0x2 in abort (/lib/x86_64-linux-gnu/libc.so.6+0x2) (BuildId: 12ab)
    #2 0x3 in (anonymous namespace)::fail(std::string const&) /workspace/tests/fuzz/v2_fuzz_core.hpp:76:5
    #3 0x4 in (anonymous namespace)::check_case(unsigned long) /workspace/tests/fuzz/v2_fuzz_core.hpp:693:9
    #4 0x5 in LLVMFuzzerTestOneInput /workspace/tests/fuzz/fuzz_v2_wire.cpp:12:5
'''


# Shape of a real drill crash (fuzz.yml run 39783623036): frames without a
# function name (libc, no symbols) sit between symbolized ones.
INJECT = '''XOFFSET_FUZZ_INJECT_CRASH: injected crash (drill input)
==116== ERROR: libFuzzer: deadly signal
    #0 0x5620090fb555 in __sanitizer_print_stack_trace (/workspace/build/fuzz/bin/Debug/fuzz_wire_schemas+0x170555)
    #2 0x56200902ebc7 in fuzzer::Fuzzer::CrashCallback() (/workspace/build/fuzz/bin/Debug/fuzz_wire_schemas+0xa3bc7)
    #3 0x78962515e32f  (/lib/x86_64-linux-gnu/libc.so.6+0x4532f) (BuildId: 8e9fd827446c24067541ac5390e6f527fb5947bb)
    #6 0x7896251418fe in abort (/lib/x86_64-linux-gnu/libc.so.6+0x288fe) (BuildId: 8e9fd827446c24067541ac5390e6f527fb5947bb)
    #7 0x56200913776c in (anonymous namespace)::inject_crash_drill(unsigned char const*, unsigned long) /workspace/tests/fuzz/v2_fuzz_core.hpp:115:9
    #8 0x56200913776c in LLVMFuzzerTestOneInput /workspace/tests/fuzz/fuzz_wire_schemas.cpp:206:5
    #9 0x56200903019f in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/workspace/build/fuzz/bin/Debug/fuzz_wire_schemas+0xa519f)

NOTE: libFuzzer has rudimentary signal handlers.
'''


class Signature(unittest.TestCase):
    def test_unnamed_frames_do_not_end_the_stack(self):
        info = ff.classify('wire_schemas', 'crash', INJECT)
        self.assertEqual(info['error_type'], 'deadly signal')
        self.assertEqual(info['top_frames'], ['{anon}::inject_crash_drill', 'LLVMFuzzerTestOneInput'])

    def test_asan_frames_skip_external_and_build(self):
        info = ff.classify('v2_wire', 'crash', ASAN)
        self.assertEqual(info['error_type'], 'asan: heap-buffer-overflow')
        self.assertEqual(info['top_frames'], ['xo::detail::read_u32', 'bool xo::validate<fz::Root>',
                                              '{anon}::run_case'])
        self.assertTrue(info['reproduced'])
        self.assertEqual(len(info['signature']), 10)
        self.assertTrue(info['log_head'].startswith('READ') is False)

    def test_diff_failure_masks_case_numbers(self):
        a = ff.classify('v2_wire', 'crash', DIFF)
        b = ff.classify('v2_wire', 'crash', DIFF.replace('4711', '9').replace('0x10', '0x99'))
        self.assertEqual(a['error_type'], "diff: case N: typed=accept, reload=reject(…)")
        self.assertEqual(a['signature'], b['signature'])
        self.assertEqual(a['top_frames'][:2], ['{anon}::fail', '{anon}::check_case'])

    def test_not_reproduced(self):
        info = ff.classify('wire_schemas', 'timeout', 'INFO: Running\nExecuted x in 3 ms\n')
        self.assertEqual(info['error_type'], 'timeout (not reproduced)')
        self.assertFalse(info['reproduced'])

    def test_artifact_kind(self):
        self.assertEqual(ff.artifact_kind('v2_wire', 'v2_wire-crash-abc'), 'crash')
        self.assertEqual(ff.artifact_kind('v2_wire', 'v2_wire-oom-abc'), 'oom')
        self.assertIsNone(ff.artifact_kind('v2_wire', 'v2_wire-slow-unit-abc'))

    def test_status_lines(self):
        fork = '#120000: cov: 900 ft: 4000 corp: 812 exec/s 3900 oom/timeout/crash: 0/0/0 time: 60s job: 9 dft_time: 0\n'
        self.assertEqual(ff.last_status(fork), {'execs': 120000, 'cov': 900, 'ft': 4000, 'exec_per_sec': 3900})
        single = ('#65536\tpulse  cov: 10 ft: 20 corp: 5/1Kb exec/s: 100 rss: 50Mb\n'
                  '#70000\tDONE   cov: 11 ft: 21 corp: 6/1Kb lim: 4 exec/s: 120 rss: 50Mb\n'
                  'stat::number_of_executed_units: 70001\nstat::average_exec_per_sec:     121\n')
        self.assertEqual(ff.last_status(single), {'execs': 70001, 'cov': 11, 'ft': 21, 'exec_per_sec': 121})

    def test_summary_without_findings(self):
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            (t / 'corpus').mkdir()
            (t / 'corpus' / 'a').write_bytes(b'xyz')
            (t / 'before').write_text('0 0 none\n')
            (t / 'corpus.log').write_text('#2\tINITED cov: 77 ft: 88 corp: 1/3b exec/s: 0 rss: 30Mb\n')
            (t / 'exit').write_text('0\n')
            ff.main(['summary', '--target', 'v2_wire', '--mode', 'pr', '--seconds', '300',
                     '--log', str(t / 'missing'), '--before', str(t / 'before'), '--corpus', str(t / 'corpus'),
                     '--corpus-log', str(t / 'corpus.log'), '--merge-log', str(t / 'missing'),
                     '--exit-file', str(t / 'exit'), '--findings', str(t / 'missing'), '--out', str(t / 'out')])
            stats = json.loads((t / 'out' / 'stats.json').read_text())
            self.assertEqual((stats['corpus_files'], stats['corpus_bytes'], stats['corpus_cov']), (1, 3, 77))
            self.assertEqual(stats['findings'], 0)


if __name__ == '__main__':
    unittest.main()
