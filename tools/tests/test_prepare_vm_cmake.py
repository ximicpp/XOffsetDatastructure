"""The optional download step must reject bytes before extracting/executing them."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class CMakePreparation(unittest.TestCase):
    def test_bad_checksum_records_failure_without_extracting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / 'untrusted.tar.gz'
            archive.write_bytes(b'not an archive')
            config = {'build_tools': {'cmake': {'version': 'fixture', 'archives': {'linux-x86_64': {
                'url': 'https://example.invalid/never-requested', 'sha256': hashlib.sha256(b'expected').hexdigest()}}}}}
            manifest = root / 'lock.json'
            manifest.write_text(json.dumps(config))
            out = root / 'result'
            script = Path(__file__).resolve().parents[1] / 'prepare_vm_cmake.py'
            command = [sys.executable, str(script), '--platform', 'linux-x86_64', '--out', str(out),
                       '--archive', str(archive), '--manifest', str(manifest)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            record = json.loads((out / 'result.json').read_text())
            self.assertEqual(record['status'], 'failed')
            self.assertIn('checksum', record['reason'])
            self.assertFalse((out / 'root').exists())
            before = (out / 'result.json').read_bytes()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual((out / 'result.json').read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
