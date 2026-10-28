#!/usr/bin/env python3
"""Add case aliases to an MSVC + Windows SDK tree for clang-cl/lld-link on a
case-sensitive file system: a lowercase symlink next to every mixed-case header
and library, and a symlink for every #include spelling that differs only in case.

  python3 tools/msvc-wine/case_links.py MSVC_ROOT
"""
import os, re, sys
root = sys.argv[1]
dirs = [d for d in (os.path.join(root, "Windows Kits/10/Include"), os.path.join(root, "Windows Kits/10/Lib"),
                    os.path.join(root, "VC/Tools/MSVC")) if os.path.isdir(d)]
files = {}
for top in dirs:
    for dp, dn, fn in os.walk(top):
        for f in fn:
            files.setdefault(f.lower(), []).append(os.path.join(dp, f))
made = 0
def link(target, name):
    global made
    path = os.path.join(os.path.dirname(target), name)
    if not os.path.lexists(path):
        os.symlink(os.path.basename(target), path); made += 1
for low, paths in files.items():
    for p in paths:
        if os.path.basename(p) != low:
            link(p, low)
inc = re.compile(rb'#\s*include\s*[<"]([^>"]+)[>"]')
spellings = set()
for low, paths in files.items():
    for p in paths:
        if p.lower().endswith((".h", ".hpp", ".inl", ".idl")) or "." not in os.path.basename(p):
            try:
                for m in inc.finditer(open(p, "rb").read()):
                    spellings.add(m.group(1).decode(errors="ignore").replace("\\", "/"))
            except OSError:
                pass
for s in spellings:
    base = os.path.basename(s)
    for p in files.get(base.lower(), []):
        if os.path.basename(p) != base:
            link(p, base)
print(f"{made} links")
