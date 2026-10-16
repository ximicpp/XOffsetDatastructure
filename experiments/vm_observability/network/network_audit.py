"""Bounded W1 prepared-v2 network diagnostics; never a timing-cost auditor.

The caller binds binary/source/stdout/output identities and compares all wire
bytes.  This module verifies the accepted workload's observations and lifetimes.
The adjacent frozen structural auditor/catalog are mandatory, hash checked input.
"""

from collections import Counter
import hashlib
from pathlib import Path


AUDITOR_SHA256 = "91ac07d1c54f306f965235b156a7ff95f18040c8eb65660434654342a626ee08"
CATALOG_SHA256 = "7bc7b0a77d24a8557a601a793daead2690406603fed26d78931cd3b942bb2e2c"
NOMINAL_BYTES = (4096, 65536, 1048576, 16777216)
H = 88
C = 32 * 1024 * 1024
HARD_EVENTS_PER_THREAD = 2048
UINT64_MAX = (1 << 64) - 1
STATS_FIELDS = {"cached_blocks", "cached_reserved_bytes", "cached_accessible_bytes", "hits", "misses"}
ENDPOINT_FIELDS = {"read", "written", "allocations", "waits", "published", "terminal_count",
                   "rx_size", "rx_capacity", "rx_init_extent", "header_copy_calls", "header_copy_bytes"}
RUN_RAW_FIELDS = {"schema", "diagnostic", "timing_evidence", "joined", "command", "pool",
                  "elements", "cycles", "before_final", "after_final", "page_size", "logical_ledger"}
CYCLE_FIELDS = {"cycle", "input_bytes", "output_bytes", "server", "client", "server_saved_size", "server_saved_capacity",
                "client_output_capacity", "server_strings_released"}
NO_RUN_KINDS = {"auto_growth", "cache_flush", "core_erase_movement_estimate", "core_new",
                "core_return", "core_return_failed", "core_reuse", "core_scan", "extent_request",
                "grow_zero", "copy_bytes_string_extent", "relocate_trivial_copy",
                "string_relocate_copy", "vector_erase_shift", "vector_insert_shift",
                "vector_insert_staged", "vector_relocated_element", "vector_relocation_attempt",
                "wire_return", "wire_reuse", "wire_scan", "wire_transfer_attempt",
                "save_header_append_extent", "save_payload_append_extent"}
FAILURE_KINDS = {"overflow_control", "vm_injected_failure", "expand_capacity_failure",
                 "expand_rejected", "registry_lookup_miss", "registry_register_failed"}


class NetworkAuditError(ValueError):
    """Malformed evidence or an accepted workload prediction was contradicted."""


def _require(ok, message):
    if not ok:
        raise NetworkAuditError(message)


def _uint(value, name):
    _require(type(value) is int and 0 <= value <= UINT64_MAX, f"{name}: expected uint64 integer")
    return value


def _obj(value, name, fields=None):
    _require(type(value) is dict, f"{name}: expected object")
    if fields is not None:
        _require(set(value) == fields, f"{name}: fields differ")
    return value


def _equal(actual, expected, name):
    _require(type(actual) is type(expected) and actual == expected,
             f"{name}: expected {expected!r}, observed {actual!r}")


