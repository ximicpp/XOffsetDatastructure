"""Strict offline audit for the D018 fixed-request owner-scaling workload.

This validates records and the accepted W2 diagnostic observations for the fixed
native Apple arm64 build profile: effective uint64 allocator alignment is 8,
AllocationHeader is 16 bytes, and successive one-element blocks advance 24
bytes.  These are frozen profile expectations, never inferred permissively from
the trace being audited.  This module does not certify build provenance or
authorize timing.  Synthetic records are never native execution evidence.  No
C++ process is started by this module.
"""

from collections import Counter
import hashlib
import importlib.util
from pathlib import Path
import re


UINT64_MAX = (1 << 64) - 1
CAPACITY = 65536
EFFECTIVE_ALIGNMENT = 8
INITIAL_USED_END = 48
BLOCK_STRIDE = 24
FIRST_USER_OFFSET = 64
FIRST_BLOCK_END = 72
SEED = 14695981039346656037
VALUE_XOR = 0x195A926A5CC24873
FNV_PRIME = 1099511628211
CONDITIONS = frozenset(((8, 1), (64, 1), (256, 1), (256, 2), (256, 4), (256, 8)))
CONTROLS = frozenset(("none", "partial_startup", "fail_create", "fail_allocate", "fail_verify"))
PHASES = ("create", "allocate", "free", "destroy")
AUDITOR_SHA256 = "91ac07d1c54f306f965235b156a7ff95f18040c8eb65660434654342a626ee08"
CATALOG_SHA256 = "7bc7b0a77d24a8557a601a793daead2690406603fed26d78931cd3b942bb2e2c"
BUCKET_COUNTS = (2, 5, 11, 23, 47, 97, 197, 397)
BUCKET_TRIGGERS = (1, 3, 6, 12, 24, 48, 98, 198)
_FOUNDATION = None


class AuditError(ValueError):
    """A schema, semantic, event, lifetime, or resource contract mismatch."""


def _require(ok, message):
    if not ok:
        raise AuditError(message)


def _fields(value, expected, label):
    _require(type(value) is dict, f"{label}: expected object")
    _require(set(value) == set(expected), f"{label}: fields differ: {sorted(set(value) ^ set(expected))}")


def _uint(value, label):
    _require(type(value) is int and 0 <= value <= UINT64_MAX, f"{label}: expected uint64")
    return value


def _decimal(value, label):
    _require(type(value) is str and re.fullmatch(r"0|[1-9][0-9]*", value) is not None,
             f"{label}: expected canonical uint64 decimal string")
    return _uint(int(value), label)


def _checksum(count):
    value = SEED
    for k in range(count):
        value = ((value ^ (VALUE_XOR ^ k)) * FNV_PRIME) & UINT64_MAX
    return str(value)


