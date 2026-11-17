#!/usr/bin/env python3
"""Fail when a tracked text file contains Chinese, Japanese or Korean characters.

The repository is English only (2026-11-17): documents, comments, strings and
tool output. Non-ASCII text such as "÷", "–" or "café" is fine; CJK ideographs,
kana, hangul, CJK punctuation and full-width forms are not, because they get
garbled on compilers and terminals that assume another code page. Test data that
needs such bytes writes them as escapes (for example "\\xE6\\x95\\xB0").

usage: check_no_cjk.py [REPO]   (default: the repository holding this script)
Binary files (a NUL byte in the first 8 KiB) and submodules are skipped.
"""
import pathlib
import re
import subprocess
import sys

# Code point ranges: CJK radicals, punctuation, kana, bopomofo, enclosed/compatibility,
# ideographs (BMP and planes 2-3), hangul, compatibility ideographs, vertical and
# small forms, half-width and full-width forms.
RANGES = [(0x2E80, 0x2FDF), (0x3000, 0x303F), (0x3040, 0x30FF), (0x3100, 0x31FF), (0x3200, 0x4DBF),
          (0x4E00, 0x9FFF), (0xA960, 0xA97F), (0xAC00, 0xD7FF), (0xF900, 0xFAFF), (0xFE30, 0xFE4F),
          (0xFF00, 0xFFEF), (0x20000, 0x3FFFF)]
CJK = re.compile("[" + "".join(f"{chr(a)}-{chr(b)}" for a, b in RANGES) + "]")


def main(argv):
    repo = pathlib.Path(argv[1] if len(argv) > 1 else pathlib.Path(__file__).resolve().parent.parent)
    out = subprocess.run(["git", "-C", str(repo), "ls-files", "-z", "-s"],
                         capture_output=True, check=True).stdout.decode("utf-8", "replace")
    bad = checked = 0
    for entry in filter(None, out.split("\0")):
        mode, _, rest = entry.partition(" ")
        if mode == "160000":  # submodule
            continue
        name = rest.split("\t", 1)[1]
        path = repo / name
        if not path.is_file():
            continue
        data = path.read_bytes()
        if b"\0" in data[:8192]:
            continue
        checked += 1
        for n, line in enumerate(data.decode("utf-8", "replace").splitlines(), 1):
            if CJK.search(line):
                print(f"{name}:{n}: {line.strip()[:120]}")
                bad += 1
    print(f"CJK characters: {bad} lines in {checked} tracked text files")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