def _frozen_summary(trace, catalog_path, thread_max):
    path = Path(catalog_path)
    auditor_path = path.parent / "audit_events.py"
    try:
        catalog_bytes = path.read_bytes()
        auditor_bytes = auditor_path.read_bytes()
    except OSError as error:
        raise NetworkAuditError(f"frozen foundation unavailable: {error}") from error
    _equal(hashlib.sha256(catalog_bytes).hexdigest(), CATALOG_SHA256, "frozen catalog sha256")
    _equal(hashlib.sha256(auditor_bytes).hexdigest(), AUDITOR_SHA256, "frozen auditor sha256")
    namespace = {"__file__": str(auditor_path.resolve()), "__name__": "_frozen_n3_network_auditor"}
    exec(compile(auditor_bytes, str(auditor_path), "exec"), namespace)
    try:
        summary = namespace["validate_trace"](trace, event_kinds_path=path)
    except namespace["TraceValidationError"] as error:
        raise NetworkAuditError(f"structural trace: {error}") from error
    _require(1 <= summary["thread_count"] <= thread_max, "trace: workload thread budget exceeded")
    counts = Counter(e["thread"] for e in trace["events"])
    _require(max(counts.values(), default=0) <= HARD_EVENTS_PER_THREAD,
             "trace: hard per-thread 2048 event budget exceeded")
    for event in trace["events"]:
        kind = event["kind"]
        _require(kind not in FAILURE_KINDS and not kind.endswith("_failed"),
                 f"trace: successful workload contains failure/control kind {kind}")
    _equal(summary["registry"]["active_count"], 0, "registry active at end")
    return summary


def _events(trace, kind=None, phase=None, domain=None):
    return [e for e in trace["events"] if (kind is None or e["kind"] == kind)
            and (phase is None or e["phase"] == phase)
            and (domain is None or e["value"] == domain)]


def _count(trace, kind, count, *, phase=None, byte_each=None):
    items = _events(trace, kind, phase)
    label = f"{phase or 'total'}/{kind}"
    _equal(len(items), count, label + " count")
    if byte_each is not None:
        for e in items:
            _equal(e["bytes"], byte_each, label + " bytes")
    return items


def _retained(summary, phase):
    outstanding = summary["allocations"]["outstanding"]
    _equal(len(outstanding), 1, "retained allocation count")
    item = outstanding[0]
    _equal(item["domain"], 2, "retained allocation domain")
    _equal(item["bytes"], 16, "retained registry bucket requested bytes")
    _equal(item["allocation_phase"], phase, "retained registry bucket original phase")
    _require(item["subject"] != 0 and item["origin_owner"] != 0,
             "retained bucket original allocation identity is absent")
    for domain in (1, 3, 4, 5):
        d = summary["allocations"]["domains"][str(domain)]
        _equal(d["current_live_requested_bytes"], 0, f"domain{domain} end requested bytes")
        _equal(d["current_live_allocations"], 0, f"domain{domain} end allocations")
    return item


def _live_generation(summary, event):
    matches = [g for g in summary["registry"]["generations"]
               if g["base"] == event["owner"] and g["begin_sequence"] <= event["sequence"]
               and event["sequence"] < g["end_sequence"]]
    _equal(len(matches), 1, f"event {event['sequence']}: active arena generation")
    return matches[0]


