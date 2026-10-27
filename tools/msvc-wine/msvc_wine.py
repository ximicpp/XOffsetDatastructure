#!/usr/bin/env python3
"""Runs an MSVC tool (cl, link, lib) under wine for a Linux build.

Invoked through the symlinks cl, link and lib next to this file (CMake's
cmake/toolchains/msvc-wine-x86_64.cmake points at them). The MSVC tree is
$XOFFSET_MSVC_ROOT (default /opt/msvc), laid out as a Visual Studio
installation: VC/Tools/MSVC/<version> and Windows Kits/10; the newest version
of each is used.

Unix absolute paths in the arguments (also after /Fo, /I, /OUT: and similar
prefixes, and inside @response files) become Z:\\ paths; Z:\\ paths in the
tool's output become Unix paths again, so Ninja's /showIncludes parsing and the
diagnostics name files the Linux side can open.
"""
import glob, os, re, subprocess, sys, tempfile

ROOT = os.environ.get("XOFFSET_MSVC_ROOT", "/opt/msvc")
TOOL = os.path.basename(sys.argv[0])


def newest(pattern):
    found = sorted(glob.glob(pattern), key=lambda p: [int(x) if x.isdigit() else x
                                                      for x in re.split(r"(\d+)", os.path.basename(p))])
    if not found:
        sys.exit(f"msvc_wine: nothing matches {pattern}")
    return found[-1]


VC = newest(os.path.join(ROOT, "VC/Tools/MSVC/*"))
SDK_INC = newest(os.path.join(ROOT, "Windows Kits/10/Include/*"))
SDK_LIB = os.path.join(ROOT, "Windows Kits/10/Lib", os.path.basename(SDK_INC))
BIN = os.path.join(VC, "bin/Hostx64/x64")


def win(path):
    return "Z:" + path.replace("/", "\\")


PREFIXES = ("/Fo", "/Fd", "/Fe", "/Fp", "/Fa", "/Fi", "/FI", "/I", "-I", "/external:I",
            "/OUT:", "/out:", "/PDB:", "/pdb:", "/IMPLIB:", "/implib:", "/LIBPATH:", "/libpath:",
            "/DEF:", "/def:", "/MAP:", "/ILK:", "/MANIFESTFILE:")


def looks_like_path(arg):
    return arg.startswith("/") and arg.count("/") > 1 and (
        os.path.exists(arg) or os.path.isdir(os.path.dirname(arg)))


def translate(arg):
    if arg.startswith("@") and os.path.isfile(arg[1:]):
        return "@" + win(response(arg[1:]))
    if looks_like_path(arg):
        return win(arg)
    for prefix in PREFIXES:
        rest = arg[len(prefix):]
        if arg.startswith(prefix) and rest.startswith("/") and looks_like_path(rest):
            return prefix + win(rest)
    return arg


def response(path):
    text = open(path, encoding="utf-8", errors="surrogateescape").read()
    out = []
    for token in re.findall(r'"[^"]*"|\S+', text):
        quoted = token.startswith('"')
        body = translate(token.strip('"'))
        out.append(f'"{body}"' if quoted or " " in body else body)
    fd, name = tempfile.mkstemp(suffix=".rsp")
    with os.fdopen(fd, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(" ".join(out))
    return name


env = dict(os.environ)
env.setdefault("WINEDEBUG", "-all")
env["INCLUDE"] = ";".join(win(p) for p in (os.path.join(VC, "include"), os.path.join(SDK_INC, "ucrt"),
                                             os.path.join(SDK_INC, "um"), os.path.join(SDK_INC, "shared")))
env["LIB"] = ";".join(win(p) for p in (os.path.join(VC, "lib/x64"), os.path.join(SDK_LIB, "ucrt/x64"),
                                         os.path.join(SDK_LIB, "um/x64")))
env["WINEPATH"] = win(BIN)
exe = os.path.join(BIN, TOOL + ".exe")
args = [translate(a) for a in sys.argv[1:]]
# Output goes through a file, not a pipe: wine services started by the first
# tool run inherit the descriptors and would hold a pipe open.
with tempfile.TemporaryFile() as out:
    proc = subprocess.run(["wine", exe] + args, env=env, stdout=out, stderr=subprocess.STDOUT,
                          stdin=subprocess.DEVNULL)
    out.seek(0)
    raw = out.read()
zpath = re.compile(rb"[Zz]:((?:\\[^\\\r\n:*?\"<>|]+)+\\?)")
text = zpath.sub(lambda m: m.group(1).replace(b"\\", b"/"), raw).replace(b"\r\n", b"\n")
sys.stdout.buffer.write(text)
sys.exit(proc.returncode)
