#!/usr/bin/env python3
"""Unit tests for scorecard.py (python3 tools/perf_gate/test_scorecard.py)."""
import json
import math
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scorecard as sc  # noqa: E402


def write(directory, tag, fmt, batch, per_cycle, byte_keys=None):
    with open(os.path.join(directory, f"{tag}_{fmt}_{batch}.jsonl"), "w") as f:
        f.write(json.dumps({"meta": {"seed": 1}}) + "\n")
        for cycle, metrics in per_cycle.items():
            for key, ns in metrics.items():
                f.write(json.dumps({"cycle": cycle, "format": fmt, "metric": key, "ns": ns}) + "\n")
            for key, b in (byte_keys or {}).items():
                f.write(json.dumps({"cycle": cycle, "format": fmt, "metric": key, "bytes": b}) + "\n")


ALL_MAIN_KEYS = sorted({k for c in sc.CELLS if c[3] == "main" for k in c[4]})


class ScorecardTest(unittest.TestCase):
    def make(self, d, xo_scale, overrides=None, rival_scale=None):
        """XOffset = xo_scale × zpp_bits on every key; cista 2x slower than zpp_bits.
        overrides: {key: xo/zpp ratio}. Cycle 0 is a 50x warm-up outlier and must be ignored."""
        overrides = overrides or {}
        for b in range(1, 4):
            for fmt, scale in (("zpp_bits", 1.0), ("cista_unchecked", 2.0), ("xoffset_trusted", None)):
                cyc = {}
                for c in range(0, 5):
                    base = 1000.0 * (1 + c) * (50.0 if c == 0 else 1.0)
                    if scale is None:
                        cyc[c] = {k: base * overrides.get(k, xo_scale) for k in ALL_MAIN_KEYS}
                    else:
                        cyc[c] = {k: base * scale for k in ALL_MAIN_KEYS}
                write(d, "head", fmt, b, cyc, {"UP.delta_zstd": 100 if scale is None else 80})
            d3 = {}
            for c in range(0, 5):
                base = 1000.0 * (1 + c)
                d3[c] = {"DP.pub": 0.1 * base, "DP.send": 0.5 * base, "DC.pub": 0.1 * base, "DC.send": 0.1 * base,
                         "DPTW.recv": 0.97 * base, "DCTW.recv": 1.2 * base}
            write(d, "d3", "xoffset_trusted", b, d3, {"DP.delta_zstd": 40})

    def rows(self, d):
        rows, batches = sc.evaluate(d, "xoffset_trusted", ["zpp_bits", "cista_unchecked"], "head", "d3")
        return {r["cell"]: r for r in rows}, batches

    def test_classes_and_strongest_rival(self):
        with tempfile.TemporaryDirectory() as d:
            self.make(d, 0.5, {"UP.send": 1.0, "UL.recv": 1.2, "UL.send": 1.4})
            rows, batches = self.rows(d)
            self.assertEqual(batches["xoffset_d3"], 3)
            self.assertEqual(rows["A"]["best"], "zpp_bits")  # cista is 2x slower, so zpp is the strongest
            self.assertAlmostEqual(rows["A"]["rivals"]["zpp_bits"]["gm"], 0.5, places=9)
            self.assertAlmostEqual(rows["A"]["rivals"]["cista_unchecked"]["gm"], 0.25, places=9)
            self.assertEqual(rows["A"]["class"], "win")
            self.assertEqual(rows["UP.send"]["class"], "tie")
            self.assertEqual(rows["UL.recv"]["class"], "loss")
            self.assertEqual(rows["UL.send"]["kind"], "report")

    def test_d_cells_pair_with_rival_u_rows(self):
        with tempfile.TemporaryDirectory() as d:
            self.make(d, 0.5)
            rows, _ = self.rows(d)
            self.assertAlmostEqual(rows["DP"]["rivals"]["zpp_bits"]["gm"], 0.6, places=9)
            self.assertAlmostEqual(rows["DC"]["rivals"]["zpp_bits"]["gm"], 0.2, places=9)
            self.assertEqual(rows["DPTW.recv"]["class"], "tie")  # 0.97
            self.assertEqual(rows["DCTW.recv"]["class"], "loss")  # 1.2

    def test_tie_bounds(self):
        self.assertEqual(sc.classify(0.9499), "win")
        self.assertEqual(sc.classify(0.95), "tie")
        self.assertEqual(sc.classify(1.05), "tie")
        self.assertEqual(sc.classify(1.0501), "loss")

    def test_render_counts_exclude_ul_send_and_bytes(self):
        with tempfile.TemporaryDirectory() as d:
            self.make(d, 0.5, {"UL.send": 1.4})
            rows, batches = sc.evaluate(d, "xoffset_trusted", ["zpp_bits"], "head", "d3")
            brows = sc.byte_rows(d, "xoffset_trusted", ["zpp_bits"], "head", "d3", True)
            text = sc.render(rows, batches, ["zpp_bits"], brows, True, None, None)
            self.assertIn("主格 19：赢 17，平 1，输 1", text)  # DPTW tie, DCTW loss; UL.send not counted
            by = {b["label"]: b for b in brows}
            self.assertEqual(by["UP 视野和平"]["xo"], 100)
            self.assertEqual(by["DP 共享和平 ÷ 对手 UP"]["rivals"]["zpp_bits"], 80)

    def test_missing_d3_fails_main(self):
        with tempfile.TemporaryDirectory() as d:
            self.make(d, 0.5)
            for b in range(1, 4):
                os.remove(os.path.join(d, f"d3_xoffset_trusted_{b}.jsonl"))
            out = os.path.join(d, "o.json")
            with open(os.devnull, "w") as null:
                old, sys.stdout = sys.stdout, null
                try:
                    rc = sc.main([d, "--json", out])
                finally:
                    sys.stdout = old
            self.assertEqual(rc, 1)
            with open(out) as f:
                cells = {c["cell"]: c for c in json.load(f)["cells"]}
            self.assertEqual(cells["DP"]["class"], "missing")
            self.assertTrue(math.isclose(cells["A"]["gm"], 0.5))


if __name__ == "__main__":
    unittest.main()
