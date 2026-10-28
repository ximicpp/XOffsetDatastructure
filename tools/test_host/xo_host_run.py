#!/usr/bin/env python3
"""Runs one program on the test host (host_core.hpp) and returns like the program did.

CTest uses it as the emulator of the iOS builds with -DXOFFSET_TEST_HOST=ON
(cmake/toolchains/ios-device.cmake, ios-simulator.cmake), and the phone scripts call it
directly:

    xo_host_run.py --backend device|sim|local [--device ID] [--app PATH] [--bundle-id ID]
                   [--timeout S] [--state DIR] PROGRAM [ARGS...]

PROGRAM is a program path of the build (e.g. <build>/bin/Release/test_vector) or a
builtin (@info, @net HOST PORT SECONDS, @sleep S); the host runs lib xoh_<basename>.
As in adb_run.sh, an argument that is an absolute path on this computer is mapped
into the app's data directory: an existing file or directory is sent first (again
only when its content changed), a path that does not exist yet is taken as an output
and copied back afterwards. The path after an output option (--output-flags:
--csv-dir, --write, --out) is always an output; when it is an existing directory
the run's files are added to it. XOFFSET_*, MAPDEMO_* and SLG_* environment variables are
passed on. The program's stdout and stderr come back as this script's stdout.

Backends (the app's data directory is <container>/Documents/xo):
  device  an iPhone, through xcrun devicectl (install, copy to/from, launch); files
          travel as one tar each way because devicectl copies single files only
  sim     an iOS simulator, through xcrun simctl; the container is a local directory
  local   the command-line host (tools/test_host/host_cli.cpp) on this computer, with
          the data directory under --state: checks the same flow without a phone

Serving (--serve 1 or XO_HOST_SERVE=1; not yet the default: on an iPhone every program
library loaded into one process keeps its 64 MiB table, and about 50 of them fill the
process's address space): the app is started once ("xoserve") and
stays up; each run sends inbox/<id>.tar and then inbox/<id>.ready, and the app runs the
programs one after another in its process (it shows the totals meanwhile). A state stamp
keeps the serving process id; when it is gone (a crash, a reinstall, a timeout) the next
run starts the app again. --fresh (XO_HOST_FRESH=1) starts it again before this run (timed
runs and memory probes: a process that ran nothing else). --serve 0 starts the app for each
run ("xorun").

Exit status: the program's exit code; 128+N when signal N ended it; 125 when the run
left no status (killed by the system, e.g. out of memory, or timed out: the output
says which); 124 for a problem of this script or the device connection.
"""
import argparse
import fcntl
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid

FORWARD_PREFIXES = ("XOFFSET_", "MAPDEMO_", "SLG_")
APP_ROOT = "Documents/xo"


def log(msg):
    sys.stderr.write("xo_host_run: %s\n" % msg)
    sys.stderr.flush()


def tree_digest(path):
    h = hashlib.sha256()
    if os.path.isfile(path):
        h.update(b"f")
        with open(path, "rb") as f:
            h.update(f.read())
        return h.hexdigest()
    for base, dirs, files in os.walk(path):
        dirs.sort()
        for name in sorted(files):
            p = os.path.join(base, name)
            h.update(os.path.relpath(p, path).encode() + b"\0")
            with open(p, "rb") as f:
                h.update(hashlib.sha256(f.read()).digest())
    return h.hexdigest()


def hexs(s):
    return s.encode("utf-8", "surrogateescape").hex()


class Backend:
    def __init__(self, a):
        self.a = a
        self.pid = None

    def ensure_installed(self):
        pass


