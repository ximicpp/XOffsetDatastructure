#!/usr/bin/env python3
"""Check (or rewrite) the sha256 manifests of the frozen benchmark trees.

usage: check_manifest.py [--write] [DIR ...]      default DIR: bench/slg bench/formal

Each DIR/MANIFEST holds '#' header lines (origin of the tree) and one
"<sha256>  <path>" line per file under DIR except MANIFEST itself. The check
fails on any missing, extra or changed file. --write keeps the header and
regenerates the file lines; use it only when re-freezing on purpose.
"""
import argparse
import hashlib
import sys
from pathlib import Path

BENCH = Path(__file__).resolve().parent
SKIP_DIRS = {"__pycache__", ".pytest_cache"}


def tree(root):
    out = {}
    for p in sorted(root.rglob("*")):
        if p.is_dir() or p.name == "MANIFEST" or SKIP_DIRS.intersection(p.relative_to(root).parts):
            continue
        out[p.relative_to(root).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def read(manifest):
    header, files = [], {}
    for line in manifest.read_text().splitlines():
        if not line.strip():
            continue
        if line.startswith("#"):
            header.append(line)
            continue
        sha, path = line.split(None, 1)
        files[path.strip()] = sha
    return header, files


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--write", action="store_true")
    p.add_argument("dirs", nargs="*", type=Path, default=[BENCH / "slg", BENCH / "formal"])
    args = p.parse_args()
    bad = 0
    for d in args.dirs:
        manifest = d / "MANIFEST"
        header, recorded = read(manifest)
        actual = tree(d)
        if args.write:
            manifest.write_text("\n".join(header + [f"{s}  {f}" for f, s in actual.items()]) + "\n")
            print(f"{manifest}: {len(actual)} files written")
            continue
        missing = sorted(set(recorded) - set(actual))
        extra = sorted(set(actual) - set(recorded))
        changed = sorted(f for f in set(recorded) & set(actual) if recorded[f] != actual[f])
        for kind, names in (("missing", missing), ("not in MANIFEST", extra), ("changed", changed)):
            for n in names:
                print(f"{d.name}/{n}: {kind}")
        if missing or extra or changed:
            bad += 1
        else:
            print(f"{manifest}: {len(actual)} files match")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
