#!/usr/bin/env python3
"""Writes the XOFFSET_RECORD lines the C++20/C++23 builds need into the test tree.

Only records that a C++20 build actually asks for get a line: each round compiles
every translation unit with a C++20 compiler (-fsyntax-only), collects the records
the library reports as having no field list, has tools/describe_dump.hpp print
their lines under C++26 reflection (P2996 Clang), and inserts each line at global
scope right after the record's definition. Rounds repeat until no record is
missing (a described root exposes its member records).

Every line is written as XOFFSET_RECORD_DERIVED(T, (bases...), members...), the
field list without the ordinary-record declaration: a test record keeps exactly
the ordinary declarations it already has (XOFFSET_ORDINARY_RECORDS or a
wire_record_layout specialization), so the C++26 build takes the same paths as
before, and the C++20 build gates SHIFT, stable_erase_if and raw receive on the
same declarations.

A record whose definition uses XOFFSET_TEST_ALLOCATOR_CTORS (tests/test_allocator_ctors.hpp)
keeps its allocator constructor only in the C++26 reflection build; it is described
with the constructors dropped, and its line is written under #if !XOFFSET_TEST_ALLOCATOR_CTORS.

  tools/describe_tests.py --compile-commands <C++26 build>/compile_commands.json
                          [--cxx20 g++-13] [--p2996 /opt/p2996-toolchain] [--rounds 8]
                          [--report FILE] [FILE.cpp ...]

The C++26 build (P2996) gives each translation unit's flags. Records that cannot
get a line (local classes, unions, non-aggregates, std types) are listed in the
report and left alone.
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MISSING = re.compile(r"describe_detail::of<(.+?)>'")


def cxx26_only():
    """Programs the C++20/C++23 builds do not build (tests/CMakeLists.txt list + tools/examples)."""
    text = open(os.path.join(REPO, "tests/CMakeLists.txt")).read()
    m = re.search(r"set\(XOFFSET_CXX26_ONLY_TESTS\n(.*?)\)", text, re.S)
    names = set(m.group(1).split()) if m else set()
    return names | {"export_signatures", "check_compat", "demo", "describe_dump"}


def translation_units(commands, only):
    skip = cxx26_only()
    units = []
    for e in json.load(open(commands)):
        path = os.path.realpath(e["file"])
        rel = os.path.relpath(path, REPO)
        if rel.startswith("..") or not rel.split("/")[0] in ("tests", "examples", "tools", "bench"):
            continue
        if os.path.splitext(os.path.basename(path))[0] in skip or "/ordinary_records/" in path:
            continue
        if only and rel not in only:
            continue
        toks = e["command"].split()
        flags = [t for t in toks if t.startswith(("-I", "-D"))]
        for i, t in enumerate(toks):
            if t == "-isystem" and toks[i + 1].startswith(REPO):
                flags += ["-isystem", toks[i + 1]]
        units.append((path, flags))
    return units


def missing_records(cxx, path, flags):
    cmd = [cxx, "-std=c++20", "-fsyntax-only", "-fmax-errors=0"]
    for i, f in enumerate(flags):    # the C++20 build has no TypeLayout include path
        if "typelayout" not in f and not (f == "-isystem" and "typelayout" in flags[i + 1]):
            cmd.append(f)
    cmd.append(path)
    p = subprocess.run(cmd, capture_output=True, text=True, env=dict(os.environ, LC_ALL="C"))
    return sorted({m.strip() for m in MISSING.findall(p.stderr)})


HELPER = r'''
#define main xoffset_describe_tests_hidden_main
#include "%(tu)s"
#undef main
#include "describe_dump.hpp"
#include <cstdio>
template <class T> void one(const char* spelled) {
    const auto lines = xoffset_describe_dump::lines<T>();
    constexpr auto loc = std::meta::source_location_of(^^T);
    std::printf("%%s\t%%s\t%%u\t%%s\n", spelled, loc.file_name(), (unsigned)loc.line(),
                lines.empty() ? "" : lines.back().c_str());
}
int main() {
%(calls)s
}
'''


def describe(p2996, path, flags, names, work):
    calls = "\n".join(f'    one<{n}>("{n}");' for n in names)
    src = os.path.join(work, os.path.basename(path) + ".describe.cpp")
    exe = src[:-4]
    open(src, "w").write(HELPER % {"tu": path, "calls": calls})
    lib = os.path.join(p2996, "lib/x86_64-unknown-linux-gnu")
    cmd = [os.path.join(p2996, "bin/clang++"), "-std=c++26", "-freflection", "-fexpansion-statements",
           "-stdlib=libc++", f"-Wl,-rpath,{lib}", "-DXOFFSET_TEST_ALLOCATOR_CTORS=0", "-I" + os.path.join(REPO, "tools"),
           "-I" + os.path.dirname(path)] + flags + [src, "-o", exe]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        return None, p.stderr
    out = subprocess.run([exe], capture_output=True, text=True).stdout
    rows = []
    for line in out.splitlines():
        spelled, file, lineno, record = line.split("\t", 3)
        rows.append((spelled, os.path.realpath(file), int(lineno), record))
    return rows, ""


def first_error(stderr):
    lines = stderr.strip().splitlines() or [""]
    return next((l for l in lines if "error:" in l), lines[0])[:300]


def as_field_list(record, spelled):
    """XOFFSET_RECORD(T, m...) -> XOFFSET_RECORD_DERIVED(T, (), m...); names spelled as the C++20 compiler did."""
    record = record.replace("<local or unnamed scope>::", "")
    m = re.match(r"XOFFSET_RECORD\((.*)\)$", record)
    if m:
        name, _, members = m.group(1).partition(", ")
        record = f"XOFFSET_RECORD_DERIVED({name}, (), {members})" if members else f"XOFFSET_RECORD_DERIVED({name}, ())"
    return record


# ---------------------------------------------------------------- source scanning

TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|R"\((?:.|\n)*?\)"|[{};()]|[A-Za-z_]\w*|\n|.', re.S)


def strip_template_head(tokens):
    """Tokens after a leading template <...> parameter list."""
    toks = [t for t in tokens if t.strip()]
    while toks[:1] == ["template"]:
        depth, i = 0, 1
        for i in range(1, len(toks)):
            if toks[i] == "<":
                depth += 1
            elif toks[i] == ">":
                depth -= 1
                if depth == 0:
                    break
        toks = toks[i + 1:]
    return toks


def is_class_item(tokens):
    toks = strip_template_head(tokens)
    return bool(toks) and toks[0] in ("struct", "class", "union", "enum")


class Item:
    def __init__(self, kind, start, end, names):
        self.kind, self.start, self.end, self.names = kind, start, end, names


def scan(text):
    """Top-level items of every namespace body: (namespace path, [Item]) plus each namespace's closing line."""
    blocks = {(): []}
    closes = {}
    stack = []          # entries: ("ns", path) or ("other", None)
    line = 1
    item_start, item_tokens = None, []
    pending_ns = None

    def ns_path():
        for kind, path in reversed(stack):
            if kind == "ns":
                return path
        return ()

    def at_ns_body():
        return not stack or stack[-1][0] == "ns"

    def finish(end_line):
        nonlocal item_start, item_tokens
        if item_start is None:
            return
        toks = strip_template_head(item_tokens)
        kind, names = "other", []
        if is_class_item(item_tokens) and "{" in toks:
            kind = "type"
        elif toks[:1] in (["using"], ["typedef"]) and toks[1:2] != ["namespace"]:
            kind = "type"
        blocks.setdefault(ns_path(), []).append(Item(kind, item_start, end_line, names))
        item_start, item_tokens = None, []

    # Preprocessor lines (with continuations) become blank lines.
    out, cont = [], False
    for l in text.split("\n"):
        directive = cont or l.lstrip().startswith("#")
        cont = directive and l.rstrip().endswith("\\")
        out.append("" if directive else l)
    text = "\n".join(out)
    depth_in_item = 0
    parens = 0
    for tok in TOKEN.findall(text):
        if tok == "\n":
            line += 1
            continue
        if tok.startswith("//") or tok.startswith("/*") or not tok.strip():
            line += tok.count("\n")
            continue
        line_here = line
        line += tok.count("\n")
        if at_ns_body() and depth_in_item == 0:
            if tok == "namespace" and item_start is None:
                pending_ns = []
                continue
            if pending_ns is not None:
                if tok == "{":
                    # One entry per brace: "a::b" for namespace a::b, "" for an unnamed one.
                    path = ns_path() + ("::".join(pending_ns),)
                    stack.append(("ns", path))
                    blocks.setdefault(path, [])
                    pending_ns = None
                elif tok == ";":       # namespace alias
                    pending_ns = None
                elif re.match(r"[A-Za-z_]\w*$", tok) and tok != "inline":
                    pending_ns.append(tok)
                continue
            if tok == "}" and stack and stack[-1][0] == "ns":
                closes[stack[-1][1]] = line_here
                stack.pop()
                continue
            if item_start is None:
                if tok == ";":
                    continue
                item_start = line_here
                parens = 0
            item_tokens.append(tok)
            if tok == "(":
                parens += 1
            elif tok == ")":
                parens -= 1
                # A macro invocation at namespace scope, as XOFFSET_REGISTER_SCHEMA_NAME(T, "n"),
                # needs no semicolon.
                if parens == 0 and re.match(r"[A-Z_][A-Z0-9_]*$", item_tokens[0]) and "{" not in item_tokens:
                    finish(line_here)
            elif tok == "{":
                depth_in_item = 1
            elif tok == ";":
                finish(line_here)
            continue
        if depth_in_item:
            item_tokens.append(tok)
            if tok == "{":
                depth_in_item += 1
            elif tok == "}":
                depth_in_item -= 1
                if depth_in_item == 0:
                    # A class body ends with "};", a function body with "}".
                    if is_class_item(item_tokens):
                        continue
                    finish(line_here)
            elif tok == ";" and depth_in_item == 0:
                finish(line_here)
            continue
    return blocks, closes