def _foundation():
    global _FOUNDATION
    if _FOUNDATION is not None:
        # validate_trace reads its catalog on each call; never let a changed
        # adjacent catalog bypass the frozen identity check through our cache.
        directory = _FOUNDATION[1].parent
        for name, digest in (("audit_events.py", AUDITOR_SHA256), ("event_kinds.json", CATALOG_SHA256)):
            path = directory / name
            _require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == digest,
                     f"accepted W2 {name}: SHA256 mismatch or missing")
        return _FOUNDATION
    here = Path(__file__).resolve()
    choices = [here.parent / "w2"]
    choices += [parent / "docs/validation/vm-observability/frozen-implementation-v1/source"
                for parent in here.parents]
    directory = next((p for p in choices if (p / "audit_events.py").is_file()), None)
    _require(directory is not None, "accepted W2 auditor unavailable (expected adjacent w2/ or accepted repository snapshot)")
    for name, digest in (("audit_events.py", AUDITOR_SHA256), ("event_kinds.json", CATALOG_SHA256)):
        path = directory / name
        _require(path.is_file(), f"accepted W2 {name}: missing")
        _require(hashlib.sha256(path.read_bytes()).hexdigest() == digest,
                 f"accepted W2 {name}: SHA256 mismatch")
    spec = importlib.util.spec_from_file_location("_owner_scaling_accepted_w2", directory / "audit_events.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _FOUNDATION = (module, directory / "event_kinds.json")
    return _FOUNDATION


def _expected_progress(c, t, r, control):
    if control == "partial_startup":
        return dict(threads_started=2, threads_joined=2, created=0, allocated=0,
                    freed=0, destroyed=0, validated=0)
    made = 3 if control == "fail_create" else c
    allocated = 0 if control == "fail_create" else 7 if control == "fail_allocate" else r
    validated = r if control == "none" else 7 if control == "fail_verify" else 0
    return dict(threads_started=t, threads_joined=t, created=made, allocated=allocated,
                freed=allocated, destroyed=made, validated=validated)


def _expected_condition(value):
    if type(value) in (list, tuple):
        _require(len(value) == 3, "expected_condition: expected (owners,threads,requests)")
        return tuple(value)
    _require(type(value) is dict and {"owners", "threads", "requests"} <= set(value)
             and set(value) <= {"owners", "threads", "requests", "per_owner"},
             "expected_condition: expected tuple or condition object")
    return tuple(value[k] for k in ("owners", "threads", "requests"))


def _audit_record_fields(record, expected_profile, expected_condition, expected_control):
    _fields(record, ("schema_version", "profile", "timing_evidence", "condition", "control", "status",
                     "reason", "counts", "checksum", "phases", "owners", "validation", "trace"), "record")
    _require(type(record["schema_version"]) is int and record["schema_version"] == 1, "record.schema_version: expected 1")
    profile = record["profile"]
    _require(profile in ("timing", "diagnostic"), "record.profile: unsupported profile")
    _require(expected_profile is None or profile == expected_profile, "record.profile: differs from expected profile")
    _require(record["timing_evidence"] is False, "record.timing_evidence: bounded functionality must be false")
    _fields(record["condition"], ("owners", "threads", "requests", "per_owner"), "condition")
    c, t, r, q = (_uint(record["condition"][k], f"condition.{k}")
                  for k in ("owners", "threads", "requests", "per_owner"))
    _require((c, t) in CONDITIONS, "condition: outside six frozen conditions")
    _require(r in (256, 8192) and r % c == 0 and q == r // c, "condition: fixed total requests/per_owner mismatch")
    _require(profile != "diagnostic" or r == 256, "condition: diagnostic requires exactly 256 total requests")
    _require(expected_condition is None or (c, t, r) == _expected_condition(expected_condition),
             "condition: differs from expected condition")
    control = record["control"]
    _require(type(control) is str and control in CONTROLS, "record.control: unsupported control")
    _require(expected_control in CONTROLS and control == expected_control, "record.control: differs from expected control")
    if control != "none":
        _require(r == 256 and (c, t) == ((256, 4) if control == "partial_startup" else (8, 1)),
                 "control: wrong frozen condition")
    _require(record["status"] == ("ok" if control == "none" else "expected_failure"), "record.status: expected exact outcome")
    _require(record["reason"] == (None if control == "none" else "control:" + control), "record.reason: expected exact control cause")
    progress = _expected_progress(c, t, r, control)
    _fields(record["counts"], progress, "counts")
    for key, expected in progress.items():
        _require(_uint(record["counts"][key], "counts." + key) == expected, f"counts.{key}: expected {expected}")
    _decimal(record["checksum"], "checksum")
    _require(record["checksum"] == _checksum(progress["validated"]), "checksum: differs from actual verified prefix")
    _fields(record["validation"], ("values", "alignment", "ranges", "nonoverlap", "conservation"), "validation")
    for key, value in record["validation"].items():
        expected = control == "none" if key == "values" else True
        _require(type(value) is bool and value is expected, f"validation.{key}: expected {expected}")
    _fields(record["phases"], PHASES, "phases")
    for name, phase in record["phases"].items():
        _fields(phase, ("wall_ns",), "phases." + name)
        value = phase["wall_ns"]
        if profile == "diagnostic" or control == "partial_startup":
            _require(value is None, f"phases.{name}: clock must be null")
        else:
            _uint(value, f"phases.{name}.wall_ns")
    _require(type(record["owners"]) is list and len(record["owners"]) == c, "owners: expected complete fixed owner ledger")
    owners = []
    for i, item in enumerate(record["owners"]):
        label = f"owners[{i}]"
        _fields(item, ("index", "worker", "core", "base", "logical", "used_end", "free_before", "free_after",
                       "created", "allocated", "freed", "destroyed"), label)
        _require(_uint(item["index"], label + ".index") == i, label + ": owner index/order mismatch")
        _require(_uint(item["worker"], label + ".worker") == i % t, label + ": fixed worker ownership mismatch")
        made = i < progress["created"]
        allocated = (min(q, max(0, progress["allocated"] - i * q)) if t == 1 else q) if made else 0
        for key in ("created", "destroyed"):
            _require(type(item[key]) is bool and item[key] == made, label + f".{key}: expected {made}")
        for key in ("allocated", "freed"):
            _require(_uint(item[key], label + "." + key) == allocated, label + f".{key}: actual prefix mismatch")
        parsed = dict(item)
        for key in ("core", "base"):
            parsed[key] = _decimal(item[key], label + "." + key)
            _require((parsed[key] != 0) == made, label + f".{key}: created/address mismatch")
        used_end = INITIAL_USED_END + BLOCK_STRIDE * allocated
        expected = dict(logical=CAPACITY, used_end=used_end, free_before=CAPACITY - used_end,
                        free_after=CAPACITY - used_end + 24 * allocated) if made else dict.fromkeys(
                            ("logical", "used_end", "free_before", "free_after"), 0)
        for key, value in expected.items():
            _require(_uint(item[key], label + "." + key) == value, label + f".{key}: conservation/extent mismatch")
        if made:
            _require(parsed["base"] % EFFECTIVE_ALIGNMENT == 0 and parsed["base"] + CAPACITY <= UINT64_MAX,
                     label + ": arena address/alignment overflow")
        owners.append(parsed)
    active = [o for o in owners if o["created"]]
    _require(len({o["core"] for o in active}) == len(active), "owners: duplicate Core identity")
    bases = sorted(o["base"] for o in active)
    _require(all(a + CAPACITY <= b for a, b in zip(bases, bases[1:])), "owners: arena ranges overlap")
    return profile, c, t, r, q, control, progress, owners


class _Stream:
    def __init__(self, events, phase):
        self.events, self.phase, self.index = events, phase, 0

    def peek(self):
        return self.events[self.index] if self.index < len(self.events) else None

    def take(self, kind, **fields):
        event = self.peek()
        _require(event is not None and event["kind"] == kind, f"{self.phase}: expected {kind} at local event {self.index}")
        for key, expected in fields.items():
            _require(event[key] == expected, f"{self.phase}/{kind}.{key}: expected {expected}")
        self.index += 1
        return event

    def finish(self):
        _require(self.index == len(self.events), f"{self.phase}: unexpected/unconsumed events")


def _audit_diagnostic(trace, owners, c, t, r, control, progress):
    module, catalog = _foundation()
    try:
        summary = module.validate_trace(trace, event_kinds_path=catalog)
    except (ValueError, TypeError, KeyError) as error:
        raise AuditError("W2 structural audit: " + str(error)) from error
    events = trace["events"]
    _require(len(events) <= 5631, "trace: exceeds frozen hard process event budget")
    created = [o for o in owners if o["created"]]
    expected_threads = t if created else 0
    _require(trace["thread_count"] == expected_threads, "trace: recorder slots differ from actual emitting team")
    by_core = {o["core"]: o for o in created}
    lane_slot, slot_lane = {}, {}
    for event in events:
        if event["kind"] == "registry_register":
            _require(event["owner"] in by_core, "trace: registration for unknown Core")
            item = by_core[event["owner"]]
            _require(event["subject"] == item["base"], "trace: Core/base registration mismatch")
            lane, slot = item["worker"], event["thread"]
            _require(lane_slot.setdefault(lane, slot) == slot and slot_lane.setdefault(slot, lane) == lane,
                     "trace: worker/recorder slot ownership changed")
    _require(set(lane_slot) == {o["worker"] for o in created}, "trace: missing worker registration")
    allowed = {"os_reserve", "os_enable", "os_unmap", "grow_zero", "host_allocate", "host_deallocate",
               "registry_register", "registry_erase", "registry_lookup_hit", "registry_lock",
               "core_metadata_capacity", "core_new", "core_return"}
    phase_rank = -1
    for event in events:
        _require(event["kind"] in allowed, "trace: unexpected event kind " + event["kind"])
        _require(event["phase"] in PHASES, "trace: unexpected event phase " + event["phase"])
        rank = PHASES.index(event["phase"])
        _require(rank >= phase_rank, "trace: global phase order/barrier violated")
        phase_rank = rank
        _require(event["thread"] in slot_lane, "trace: emitting slot has no fixed owner lane")
    mutex = []
    nodes, vectors, bucket_changes = {}, {}, []

    def lock(stream, owner):
        event = stream.take("registry_lock", owner=owner)
        if not mutex:
            mutex.append(event["subject"])
        _require(event["subject"] == mutex[0] != 0, "registry_lock: expected one shared registry mutex")

    for lane in sorted(lane_slot):
        local = [o for o in created if o["worker"] == lane]
        slot_events = [e for e in events if e["thread"] == lane_slot[lane]]
        streams = {phase: _Stream([e for e in slot_events if e["phase"] == phase], phase) for phase in PHASES}
        stream = streams["create"]
        for item in local:
            base, core = item["base"], item["core"]
            stream.take("os_reserve", owner=0, subject=base, bytes=CAPACITY, value=0)
            stream.take("os_enable", owner=0, subject=base, bytes=CAPACITY, value=0)
            stream.take("grow_zero", owner=base, subject=base, bytes=CAPACITY, value=0)
            node = stream.take("host_allocate", owner=base, bytes=32, value=2)
            nodes[base] = node["subject"]
            change = []
            while stream.peek() and stream.peek()["kind"] in ("host_allocate", "host_deallocate"):
                event = stream.peek()
                change.append(stream.take(event["kind"], owner=base, value=2))
            registration = stream.take("registry_register", owner=core, subject=base, bytes=0, value=0)
            bucket_changes.append((registration["sequence"], change))
            stream.take("core_metadata_capacity", owner=base, subject=core, bytes=0, value=24)
            lock(stream, base)
        stream.finish()
        stream = streams["allocate"]
        for item in local:
            base, core = item["base"], item["core"]
            for j in range(item["allocated"]):
                stream.take("registry_lookup_hit", owner=core, subject=base, bytes=0, value=0)
                lock(stream, base)
                stream.take("core_new", owner=base, subject=base + FIRST_USER_OFFSET + BLOCK_STRIDE * j,
                            bytes=8, value=FIRST_BLOCK_END + BLOCK_STRIDE * j)
        stream.finish()
        stream = streams["free"]
        for item in local:
            base, core = item["base"], item["core"]
            capacity, pointer = 0, None
            for j in range(item["freed"]):
                stream.take("registry_lookup_hit", owner=core, subject=base, bytes=0, value=0)
                lock(stream, base)
                if j == capacity:
                    new_capacity = max(1, 2 * capacity)
                    fresh = stream.take("host_allocate", owner=base, bytes=16 * new_capacity, value=1)
                    if pointer is not None:
                        stream.take("host_deallocate", owner=base, subject=pointer, bytes=16 * capacity, value=1)
                    pointer, capacity = fresh["subject"], new_capacity
                stream.take("core_return", owner=base, subject=base + FIRST_USER_OFFSET + BLOCK_STRIDE * j,
                            bytes=BLOCK_STRIDE, value=j + 1)
                stream.take("core_metadata_capacity", owner=base, subject=core, bytes=16 * capacity, value=24)
            vectors[base] = (pointer, capacity)
        stream.finish()
        stream = streams["destroy"]
        for item in local:
            base, core = item["base"], item["core"]
            stream.take("host_deallocate", owner=base, subject=nodes[base], bytes=32, value=2)
            stream.take("registry_erase", owner=core, subject=base, bytes=0, value=1)
            lock(stream, base)
            pointer, capacity = vectors[base]
            if pointer is not None:
                stream.take("host_deallocate", owner=base, subject=pointer, bytes=16 * capacity, value=1)
            stream.take("os_unmap", owner=base, subject=base, bytes=CAPACITY, value=0)
        stream.finish()

    # Buckets are checked as one shared chronological lineage, inside their
    # source registration blocks, not classified merely by allocation size.
    bucket_pointer = None
    bucket_bytes = 0
    bucket_steps = 0
    for ordinal, (_, changes) in enumerate(sorted(bucket_changes), 1):
        if ordinal in BUCKET_TRIGGERS:
            wanted_bytes = BUCKET_COUNTS[BUCKET_TRIGGERS.index(ordinal)] * 8
            _require(len(changes) == (1 if bucket_pointer is None else 2), "registry bucket: missing/extra growth events")
            fresh = changes[0]
            _require(fresh["kind"] == "host_allocate" and fresh["bytes"] == wanted_bytes,
                     "registry bucket: wrong allocation lineage/extent")
            if bucket_pointer is not None:
                old = changes[1]
                _require(old["kind"] == "host_deallocate" and old["subject"] == bucket_pointer and old["bytes"] == bucket_bytes,
                         "registry bucket: old bucket lineage not released")
            bucket_pointer, bucket_bytes = fresh["subject"], wanted_bytes
            bucket_steps += 1
        else:
            _require(not changes, "registry bucket: unexpected rehash")
    expected_outstanding = [] if not created else [{
        "subject": bucket_pointer, "domain": 2, "bytes": bucket_bytes,
    }]
    outstanding = summary["allocations"]["outstanding"]
    _require(len(outstanding) == len(expected_outstanding), "endstate: missing/extra retained allocation")
    if expected_outstanding:
        for key, value in expected_outstanding[0].items():
            _require(outstanding[0][key] == value, "endstate: expected only final shared registry bucket")
        expected_bytes = 40 if control == "fail_create" else {8: 88, 64: 776, 256: 3176}[c]
        _require(bucket_bytes == expected_bytes, "endstate: final bucket size mismatch")
    registry = summary["registry"]
    _require(registry["active_count"] == 0 and registry["peak_active_count"] == len(created),
             "registry: active peak/endstate mismatch")
    _require(len(registry["generations"]) == len(created), "registry: missing/extra owner generations")
    for generation in registry["generations"]:
        _require(generation["end_sequence"] is not None and not generation["rebinds"],
                 "registry: generation not fully terminated")
    allocated = progress["allocated"]
    sum_g = sum((o["allocated"] - 1).bit_length() + 1 for o in created if o["allocated"])
    expected_events = 7 * allocated + 11 * len(created) + 2 * sum_g + max(0, 2 * bucket_steps - 1)
    _require(len(events) == expected_events, "trace: exact full-lifetime event count mismatch")
    if control == "none":
        g = (r // c).bit_length()
        process_bound = 7 * r + c * (13 + 2 * g) - 1
        worker_bound = (7 * r + c * (11 + 2 * g)) // t + 2 * c - 1
        _require(len(events) <= process_bound, "trace: condition process budget exceeded")
        _require(all(n <= worker_bound for n in Counter(e["thread"] for e in events).values()),
                 "trace: condition worker budget exceeded")
    return summary


def audit_record(record, *, expected_profile=None, expected_condition=None, expected_control="none"):
    """Return a JSON-safe audit summary; raise AuditError on any mismatch.

    expected_condition accepts (C,T,R), or an object containing owners, threads,
    requests (and optionally per_owner).  A control is accepted only when its
    exact name is explicitly supplied by the caller.
    """
    try:
        profile, c, t, r, q, control, progress, owners = _audit_record_fields(
            record, expected_profile, expected_condition, expected_control)
        trace_summary = None
        if profile == "diagnostic":
            trace_summary = _audit_diagnostic(record["trace"], owners, c, t, r, control, progress)
        else:
            _require(record["trace"] is None, "timing: diagnostic trace forbidden")
        return dict(schema="owner-scaling-audit-v1", status="passed", profile=profile,
                    timing_evidence=False, condition=dict(record["condition"]), control=control,
                    counts=dict(record["counts"]), checksum=record["checksum"], trace_summary=trace_summary)
    except AuditError:
        raise
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        raise AuditError("malformed owner-scaling record: " + str(error)) from error
