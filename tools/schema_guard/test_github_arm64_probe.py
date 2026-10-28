import copy
import json
import subprocess

import wire_inputs
import unittest

import github_arm64_probe as probe

class GithubArm64InputTests(unittest.TestCase):
    def setUp(self):
        fingerprint, inputs = wire_inputs.fingerprint()
        gitlinks = {name: subprocess.check_output(["git", "rev-parse", "HEAD:" + name], cwd=probe.ROOT, text=True).strip()
                    for name in ("external/boost", "external/typelayout")}
        self.data = {"fingerprint": fingerprint, "inputs": inputs, "gitlinks": gitlinks}

    def test_current_source_inputs_are_valid(self):
        probe.verify_inputs(self.data)

    def test_stale_fingerprint_is_rejected(self):
        data = copy.deepcopy(self.data)
        data["fingerprint"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            probe.verify_inputs(data)

    def test_unexpected_source_file_is_rejected(self):
        data = copy.deepcopy(self.data)
        data["inputs"]["files"]["other.hpp"] = "0" * 64
        import hashlib
        encoded = json.dumps(data["inputs"], sort_keys=True, separators=(",", ":")).encode()
        data["fingerprint"] = hashlib.sha256(encoded).hexdigest()
        with self.assertRaisesRegex(ValueError, "source file set"):
            probe.verify_inputs(data)

    def test_wrong_gitlink_is_rejected(self):
        data = copy.deepcopy(self.data)
        data["gitlinks"]["external/boost"] = "0" * 40
        with self.assertRaisesRegex(ValueError, "gitlink changed"):
            probe.verify_inputs(data)

if __name__ == "__main__":
    unittest.main()