def _load(trace, summary, phase, payload_bytes):
    """Associate construction prefix with its following registration, not raw base reuse."""
    reg = _count(trace, "registry_register", 1, phase=phase)[0]
    generation = next(g for g in summary["registry"]["generations"] if g["generation"] == reg["sequence"])
    header = _count(trace, "load_header_copy", 1, phase=phase, byte_each=H)[0]
    copy = _count(trace, "load_payload_copy", 1, phase=phase, byte_each=payload_bytes)[0]
    _equal(header["owner"], 0, phase + " unadmitted header owner")
    _equal(copy["owner"], reg["subject"], phase + " construction-prefix base")
    _equal(copy["subject"], reg["subject"], phase + " payload destination")
    _equal(header["thread"], reg["thread"], phase + " header thread")
    _equal(copy["thread"], reg["thread"], phase + " payload thread")
    _require(header["sequence"] < copy["sequence"] < reg["sequence"],
             phase + ": header/payload/register construction-prefix order differs")
    # The exact phase has one registration. Check every candidate explicitly so
    # a reused base can never inherit the preceding request's generation.
    candidates = [e for e in _events(trace, "registry_register", phase)
                  if e["thread"] == copy["thread"] and e["subject"] == copy["owner"]
                  and e["sequence"] > copy["sequence"]]
    _equal(len(candidates), 1, phase + " unique following construction-prefix generation")
    domain5 = [e for e in _events(trace, phase=phase)
               if e["kind"] in ("host_allocate", "host_deallocate") and e["value"] == 5]
    alloc = [e for e in domain5 if e["kind"] == "host_allocate"]
    free = [e for e in domain5 if e["kind"] == "host_deallocate"]
    _equal(len(alloc), 4, phase + " domain5 allocations")
    _equal(len(free), 4, phase + " domain5 deallocations")
    _equal([e["bytes"] for e in alloc], [16, 32, 64, 128], phase + " domain5 libc++ geometry")
    _equal(sum(e["bytes"] for e in free), 240, phase + " domain5 freed requested bytes")
    live, peak = 0, 0
    for e in domain5:
        _equal(e["owner"], reg["subject"], phase + " domain5 arena base")
        _equal(e["thread"], reg["thread"], phase + " domain5 thread")
        _equal(_live_generation(summary, e)["generation"], reg["sequence"], phase + " domain5 generation")
        live += e["bytes"] if e["kind"] == "host_allocate" else -e["bytes"]
        peak = max(peak, live)
    _equal(live, 0, phase + " domain5 phase end requested bytes")
    _equal(peak, 192, phase + " domain5 local load peak requested bytes")
    for kind in ("os_reserve", "os_enable", "pool_scan"):
        for e in _events(trace, kind, phase):
            _equal(e["thread"], reg["thread"], phase + f" {kind} prefix thread")
            _equal(e["subject"], reg["subject"], phase + f" {kind} prefix mapping base")
            _require(e["sequence"] < copy["sequence"], phase + f" {kind} must precede payload copy")
    return {"phase": phase, "thread": reg["thread"], "base": reg["subject"],
            "generation": reg["sequence"], "end_sequence": generation["end_sequence"],
            "prefix_header_sequence": header["sequence"], "prefix_payload_sequence": copy["sequence"],
            "domain5_local_peak_requested_bytes": peak,
            "validation_range_count_basis": "frozen schema/source expects 5 owned and 0 free ranges; no runtime event counts ranges; four allocations (1/2/4/8 capacities) alone cannot distinguish 5..8 owned ranges"}


def _mapping_lifetimes(trace):
    live, lifetimes, registered = {}, [], set()
    for e in trace["events"]:
        kind, base, size = e["kind"], e["subject"], e["bytes"]
        if kind == "os_reserve":
            _require(base not in live, "mapping: duplicate live reservation base")
            interval = {"base": base, "reserved_bytes": size, "reserve_sequence": e["sequence"],
                        "reserve_phase": e["phase"], "unmap_sequence": None, "registrations": []}
            live[base] = interval
            lifetimes.append(interval)
        elif kind == "os_enable":
            matches = [m for m in live.values() if m["base"] <= base and
                       base + size <= m["base"] + m["reserved_bytes"]]
            _equal(len(matches), 1, "mapping: enabled range must belong to one live reservation")
        elif kind == "registry_register":
            _require(base in live, "mapping: registration without live mapping")
            live[base]["registrations"].append(e["sequence"])
            registered.add(base)
        elif kind == "registry_erase":
            registered.remove(base)  # structural audit already paired this erase
        elif kind == "os_unmap":
            _require(base in live, "mapping: unmap without live reservation")
            _require(base not in registered, "mapping: unmap while arena owner remains registered")
            m = live.pop(base)
            _equal(size, m["reserved_bytes"], "mapping: unmap original reserved extent")
            m["unmap_sequence"] = e["sequence"]
    _equal(len(live), 0, "mapping reservations at end")
    return lifetimes


