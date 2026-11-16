"""Score same-process rival ratios in the nine paired CppCon runs."""
import re
import statistics
import sys
from pathlib import Path


def read_file(path):
    metrics = {}
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            continue
        match = re.fullmatch(r"(.+?)\s+(\S+)\s+([0-9.]+)", line)
        if not match:
            raise ValueError(f"{path}: invalid row {line!r}")
        name, metric, value = match.groups()
        key = name, metric
        if key in metrics:
            raise ValueError(f"{path}: duplicate row {key!r}")
        metrics[key] = float(value)
    return metrics


root = Path(sys.argv[1])
candidate_name = "xoffset inline"
base_runs = [read_file(root / f"{i:02d}-baseline.txt") for i in range(1, 10)]
new_runs = [read_file(root / f"{i:02d}-candidate.txt") for i in range(1, 10)]
sr_rivals = (
    "zpp::bits", "MessagePack", "Protocol Buffers", "FlatBuffers", "Cap'n Proto",
    "cista raw", "cista offset",
)
rw_rivals = ("std (shared)", "Protocol Buffers", "cista raw", "cista offset")

passed = True
print("| Metric | PR #286 | Audited original | Inline layout | Fastest rival | Median ratio | Wins |")
print("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
for metric, rivals in (("sr_reuse", sr_rivals), ("sr_fresh", sr_rivals), ("rw", rw_rivals)):
    base = [m["xoffset 4.0", metric] for m in base_runs]
    audited = [m["xoffset 4.0", metric] for m in new_runs]
    new = [m[candidate_name, metric] for m in new_runs]
    fastest = [min(m[r, metric] for r in rivals) for m in new_runs]
    ratios = [n / f for n, f in zip(new, fastest)]
    wins = sum(r < 1 for r in ratios)
    med = statistics.median
    passed &= med(ratios) <= 0.95 and wins >= 8
    print(f"| {metric} | {med(base):.1f} | {med(audited):.1f} | {med(new):.1f} | "
          f"{med(fastest):.1f} | {med(ratios):.3f} | {wins}/9 |")

print()
for rival in rw_rivals:
    ratio = statistics.median(m[candidate_name, "rw"] / m[rival, "rw"] for m in new_runs)
    rival_rw = statistics.median(m[rival, "rw"] for m in new_runs)
    print(f"- RW vs {rival}: {ratio:.3f} (rival median {rival_rw:.1f} ns)")

print()
for metric in ("bytes_zstd3", "bytes_after_rw_zstd3", "bytes_after_rw_compact_zstd3"):
    ratios = [
        m[candidate_name, metric] / min(m[r, "bytes_zstd3"] for r in sr_rivals)
        for m in new_runs
    ]
    sizes = {int(m[candidate_name, metric]) for m in new_runs}
    passed &= max(ratios) <= 1.15
    print(f"- {metric}: {', '.join(map(str, sorted(sizes)))} B (max ratio {max(ratios):.3f})")

print()
print("PASS" if passed else "FAIL")
sys.exit(0 if passed else 1)
