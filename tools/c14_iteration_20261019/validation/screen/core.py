"""Strict, portable evidence primitives; importing this module executes nothing."""
from __future__ import annotations

import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shutil
import stat


class ProtocolError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ProtocolError(message)


def integer(value, minimum=0, maximum=2**63-1):
    return type(value) is int and minimum <= value <= maximum


def decimal(value, positive=False, signed=False):
    pattern = r"(?:0|[1-9][0-9]*)" if not signed else r"(?:0|-?[1-9][0-9]*)"
    require(type(value) is str and re.fullmatch(pattern, value) is not None, "noncanonical decimal string")
    result = int(value)
    require(-(2**63) <= result <= 2**64-1 if signed else 0 <= result <= 2**64-1, "decimal outside integer range")
    require(not positive or result > 0, "decimal must be positive")
    return result


def hexsha(value):
    return type(value) is str and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def exact(value, keys, label):
    require(type(value) is dict and set(value) == set(keys), "unexpected exact fields: " + label)
    return value


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key: " + key)
        result[key] = value
    return result


def _constant(value):
    raise ProtocolError("nonfinite JSON token: " + value)


def finite_tree(value):
    if type(value) is float:
        require(math.isfinite(value), "nonfinite JSON number")
    elif type(value) is list:
        for item in value:
            finite_tree(item)
    elif type(value) is dict:
        for item in value.values():
            finite_tree(item)


def parse(raw):
    require(type(raw) in (str, bytes), "JSON must be text/bytes")
    try:
        text = raw.decode("utf-8", errors="strict") if type(raw) is bytes else raw
        value = json.loads(text, object_pairs_hook=_pairs, parse_constant=_constant)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise ProtocolError("invalid UTF-8 JSON: " + str(error)) from error
    finite_tree(value)
    return value


def read(path):
    return parse(Path(path).read_bytes())


def encoded(value):
    finite_tree(value)
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2, allow_nan=False) + "\n").encode("utf-8")


def same(left,right):
    """JSON equality that does not confuse bool/int or float/int."""
    return encoded(left)==encoded(right)


def write(path, value):
    with Path(path).open("xb") as target:
        target.write(encoded(value))


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024*1024), b""):
            digest.update(block)
    return digest.hexdigest()


def file_id(path):
    path = Path(path)
    require(path.is_file(), "missing regular file: " + str(path))
    return dict(bytes=str(path.stat().st_size), sha256=sha(path))


def validate_id(value):
    exact(value, ("bytes", "sha256"), "file identity")
    decimal(value["bytes"])
    require(hexsha(value["sha256"]), "invalid SHA-256")
    return value


def absolute_string(value):
    return type(value) is str and bool(value) and (PurePosixPath(value).is_absolute() or PureWindowsPath(value).is_absolute())


def absolute(path):
    path = Path(path)
    require(path.is_absolute(), "execution path must be absolute: " + str(path))
    return path.resolve()


def relative(value):
    require(type(value) is str and value and "\\" not in value and ":" not in value, "invalid relative path")
    path = PurePosixPath(value)
    require(not path.is_absolute() and path.as_posix() == value and all(p not in (".", "..") for p in path.parts), "unsafe relative path")
    return path


def fresh(path):
    path = absolute(path)
    require(not path.exists(), "output already exists: " + str(path))
    path.mkdir(parents=True, exist_ok=False)
    return path


def inventory(directory, exclude=("manifest.json",), exclude_roots=()):
    directory = Path(directory)
    result = {}
    for path in sorted(directory.rglob("*")):
        name = path.relative_to(directory).as_posix()
        if name in exclude or path.relative_to(directory).parts[0] in exclude_roots:
            continue
        mode = path.lstat().st_mode
        require(stat.S_ISDIR(mode) or stat.S_ISREG(mode), "bundle contains link/special file: " + name)
        if stat.S_ISREG(mode):
            result[name] = file_id(path)
    return result


def seal(directory, status):
    write(Path(directory) / "status.json", status)
    write(Path(directory) / "manifest.json", dict(schema_version=1, files=inventory(directory, exclude_roots=("queue",))))


def check_inventory(directory, expected_sha=None):
    directory = Path(directory)
    if expected_sha is not None:
        require(hexsha(expected_sha) and sha(directory / "manifest.json") == expected_sha, "external manifest pin differs")
    manifest = exact(read(directory / "manifest.json"), ("schema_version", "files"), "manifest")
    require(type(manifest["schema_version"]) is int and manifest["schema_version"] == 1, "unsupported manifest schema")
    require(type(manifest["files"]) is dict, "invalid inventory")
    for name, identity in manifest["files"].items():
        relative(name)
        validate_id(identity)
    require(manifest["files"] == inventory(directory, exclude_roots=("queue",)), "bundle members/bytes changed")
    return manifest


def copy_file(source, target):
    source, target = Path(source), Path(target)
    require(not target.exists(), "copy target exists")
    target.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as src, target.open("xb") as dst:
        shutil.copyfileobj(src, dst)
    require(file_id(source) == file_id(target), "source changed during copy")
    return file_id(target)


def tree_id(root):
    """Bind empty directories, symlink target spelling and its resolved bytes."""
    root = Path(root)
    require(root.is_absolute(),"closure path must be absolute")
    records = {}

    def visit(path, logical, active):
        link=path.is_symlink()
        try:
            resolved = path.resolve(strict=True)
        except FileNotFoundError as error:
            # A tracked source symlink may intentionally reference an optional
            # external build system. Bind that state; never silently omit it.
            require(link,"missing ordinary closure path: "+str(path))
            try:
                unresolved_target=path.resolve(strict=False)
            except (RuntimeError,OSError) as cycle:
                raise ProtocolError("unresolvable closure link/path: "+str(path)) from cycle
            require(not unresolved_target.exists(),"closure target appeared during identity capture")
            records[logical+"@link"]=os.readlink(path)
            records[logical+"@unavailable"]={"kind":"missing-symlink-target","resolved_target":str(unresolved_target)}
            return
        except (RuntimeError,OSError) as error:
            raise ProtocolError("unresolvable closure link/path: "+str(path)) from error
        if link:
            require(resolved not in active, "closure symlink cycle")
            records[logical + "@link"] = os.readlink(path)
        if resolved.is_dir():
            require(resolved not in active, "closure directory cycle")
            records[logical + "/"] = "directory"
            for child in sorted(resolved.iterdir()):
                visit(child, logical + "/" + child.name, active | {resolved})
        else:
            require(resolved.is_file(), "unsupported closure node")
            records[logical] = file_id(resolved)

    visit(root, ".", set())
    return records