def _stdout(obj, command, nominal):
    _obj(obj, "stdout")
    for key, value in {"schema_version": 1, "contract_version": "1.0.0", "status": "passed",
                       "command": command, "diagnostic": True, "timing_evidence": False,
                       "element_count": nominal // 4}.items():
        _require(key in obj, "stdout: missing " + key)
        _equal(obj[key], value, "stdout." + key)
    checksum = obj.get("semantic_checksum")
    _require(type(checksum) is str and checksum.isascii() and checksum.isdecimal(),
             "stdout.semantic_checksum: expected decimal uint64 string")
    _require(0 <= int(checksum) <= UINT64_MAX, "stdout.semantic_checksum: out of range")


def _endpoint(obj, read, written, name):
    _obj(obj, name, ENDPOINT_FIELDS)
    for key in ENDPOINT_FIELDS - {"published"}:
        _uint(obj[key], name + "." + key)
    for key, expected in {"read": read, "written": written, "allocations": 1, "published": True,
                          "terminal_count": 1, "rx_size": read, "rx_init_extent": read,
                          "header_copy_calls": 1, "header_copy_bytes": H}.items():
        _equal(obj[key], expected, name + "." + key)
    _require(obj["rx_capacity"] >= read, name + ": capacity smaller than live receive string")


def _stats(obj, name, *, cached, reserved, accessible, hits, misses):
    _obj(obj, name, STATS_FIELDS)
    for key in STATS_FIELDS:
        _uint(obj[key], name + "." + key)
    for key, value in {"cached_blocks": cached, "cached_reserved_bytes": reserved,
                       "cached_accessible_bytes": accessible, "hits": hits, "misses": misses}.items():
        _equal(obj[key], value, name + "." + key)


def audit_case(trace, raw_observation, w1_stdout_obj, *, kind_catalog_path, nominal_bytes, pool):
    """Audit exactly two work exchanges and the mandatory warm exchange, if any.

    Returns scoped counts, copy extents, arena generations and separate mapping
    lifetimes.  No clock field is read or converted to a performance conclusion.
    Raises NetworkAuditError on every rejected observation.
    """
    _require(type(nominal_bytes) is int and nominal_bytes in NOMINAL_BYTES, "unsupported nominal bytes")
    _require(type(pool) is str and pool in ("none", "cold", "warm"), "unsupported pool mode")
    summary = _frozen_summary(trace, kind_catalog_path, 2)
    _stdout(w1_stdout_obj, "run", nominal_bytes)
    stdout = w1_stdout_obj
    for key, value in {"pool": pool, "iterations": 2, "max_wire_bytes": C,
                       "owner_capacity_bytes": C, "max_inflight_bytes": 4*C,
                       "reserved_inflight_peak_bytes": 4*C, "connection_count": 1,
                       "max_inflight_requests": 1, "timeout_ms": 30000,
                       "pool_measured_hits": 4 if pool == "warm" else 0,
                       "pool_measured_misses": 4 if pool == "cold" else 0,
                       "pool_warmup_hits": 0, "pool_warmup_misses": 2 if pool == "warm" else 0,
                       "output_files": ["output-0.wire", "output-1.wire"]}.items():
        _equal(stdout.get(key), value, "stdout." + key)
    Wi = _uint(stdout.get("input_bytes_per_iteration"), "stdout.input_bytes_per_iteration")
    Wo = _uint(stdout.get("output_bytes_per_iteration"), "stdout.output_bytes_per_iteration")
    _require(H < Wi <= C and H < Wo <= C and Wi == Wo, "wire input/output geometry differs")
    Pi, Po = Wi-H, Wo-H
    _require(Pi >= nominal_bytes and Pi < C, "payload does not fit accepted nominal/capacity bounds")
    _require(type(stdout.get("samples")) is list and len(stdout["samples"]) == 2,
             "stdout.samples: exactly two work records required")
    for i, sample in enumerate(stdout["samples"]):
        _obj(sample, f"stdout.samples[{i}]")
        _equal(sample.get("input_bytes"), Wi, f"stdout.samples[{i}].input_bytes")
        _equal(sample.get("output_bytes"), Wo, f"stdout.samples[{i}].output_bytes")
    raw = _obj(raw_observation, "raw", RUN_RAW_FIELDS)
    for key, value in {"schema": "n3-network-raw-v1", "diagnostic": True, "timing_evidence": False,
                       "joined": True, "command": "run", "pool": pool, "elements": nominal_bytes//4}.items():
        _equal(raw[key], value, "raw." + key)
    page = _uint(raw["page_size"], "raw.page_size")
    _require(page >= 4096 and page & (page-1) == 0, "raw.page_size: unsupported VM page geometry")
    rounded = lambda size: ((size + page - 1) // page) * page
    ledger = _obj(raw["logical_ledger"], "raw.logical_ledger",
                  {"inflight_bytes", "queue_frames", "peak_inflight_bytes"})
    for key, expected in {"inflight_bytes": 0, "queue_frames": 0, "peak_inflight_bytes": 4*C}.items():
        _equal(ledger[key], expected, "raw.logical_ledger." + key)
    cycles = (["warm"] if pool == "warm" else []) + ["work.0", "work.1"]
    _require(type(raw["cycles"]) is list, "raw.cycles: expected array")
    _equal(len(raw["cycles"]), len(cycles), "raw.cycles count (warm retained)")
    for record, cycle in zip(raw["cycles"], cycles):
        _obj(record, "raw cycle " + cycle, CYCLE_FIELDS)
        _equal(record["cycle"], cycle, "raw cycle identity/order")
        _equal(record["input_bytes"], Wi, cycle + " input bytes")
        _equal(record["output_bytes"], Wo, cycle + " output bytes")
        _endpoint(record["server"], Wi, Wo, cycle + ".server")
        _endpoint(record["client"], Wo, Wi, cycle + ".client")
        _equal(record["server_saved_size"], Wo, cycle + " server saved size")
        for key in ("server_saved_capacity", "client_output_capacity"):
            _require(_uint(record[key], cycle+"."+key) >= Wo, cycle+"."+key+": capacity below size")
        _equal(record["server_strings_released"], True, cycle + " server string lifetime completion")
    for checkpoint in ("before_final", "after_final"):
        _obj(raw[checkpoint], "raw."+checkpoint, {"server", "client"})
        for endpoint, payload in (("server", Pi), ("client", Po)):
            retained = checkpoint == "before_final" and pool != "none"
            _stats(raw[checkpoint][endpoint], f"raw.{checkpoint}.{endpoint}",
                   cached=1 if retained else 0, reserved=rounded(C) if retained else 0,
                   accessible=rounded(payload) if retained else 0,
                   hits=2 if pool == "warm" else 0,
                   misses=(1 if pool == "warm" else 2) if pool != "none" else 0)
    for field in ("cached_blocks", "cached_reserved_bytes", "cached_accessible_bytes"):
        _equal(stdout.get("pool_"+field+"_before_final_clear"),
               sum(raw["before_final"][ep][field] for ep in ("server", "client")),
               "stdout/raw before-final pool " + field)
    allowed_phases = {"init.input", "init.preflight", "init.pool.server", "init.pool.client", "init.connection",
                      "init.pool.stats", "cold.clear.0", "cold.clear.1", "warm.output_archive", "warm.output_release",
                      "work.0.output_archive", "work.0.output_release", "work.1.output_archive", "work.1.output_release",
                      "before_final.stats", "final.clear", "diag.pool.check", "request.release", "run.teardown"}
    for cycle in cycles:
        allowed_phases.update(f"server.{cycle}.{p}" for p in ("receive", "load", "process", "save", "release", "send", "strings_release"))
        allowed_phases.update(f"client.{cycle}.{p}" for p in ("send", "receive", "load", "validate", "release", "server_wait"))
    for event in trace["events"]:
        _require(event["phase"] in allowed_phases, "unexpected/missing network phase " + event["phase"])
        _require(event["kind"] not in NO_RUN_KINDS and not event["kind"].startswith("expand_"),
                 "unexpected normal-run allocation/migration kind " + event["kind"])
    E = len(cycles)
    _count(trace, "registry_register", 1+2*E)
    _count(trace, "registry_erase", 1+2*E)
    _equal(summary["registry"]["peak_active_count"], 1, "run peak simultaneously registered owners")
    loads = [_load(trace, summary, "init.preflight", Pi)]
    preflight_erase = _count(trace, "registry_erase", 1, phase="init.preflight")[0]
    _equal(preflight_erase["sequence"], loads[0]["end_sequence"], "preflight erase generation")
    _count(trace, "string_assign_copy", 1, phase="init.preflight", byte_each=21)
    for cycle in cycles:
        s = _load(trace, summary, f"server.{cycle}.load", Pi)
        c = _load(trace, summary, f"client.{cycle}.load", Po)
        _require(s["thread"] != c["thread"], "server/client recorder threads must differ")
        _equal(c["thread"], loads[0]["thread"], "client/preflight main thread")
        _require(loads[-1]["end_sequence"] < s["generation"] < s["end_sequence"] < c["generation"],
                 cycle + ": owner/cycle completion ordering differs")
        for endpoint, load in (("server", s), ("client", c)):
            erase = _count(trace, "registry_erase", 1, phase=f"{endpoint}.{cycle}.release")[0]
            _equal(erase["sequence"], load["end_sequence"], cycle + " erase generation")
            _equal(erase["thread"], load["thread"], cycle + " erase thread")
        _count(trace, "string_assign_copy", 1, phase=f"server.{cycle}.process", byte_each=21)
        for kind, expected in (("save_header_copy", H), ("save_payload_copy", Po)):
            event = _count(trace, kind, 1, phase=f"server.{cycle}.save", byte_each=expected)[0]
            _equal(_live_generation(summary, event)["generation"], s["generation"], cycle+" save owner generation")
        loads.extend((s, c))
    for kind, count in (("load_header_copy", 1+2*E), ("load_payload_copy", 1+2*E),
                        ("save_header_copy", E), ("save_payload_copy", E), ("string_assign_copy", 1+E)):
        _count(trace, kind, count)
    for event in _events(trace, "string_assign_copy"):
        gen = _live_generation(summary, event)
        expected_phase = "init.preflight" if gen["generation"] == loads[0]["generation"] else next(
            (item["phase"].replace(".load", ".process") for item in loads[1::2]
             if item["generation"] == gen["generation"]), None)
        _equal(event["phase"], expected_phase, "string assignment arena generation/phase")
    d5 = summary["allocations"]["domains"]["5"]
    for key, value in {"allocation_calls": 4*(1+2*E), "deallocation_calls": 4*(1+2*E),
                       "allocated_requested_bytes": 240*(1+2*E), "deallocated_requested_bytes": 240*(1+2*E),
                       "peak_live_requested_bytes": 192}.items():
        _equal(d5[key], value, "domain5 synchronous-chain " + key)
    _equal(summary["allocations"]["domains"]["1"]["allocation_calls"], 0,
           "normal-run domain1 Core-vector allocation calls")
    for domain in (3, 4):
        _equal(summary["allocations"]["domains"][str(domain)]["allocation_calls"],
               0 if pool == "none" else 2, f"domain{domain} pool allocation calls")
        if pool != "none":
            for endpoint in ("server", "client"):
                _equal(len(_events(trace, "host_allocate", "init.pool."+endpoint, domain)), 1,
                       f"domain{domain} original {endpoint} pool-init allocation")
    scans = _count(trace, "pool_scan", 4 if pool == "warm" else 0, byte_each=C)
    _require(all(e["phase"] in {f"{ep}.work.{i}.load" for ep in ("server", "client") for i in (0, 1)}
                 for e in scans), "pool scan outside warm-mode work loads")
    expected_maps = 3 if pool == "warm" else 5
    for kind in ("os_reserve", "os_enable", "os_unmap"):
        _count(trace, kind, expected_maps, byte_each=rounded(C) if kind != "os_enable" else rounded(Pi))
    for kind in ("os_advise", "os_protect", "os_reclaim", "os_replace"):
        _count(trace, kind, 0)
    mappings = _mapping_lifetimes(trace)
    retained = _retained(summary, "init.preflight")
    _equal(retained["origin_owner"], loads[0]["base"], "retained bucket original causal preflight base")
    _equal(retained["origin_thread"], loads[0]["thread"], "retained bucket original causal preflight thread")
    _require(loads[0]["prefix_payload_sequence"] < retained["allocation_sequence"] < loads[0]["generation"],
             "retained bucket must originate in first preflight registration's construction prefix")
    return {"schema": "n3-network-audit-v1", "diagnostic": True, "timing_evidence": False,
            "nominal_bytes": nominal_bytes, "pool": pool, "work_exchanges": 2,
            "warm_exchanges": int(pool == "warm"), "wire_input_bytes": Wi, "wire_output_bytes": Wo,
            "payload_input_bytes": Pi, "payload_output_bytes": Po, "loads": loads,
            "mapping_lifetimes": mappings, "retained_registry_bucket": retained,
            "trace_summary": summary, "coverage": "bounded explicit network/arena observations; no RSS, malloc, kernel copy or latency-saving fraction"}


def audit_fixture(trace, stdout_obj, *, kind_catalog_path, nominal_bytes):
    """Audit separate original preparation/readback semantics, not request costs."""
    _require(type(nominal_bytes) is int and nominal_bytes in NOMINAL_BYTES, "unsupported nominal bytes")
    summary = _frozen_summary(trace, kind_catalog_path, 1)
    _stdout(stdout_obj, "fixture", nominal_bytes)
    frame = _uint(stdout_obj.get("frame_bytes"), "stdout.frame_bytes")
    _require(frame > H + nominal_bytes, "fixture frame geometry")
    allowed = {"fixture.build", "fixture.reserve", "fixture.save", "fixture.readback_verify", "fixture.teardown"}
    _require(all(e["phase"] in allowed for e in trace["events"]), "unexpected/missing fixture phase")
    _count(trace, "registry_register", 2)
    _count(trace, "registry_erase", 2)
    _equal(summary["registry"]["peak_active_count"], 2, "fixture peak simultaneously registered owners")
    _count(trace, "auto_growth", 1, phase="fixture.reserve")
    _count(trace, "extent_request", 2, phase="fixture.reserve")
    _count(trace, "cache_flush", 0)
    _count(trace, "wire_transfer_attempt", 0)
    _count(trace, "save_header_copy", 1, phase="fixture.save", byte_each=H)
    _count(trace, "save_payload_copy", 1, phase="fixture.save", byte_each=frame-H)
    _count(trace, "load_header_copy", 1)
    _count(trace, "load_payload_copy", 1)
    loaded = _load(trace, summary, "fixture.readback_verify", frame-H)
    retained = _retained(summary, "fixture.build")
    first_register = _count(trace, "registry_register", 1, phase="fixture.build")[0]
    first_generation = summary["registry"]["generations"][0]
    _equal(first_generation["begin_sequence"], first_register["sequence"], "fixture first build registration")
    _equal(retained["origin_owner"], first_generation["base"], "fixture retained bucket original causal base")
    _equal(retained["origin_thread"], 0, "fixture retained bucket original thread")
    _require(retained["allocation_sequence"] < first_generation["begin_sequence"],
             "fixture retained bucket must originate before first registration")
    mappings = _mapping_lifetimes(trace)
    return {"schema": "n3-network-fixture-audit-v1", "diagnostic": True, "timing_evidence": False,
            "nominal_bytes": nominal_bytes, "frame_bytes": frame, "verified_readback_load": loaded,
            "retained_registry_bucket": retained, "mapping_lifetimes": mappings, "trace_summary": summary,
            "coverage": "separate fixture preparation; not per-request network cost"}
