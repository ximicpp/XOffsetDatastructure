import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fuzz_gcc16 as fg  # noqa: E402

PASS = 'test_v2_fuzz: seed=%s golden=64 sparse=20 cases=1000 boundary_cases=1505 accepted=1 header_rejects=1 payload_rejects=1 inconsistencies=0\n'

# A replay binary stand-in: names each input on stderr like replay_main.cpp and
# fails (differential report, abort status) on inputs starting with BAD.
FAKE_REPLAY = '''#!/usr/bin/env python3
import sys
for path in sys.argv[1:]:
    data = open(path, 'rb').read()
    sys.stderr.write('replay: %s (%d bytes)\\n' % (path, len(data)))
    if data.startswith(b'BAD'):
        sys.stderr.write('v2 fuzz FAILED: case input: typed=accept oracle=reject(size 12)\\n')
        sys.stderr.flush()
        sys.exit(134)
print('replay: %d inputs, 0 bytes, all passed' % (len(sys.argv) - 1))
'''


def executable(path, text):
    path.write_text(text)
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path


class Diff(unittest.TestCase):
    def test_failed_and_killed_processes_become_findings(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            logs = tmp / 'logs'
            logs.mkdir()
            wire = tmp / 'v2fuzz-s11-c42.wire'
            wire.write_bytes(b'wire bytes')
            (logs / 'p0.log').write_text(PASS % 10)
            (logs / 'p0.exit').write_text('0\n')
            (logs / 'p1.log').write_text('v2 fuzz FAILED: case 42: raw=accept oracle=reject(bad 0x10)\n'
                                         'v2 fuzz replay: test_v2_fuzz --replay 11 42\n'
                                         'v2 fuzz dump: %s\n' % wire)
            (logs / 'p1.exit').write_text('134\n')
            (logs / 'p2.log').write_text('')
            (logs / 'p2.exit').write_text('137\n')
            out = tmp / 'report'
            fg.main(['diff', '--logs', str(logs), '--seeds', '10 11 12', '--cases', '1000',
                     '--image', 'gcc@sha256:x', '--out', str(out)])
            doc = json.loads((out / 'findings.json').read_text())
            self.assertEqual((doc['job'], doc['needs'], doc['artifact']), ('gcc16-diff', 'gcc16-diff', 'fuzz-gcc16-diff'))
            self.assertEqual([p['completed'] for p in doc['processes']], [True, False, False])
            by_kind = {f['kind']: f for f in doc['findings']}
            self.assertEqual(set(by_kind), {'crash', 'timeout'})
            crash = by_kind['crash']
            self.assertEqual(crash['target'], 'v2_wire')
            self.assertTrue(crash['error_type'].startswith('diff: case N:'))
            self.assertEqual(crash['replay_cmd'], 'test_v2_fuzz --replay 11 42')
            self.assertIn('build/gcc16-asan/bin/Debug/test_v2_fuzz --replay 11 42', crash['reproduce'][0])
            self.assertEqual((out / crash['input']).read_bytes(), b'wire bytes')
            self.assertEqual(len(crash['signature']), 10)
            self.assertNotIn('input', by_kind['timeout'])
            self.assertEqual(by_kind['timeout']['replay_cmd'], 'test_v2_fuzz 1000 12')

    def test_no_findings_still_writes_the_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / 'p0.log').write_text(PASS % 5)
            (tmp / 'p0.exit').write_text('0\n')
            fg.main(['diff', '--logs', str(tmp), '--seeds', '5', '--cases', '1000', '--image', 'i',
                     '--out', str(tmp / 'report')])
            self.assertEqual(json.loads((tmp / 'report' / 'findings.json').read_text())['findings'], [])


class Replay(unittest.TestCase):
    def test_replay_continues_after_each_failing_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            wrapper = executable(tmp / 'wrapper', '#!/bin/sh\nexec "$@"\n')
            binary = executable(tmp / 'fuzz_v2_wire_replay', FAKE_REPLAY)
            corpus = tmp / 'corpus'
            corpus.mkdir()
            for i in range(7):
                (corpus / ('in-%d' % i)).write_bytes(b'BAD one' if i in (2, 5) else b'good')
            (corpus / '.gitkeep').write_bytes(b'BAD but skipped')
            out = tmp / 'report'
            code = fg.main(['replay', '--image', 'i', '--out', str(out), '--wrapper', str(wrapper),
                            '--chunk', '3', '--spec', 'v2_wire:%s:%s:%s' % (binary, corpus, tmp / 'missing')])
            self.assertEqual(code, 0)
            doc = json.loads((out / 'findings.json').read_text())
            self.assertEqual(doc['inputs'], {'v2_wire': {'inputs': 7, 'failed': 2, 'runs': 3}})
            [finding] = doc['findings']
            self.assertEqual(finding['artifacts'], [str(corpus / 'in-2'), str(corpus / 'in-5')])
            self.assertEqual((out / finding['input']).read_bytes(), b'BAD one')
            self.assertEqual(finding['replay_cmd'],
                             'fuzz_v2_wire_replay tests/fuzz/regressions/v2_wire/' + finding['signature'])


if __name__ == '__main__':
    unittest.main()