def definition_text(text, def_line):
    """The text of the type definition that contains def_line ("" if none)."""
    blocks, _ = scan(text)
    lines = text.split("\n")
    for items in blocks.values():
        for item in items:
            if item.kind == "type" and item.start <= def_line <= item.end:
                return "\n".join(lines[item.start - 1:item.end])
    return ""


def insertion(text, def_line):
    """(line after which to insert, namespaces to close and reopen) for a record defined at def_line.

    The line goes after the run of type definitions that contains the record. At
    global scope that is the place; inside one namespace whose body ends with the
    run, it is after the namespace's closing brace; otherwise the enclosing
    namespaces are closed for the line and reopened after it.
    """
    blocks, closes = scan(text)
    for path, items in blocks.items():
        for idx, item in enumerate(items):
            if item.kind == "type" and item.start <= def_line <= item.end:
                j = idx
                while j + 1 < len(items) and items[j + 1].kind == "type":
                    j += 1
                if not path:
                    return items[j].end, ()
                if len(path) == 1 and j + 1 == len(items):
                    return closes[path], ()
                return items[j].end, path
    return None, None


def insert_lines(path, entries):
    """entries: [(def_line, record_line)]; each group of lines in definition order."""
    text = open(path).read()
    groups = {}
    for def_line, record in entries:
        after, reopen = insertion(text, def_line)
        if after is None:
            raise RuntimeError(f"{path}:{def_line}: no record definition found")
        if "XOFFSET_TEST_ALLOCATOR_CTORS" in definition_text(text, def_line):
            record = "#if !XOFFSET_TEST_ALLOCATOR_CTORS\n" + record + "\n#endif"
        groups.setdefault((after, reopen), []).append((def_line, record))
    lines = text.split("\n")
    for (after, reopen), recs in sorted(groups.items(), key=lambda kv: -kv[0][0]):
        block = ["} // namespace " + (label or "(unnamed)") for label in reversed(reopen)]
        block += [r for _, r in sorted(recs)]
        block += ["namespace " + (label + " " if label else "") + "{" for label in reopen]
        lines[after:after] = block
    open(path, "w").write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--compile-commands", required=True)
    ap.add_argument("--cxx20", default="g++-13")
    ap.add_argument("--p2996", default="/opt/p2996-toolchain")
    ap.add_argument("--rounds", type=int, default=8)
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--report")
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    units = translation_units(args.compile_commands, set(args.files))
    work = tempfile.mkdtemp(prefix="describe_tests-")
    problems = {}
    written = 0
    for rnd in range(1, args.rounds + 1):
        with cf.ThreadPoolExecutor(args.jobs) as ex:
            found = list(ex.map(lambda u: (u, missing_records(args.cxx20, *u)), units))
        todo = [(u, [n for n in names if (u[0], n) not in problems]) for u, names in found]
        todo = [(u, names) for u, names in todo if names]
        print(f"round {rnd}: {sum(len(n) for _, n in todo)} missing records in {len(todo)} translation units", flush=True)
        if not todo:
            break
        inserts = {}   # file -> {(def_line, record)}
        for (path, flags), names in todo:
            usable = []
            for n in names:
                if "()::" in n or n.startswith("std::"):
                    problems[(path, n)] = "local class or std type: no XOFFSET_RECORD line possible"
                else:
                    usable.append(n)
            spelled = [n.replace("{anonymous}::", "") for n in usable]
            if not spelled:
                continue
            rows, err = describe(args.p2996, path, flags, spelled, work)
            if rows is None:
                for n in usable:
                    problems[(path, n)] = "describe helper failed: " + first_error(err)
                continue
            for (n, (sp, file, lineno, record)) in zip(usable, rows):
                if not record.startswith("XOFFSET_RECORD"):
                    problems[(path, n)] = record or "no line printed"
                    continue
                inserts.setdefault(file, set()).add((lineno, as_field_list(record, sp)))
        if not inserts:
            break
        for file, entries in inserts.items():
            text = open(file).read()
            fresh = [(l, r) for l, r in entries if r not in text]
            if fresh:
                insert_lines(file, fresh)
                written += len(fresh)
    report = [f"{os.path.relpath(p, REPO)}: {n}: {why}" for (p, n), why in sorted(problems.items())]
    print(f"{written} lines written; {len(report)} records left alone")
    for r in report:
        print("  " + r)
    if args.report:
        open(args.report, "w").write("\n".join(report) + "\n")


if __name__ == "__main__":
    sys.exit(main())
