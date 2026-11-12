#!/usr/bin/env python3
"""Unit tests for x86_columns.py (python3 tools/perf_gate/test_x86_columns.py)."""
import json
import math
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import x86_columns as xc  # noqa: E402


def write(directory, tag, fmt, batch, per_cycle):
    with open(os.path.join(directory, f"{tag}_{fmt}_{batch}.jsonl"), "w") as f:
        f.write(json.dumps({"meta": {}}) + "\n")
        for cycle, metrics in per_cycle.items():
            for key, ns in metrics.items():
                f.write(json.dumps({"cycle": cycle, "format": fmt, "metric": key, "ns": ns}) + "\n")
            f.write(json.dumps({"cycle": cycle, "format": fmt, "metric": "UPW.deliveries", "bytes": 60}) + "\n")


class X86ColumnsTest(unittest.TestCase):
    def run_case(self, xo_scale, rival_noise, ul_send=1.0):
        with tempfile.TemporaryDirectory() as d:
            for b in range(1, 6):
                for fmt, scale in (("xoffset_trusted", xo_scale), ("zpp_bits", 1.0)):
                    cyc = {}
                    for c in range(0, 5):  # cycle 0 is warm-up and must be ignored
                        # The rival's batches spread symmetrically in log space.
                        noise = math.exp(rival_noise * (-1, 1, -1, 1, 0)[b - 1]) if fmt == "zpp_bits" else 1.0
                        base = 1000.0 * (1 + c)  # cycles differ; the pairing removes it
                        cyc[c] = {"UPW.recv": base * scale * noise * (50.0 if c == 0 else 1.0),
                                  "UL.send": base * (ul_send if fmt == "xoffset_trusted" else 1.0)}
                    write(d, "head", fmt, b, cyc)
            rows = xc.evaluate(d, "head", "xoffset_trusted", ["zpp_bits"])
            return {r["key"]: r for r in rows}

    def test_faster_passes(self):
        rows = self.run_case(0.90, 0.0)
        r = rows["UPW.recv"]
        self.assertAlmostEqual(r["ratio"]["gm"], 0.90, places=6)
        self.assertEqual(r["ratio"]["batches"], 5)
        self.assertEqual(r["verdict"], "pass")

    def test_slower_fails(self):
        self.assertEqual(self.run_case(1.06, 0.0)["UPW.recv"]["verdict"], "fail")

    def test_inside_noise_warns(self):
        self.assertEqual(self.run_case(1.02, 0.0)["UPW.recv"]["verdict"], "warn")

    def test_noisy_parity_passes_on_the_point_estimate(self):
        r = self.run_case(0.99, 0.20)["UPW.recv"]
        self.assertLessEqual(r["ratio"]["gm"], 1.0)
        self.assertGreater(r["ratio"]["upper"], 1.05)
        self.assertEqual(r["verdict"], "pass")

    def test_ul_send_report_only_and_missing(self):
        rows = self.run_case(0.9, 0.0, ul_send=1.40)
        self.assertEqual(rows["UL.send"]["verdict"], "report")
        self.assertTrue(math.isclose(rows["UL.send"]["ratio"]["gm"], 1.40, rel_tol=1e-9))
        self.assertEqual(self.run_case(0.9, 0.0, ul_send=2.5)["UL.send"]["verdict"], "report")
        self.assertEqual(rows["DPTW.recv / UPW.recv"]["verdict"], "missing")
        self.assertEqual(rows["ULF.recv / UL.recv"]["verdict"], "missing")

    def test_d3_rows_pair_with_rival_u_rows(self):
        with tempfile.TemporaryDirectory() as d:
            for b in range(1, 4):
                cyc = {c: {"UP.send": 1000.0 * (1 + c), "UPW.recv": 500.0} for c in range(0, 4)}
                write(d, "head", "zpp_bits", b, cyc)
                write(d, "head", "xoffset_trusted", b, cyc)
                # D3 process: publish 200 + send 600 against the rival's UP.send of 1000 * (1 + c).
                d3 = {c: {"DP.pub": 0.2 * 1000.0 * (1 + c), "DP.send": 0.6 * 1000.0 * (1 + c),
                          "DPTW.recv": 450.0} for c in range(0, 4)}
                write(d, "headd3", "xoffset_trusted", b, d3)
            rows = {r["key"]: r for r in xc.evaluate(d, "head", "xoffset_trusted", ["zpp_bits"], "headd3")}
            self.assertAlmostEqual(rows["DP.pub+send / UP.send"]["ratio"]["gm"], 0.8, places=6)
            self.assertAlmostEqual(rows["DPTW.recv / UPW.recv"]["ratio"]["gm"], 0.9, places=6)
            self.assertEqual(rows["DP.pub+send / UP.send"]["verdict"], "pass")
            self.assertEqual(rows["DC.pub+send / UC.send"]["verdict"], "missing")

if __name__ == "__main__":
    unittest.main()