class LocalBackend(Backend):
    """Command-line host; data directory <state>/root."""

    def __init__(self, a):
        super().__init__(a)
        self.root = os.path.join(a.state, "root")
        os.makedirs(os.path.join(self.root, "inbox"), exist_ok=True)

    def push(self, local, rel):
        dst = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(local, dst)

    def pull(self, rel, local):
        src = os.path.join(self.root, rel)
        if not os.path.exists(src):
            return False
        shutil.copyfile(src, local)
        return True

    def launch(self, args):
        if args[0] != "xoserve":
            # synchronous: the host process ends when the run ends
            self.proc = subprocess.run([self.a.app, "--root", self.root] + args)
            self.pid = None
            return True
        # the serving host outlives this script; it must not inherit the variables that
        # requests pass on, and its own output (between runs) goes to a log
        env = {k: v for k, v in os.environ.items() if not k.startswith(FORWARD_PREFIXES)}
        with open(os.path.join(self.a.state, "serve.log"), "ab") as logf:
            proc = subprocess.Popen([self.a.app, "--root", self.root, "xoserve"], stdin=subprocess.DEVNULL,
                                    stdout=logf, stderr=logf, env=env, start_new_session=True)
        self.pid = proc.pid
        return True

    def alive(self):
        return bool(self.pid) and pid_alive(self.pid, "xoserve")

    def stop(self):
        if self.pid and pid_alive(self.pid, "xoserve"):
            os.kill(self.pid, 9)


class SimBackend(Backend):
    def __init__(self, a):
        super().__init__(a)
        self.dev = a.device or "booted"
        self.container = None

    def ensure_installed(self):
        stamp_key = "install-sim-%s" % self.dev
        if self.a.bundle_digest != read_stamp(self.a.state, stamp_key):
            r = subprocess.run(["xcrun", "simctl", "install", self.dev, self.a.app], capture_output=True, text=True)
            if r.returncode != 0:
                raise RuntimeError("simctl install failed: " + r.stderr.strip())
            write_stamp(self.a.state, stamp_key, self.a.bundle_digest)
            self.container = None
            clear_input_stamps(self.a.state)
        if self.container is None:
            r = subprocess.run(["xcrun", "simctl", "get_app_container", self.dev, self.a.bundle_id, "data"],
                               capture_output=True, text=True)
            if r.returncode != 0:
                raise RuntimeError("simctl get_app_container failed: " + r.stderr.strip())
            self.container = r.stdout.strip()

    def push(self, local, rel):
        dst = os.path.join(self.container, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(local, dst)

    def pull(self, rel, local):
        src = os.path.join(self.container, rel)
        if not os.path.exists(src):
            return False
        shutil.copyfile(src, local)
        return True

    def launch(self, args):
        r = subprocess.run(["xcrun", "simctl", "launch", "--terminate-running-process", self.dev, self.a.bundle_id] + args,
                           capture_output=True, text=True)
        if r.returncode != 0:
            log("simctl launch failed: " + r.stderr.strip())
            return False
        # "<bundle id>: <pid>"; simulator apps are processes of this Mac
        try:
            self.pid = int(r.stdout.strip().rsplit(":", 1)[1])
        except (IndexError, ValueError):
            self.pid = None
        return True

    def alive(self):
        if not self.pid:
            return True
        return pid_alive(self.pid, "")

    def stop(self):
        pass   # the next launch terminates the running app


class DeviceBackend(Backend):
    def __init__(self, a):
        super().__init__(a)
        if not a.device:
            raise RuntimeError("--device (or XO_HOST_DEVICE) is required for the device backend")
        self.dev = a.device
        self.pid = None

    def devicectl(self, args, timeout=300):
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as t:
            out = t.name
        try:
            r = subprocess.run(["xcrun", "devicectl"] + args + ["--quiet", "--json-output", out],
                               capture_output=True, text=True, timeout=timeout)
            try:
                with open(out) as f:
                    data = json.load(f)
            except (OSError, ValueError):
                data = {}
            return r.returncode, data, r.stderr.strip()
        finally:
            os.unlink(out)

    def ensure_installed(self):
        # The caller installs the signed app (phone scripts); --install does it here.
        stamp_key = "install-device-%s" % self.dev
        if self.a.install and self.a.bundle_digest != read_stamp(self.a.state, stamp_key):
            rc, _, err = self.devicectl(["device", "install", "app", "--device", self.dev, self.a.app], timeout=900)
            if rc != 0:
                raise RuntimeError("devicectl install failed: " + err)
            write_stamp(self.a.state, stamp_key, self.a.bundle_digest)
            clear_input_stamps(self.a.state)

    def copy_args(self):
        return ["--device", self.dev, "--domain-type", "appDataContainer", "--domain-identifier", self.a.bundle_id]

    def push(self, local, rel):
        rc, _, err = self.devicectl(["device", "copy", "to"] + self.copy_args() + ["--source", local, "--destination", rel],
                                    timeout=900)
        if rc != 0:
            raise RuntimeError("devicectl copy to %s failed: %s" % (rel, err))

    def pull(self, rel, local):
        rc, _, _ = self.devicectl(["device", "copy", "from"] + self.copy_args() + ["--source", rel, "--destination", local],
                                  timeout=900)
        return rc == 0 and os.path.exists(local)

    def launch(self, args):
        rc, data, err = self.devicectl(["device", "process", "launch", "--device", self.dev, "--terminate-existing",
                                        self.a.bundle_id] + args, timeout=120)
        if rc != 0:
            log("devicectl launch failed: " + err)
            return False
        self.pid = (data.get("result", {}).get("process", {}) or {}).get("processIdentifier")
        return True

    def alive(self):
        if not self.pid:
            return True
        rc, data, _ = self.devicectl(["device", "info", "processes", "--device", self.dev], timeout=60)
        if rc != 0:
            return True
        procs = data.get("result", {}).get("runningProcesses", [])
        return any(p.get("processIdentifier") == self.pid for p in procs)

    def stop(self):
        pass   # the next launch terminates the running app


def pid_alive(pid, marker):
    """A process of this computer with this id runs (not a zombie) and its command line
    contains marker (a reused id belongs to some other program)."""
    try:
        os.waitpid(pid, os.WNOHANG)   # reaps it when it is a child of this script
    except ChildProcessError:
        pass
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        pass
    r = subprocess.run(["ps", "-o", "stat=", "-o", "command=", "-p", str(pid)], capture_output=True, text=True)
    line = r.stdout.strip()
    if r.returncode != 0 or not line or line.startswith("Z"):
        return False
    return marker in line


def read_stamp(state, key):
    try:
        with open(os.path.join(state, "stamps", key)) as f:
            return f.read().strip()
    except OSError:
        return None


def write_stamp(state, key, value):
    os.makedirs(os.path.join(state, "stamps"), exist_ok=True)
    with open(os.path.join(state, "stamps", key), "w") as f:
        f.write(value or "")


def clear_input_stamps(state):
    """After an install the app's data directory may be new: send every input again (and
    the app that served is gone)."""
    d = os.path.join(state, "stamps")
    for name in os.listdir(d) if os.path.isdir(d) else []:
        if name.startswith(("input-", "serve-")):
            os.unlink(os.path.join(d, name))


def ensure_serving(a, backend):
    """Starts the serving app unless the one this state directory started still runs."""
    key = "serve-%s-%s" % (a.backend, a.device or "")
    stamp = (read_stamp(a.state, key) or "").split()
    pid = int(stamp[0]) if stamp and stamp[0].isdigit() else None
    same_app = len(stamp) > 1 and stamp[1] == a.bundle_digest
    if pid and same_app and not a.fresh:
        backend.pid = pid
        if backend.alive():
            return True
    elif pid:
        backend.pid = pid
        backend.stop()   # device, sim: the launch below terminates it
    write_stamp(a.state, key, "")
    if not backend.launch(["xoserve"]):
        return False
    write_stamp(a.state, key, "%s %s" % (backend.pid or "", a.bundle_digest))
    return True


def forget_serving(a):
    write_stamp(a.state, "serve-%s-%s" % (a.backend, a.device or ""), "")


def bundle_digest(app):
    """Changes whenever the app bundle (or the command-line host's libraries) changes."""
    h = hashlib.sha256()
    paths = [app]
    if os.path.isfile(app):
        paths.append(os.path.join(os.path.dirname(app), "hosted"))
    for p in paths:
        if os.path.isfile(p):
            st = os.stat(p)
            h.update(("%s %d %d\n" % (p, st.st_size, st.st_mtime_ns)).encode())
        for base, dirs, files in os.walk(p):
            dirs.sort()
            for name in sorted(files):
                q = os.path.join(base, name)
                st = os.stat(q)
                h.update(("%s %d %d\n" % (os.path.relpath(q, p), st.st_size, st.st_mtime_ns)).encode())
    return h.hexdigest()


def run(a):
    prog = a.program
    name = prog if prog.startswith("@") else os.path.basename(prog)
    if name.endswith(".exe"):
        name = name[:-4]
    if not a.app:
        guess = os.path.join(os.path.dirname(os.path.abspath(prog)), "xoffset_test_host")
        a.app = guess + ".app" if a.backend != "local" else guess
    a.app = os.path.abspath(a.app)
    if not os.path.exists(a.app):
        log("no test host at %s (build the target xoffset_test_host_bundle)" % a.app)
        return 124
    a.bundle_digest = bundle_digest(a.app)
    backend = {"local": LocalBackend, "sim": SimBackend, "device": DeviceBackend}[a.backend](a)
    prefix = "" if a.backend == "local" else APP_ROOT + "/"

    lines = ["prog " + hexs(name)]
    tar_buf = io.BytesIO()
    tar = tarfile.open(fileobj=tar_buf, mode="w", format=tarfile.USTAR_FORMAT)
    outputs = []
    merge = set()   # output directories that exist here: the run's files are added to them
    output_flags = set(f for f in a.output_flags.split(",") if f)
    prev = None
    for arg in a.args:
        flagged = prev in output_flags
        prev = arg
        if os.path.isabs(arg) and len(arg) > 1:
            p = os.path.normpath(arg)
            if flagged and os.path.isdir(p):
                outputs.append(p)
                merge.add(p)
                lines.append("outdir " + hexs(p))
            elif os.path.exists(p) and not flagged:
                digest = tree_digest(p)
                key = "input-%s-%s" % (a.backend, hashlib.sha256(p.encode()).hexdigest()[:24])
                if digest != read_stamp(a.state, key):
                    lines.append("clear " + hexs(p))
                    tar.add(p, arcname="fs" + p, recursive=True)
                    a.sent_stamps.append((key, digest))
            else:
                outputs.append(p)
                lines.append("out " + hexs(p))
            lines.append("arg " + hexs("@XOFS@" + p))
        else:
            lines.append("arg " + hexs(arg))
    for k in sorted(os.environ):
        if k.startswith(FORWARD_PREFIXES):
            lines.append("env " + hexs("%s=%s" % (k, os.environ[k])))
    req = ("\n".join(lines) + "\n").encode()
    info = tarfile.TarInfo("request")
    info.size = len(req)
    info.mtime = int(time.time())
    tar.addfile(info, io.BytesIO(req))
    tar.close()

    run_id = uuid.uuid4().hex[:16]
    work = tempfile.mkdtemp(prefix="xo-host-run-")
    try:
        backend.ensure_installed()
        inbox = os.path.join(work, "in.tar")
        with open(inbox, "wb") as f:
            f.write(tar_buf.getvalue())
        backend.push(inbox, "%sinbox/%s.tar" % (prefix, run_id))
        result = os.path.join(work, "out.tar")
        rel_out = "%soutbox/%s.tar" % (prefix, run_id)
        if a.serve:
            if not ensure_serving(a, backend):
                return 124
            ready = os.path.join(work, "ready")
            with open(ready, "wb") as f:
                f.write(b"1\n")   # devicectl failed on some empty files (iPhone run 6, CoreDeviceError 7000)
            backend.push(ready, "%sinbox/%s.ready" % (prefix, run_id))
        elif not backend.launch(["xorun", run_id]):
            return 124
        deadline = time.time() + a.timeout
        got = backend.pull(rel_out, result)
        why = reason = None
        dead_checks = 0
        while not got:
            if time.time() > deadline:
                why, reason = "timeout after %d s" % a.timeout, "timeout"
                break
            time.sleep(a.poll)
            got = backend.pull(rel_out, result)
            if not got and not backend.alive():
                # gone without a result: give the file system a moment, then pack what it left
                dead_checks += 1
                if dead_checks >= 3:
                    why, reason = "the app ended without a result (killed by the system?)", "killed"
                    break
        if not got:
            log(why + "; packing what the run left")
            if a.serve:
                backend.stop()
                forget_serving(a)
            if not backend.launch(["xopack", run_id, reason]):
                return 124
            end = time.time() + 120
            while not got and time.time() < end:
                time.sleep(a.poll)
                got = backend.pull(rel_out, result)
            if not got:
                log("no result from xopack either")
                return 124
        for key, digest in a.sent_stamps:
            write_stamp(a.state, key, digest)
        with tarfile.open(result) as t:
            members = t.getmembers()
            status = t.extractfile("status").read().decode(errors="replace") if "status" in t.getnames() else "none"
            out = t.extractfile("out.txt").read() if "out.txt" in t.getnames() else b""
            for p in outputs:
                if p not in merge and os.path.lexists(p):
                    shutil.rmtree(p) if os.path.isdir(p) and not os.path.islink(p) else os.unlink(p)
            for m in members:
                if not m.name.startswith("fs/"):
                    continue
                dest = "/" + m.name[3:]
                if not any(dest == p or dest.startswith(p.rstrip("/") + "/") for p in outputs):
                    continue
                if m.isdir():
                    os.makedirs(dest, exist_ok=True)
                elif m.isfile():
                    os.makedirs(os.path.dirname(dest), exist_ok=True)
                    with open(dest, "wb") as f:
                        f.write(t.extractfile(m).read())
        sys.stdout.buffer.write(out)
        sys.stdout.flush()
        first = status.split("\n", 1)[0].split()
        if a.status_file:
            with open(a.status_file, "w") as f:
                f.write(status)
        if len(first) >= 2 and first[0] == "exit" and first[1].lstrip("-").isdigit():
            return int(first[1]) & 0xff
        if len(first) >= 2 and first[0] == "signal" and first[1].isdigit():
            log("ended by signal %s" % first[1])
            return 128 + int(first[1])
        log("run left no exit status (%s)" % status.split("\n", 1)[0].strip())
        return 125
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--backend", choices=("device", "sim", "local"), default=os.environ.get("XO_HOST_BACKEND", "local"))
    p.add_argument("--device", default=os.environ.get("XO_HOST_DEVICE"))
    p.add_argument("--app", default=os.environ.get("XO_HOST_APP"), help="the .app (device, sim) or the host program (local)")
    p.add_argument("--bundle-id", default=os.environ.get("XO_HOST_BUNDLE_ID", "com.ximicpp.xoffset.testhost"))
    p.add_argument("--install", action="store_true", default=os.environ.get("XO_HOST_INSTALL", "") == "1",
                   help="device: install --app when it changed (default: the caller installed the signed app)")
    p.add_argument("--timeout", type=int, default=int(os.environ.get("XO_HOST_TIMEOUT", "1500")))
    p.add_argument("--poll", type=float, default=float(os.environ.get("XO_HOST_POLL", "0")),
                   help="seconds between two looks for the result (default 1 on a phone, 0.1 otherwise)")
    p.add_argument("--state", default=os.environ.get("XO_HOST_STATE", os.path.join(tempfile.gettempdir(), "xo-host-state")))
    p.add_argument("--status-file", help="also write the run's status lines here")
    p.add_argument("--serve", type=int, choices=(0, 1), default=int(os.environ.get("XO_HOST_SERVE", "0")),
                   help="1: run in the app that stays up (started when needed); 0 (default): start the app for each run")
    p.add_argument("--output-flags", default=os.environ.get("XO_HOST_OUTPUT_FLAGS", "--csv-dir,--write,--out"),
                   help="options whose path argument is an output even when it exists (a directory there gets "
                        "the run's files added)")
    p.add_argument("--fresh", action="store_true", default=os.environ.get("XO_HOST_FRESH", "") == "1",
                   help="serving: start the app again before this run")
    p.add_argument("program")
    p.add_argument("args", nargs=argparse.REMAINDER)
    a = p.parse_args()
    a.sent_stamps = []
    if a.poll <= 0:
        a.poll = 1.0 if a.backend == "device" else 0.1
    os.makedirs(a.state, exist_ok=True)
    # one run at a time per state directory (the app runs one program at a time)
    with open(os.path.join(a.state, "lock"), "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        try:
            return run(a)
        except (RuntimeError, OSError, subprocess.SubprocessError) as e:
            log(str(e))
            return 124


if __name__ == "__main__":
    sys.exit(main())
