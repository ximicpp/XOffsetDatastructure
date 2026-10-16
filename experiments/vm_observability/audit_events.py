"""Offline validation of N3 diagnostic event objects.

This module audits recorded observations, not source/binary provenance or case
semantics.  Its allocation totals cover the five instrumented allocator domains
and requested bytes only.  They exclude allocator overhead and process RSS.
"""

from collections import defaultdict
import json
from pathlib import Path


UINT64_MAX = (1 << 64) - 1
MAX_THREADS = 16
MAX_EVENTS_PER_THREAD = 16384
MAX_LABEL_UTF8_BYTES = 63
TRACE_FIELDS = frozenset(
    ("schema", "diagnostic", "timing_evidence", "valid", "dropped", "thread_count", "events")
)
EVENT_FIELDS = frozenset(
    ("sequence", "thread", "phase", "kind", "owner", "subject", "bytes", "value")
)
ALLOCATION_DOMAINS = {
    1: "Core free vector",
    2: "shared registry nodes/buckets",
    3: "pool cached slots",
    4: "pool state/control block",
    5: "wire-validation range vectors",
}
SHARED_CAUSAL_DOMAINS = frozenset((2, 3, 4))
ALLOCATION_KINDS = frozenset(("host_allocate", "host_deallocate", "host_allocate_failed"))
REGISTRY_KINDS = frozenset(
    (
        "registry_register",
        "registry_register_failed",
        "registry_erase",
        "registry_lookup_hit",
        "registry_lookup_miss",
        "registry_rebind",
        "registry_lock",
    )
)
DEFAULT_EVENT_KINDS_PATH = Path(__file__).resolve().with_name("event_kinds.json")


class TraceValidationError(ValueError):
    """A trace is malformed, incomplete, or contradicts its own observations."""


def _require(condition, message):
    if not condition:
        raise TraceValidationError(message)


def _uint(value, label):
    # bool is an int subclass, but is never a JSON integer in this contract.
    _require(type(value) is int and 0 <= value <= UINT64_MAX, f"{label}: expected uint64 integer")


def _exact_fields(obj, expected, label):
    _require(type(obj) is dict, f"{label}: expected JSON object")
    _require(
        set(obj) == expected,
        f"{label}: fields differ (missing={sorted(expected - set(obj))}, "
        f"unexpected={sorted(map(str, set(obj) - expected))})",
    )


def _load_kind_catalog(path):
    try:
        catalog = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, ValueError) as error:
        raise TraceValidationError(f"event catalog unavailable or invalid: {error}") from error
    _require(type(catalog) is dict, "event catalog: expected object")
    _require(catalog.get("schema") == "n3-event-kinds-v1", "event catalog: incorrect schema")
    kinds = catalog.get("kinds")
    _require(type(kinds) is dict and bool(kinds), "event catalog.kinds: expected nonempty object")
    for kind, rules in kinds.items():
        _require(type(kind) is str and bool(kind), "event catalog: invalid kind name")
        _require(type(rules) is dict, f"event catalog.{kind}: expected object")
        for field in ("owner", "subject"):
            _require(rules.get(field) in ("any", "nonzero", "zero"), f"event catalog.{kind}.{field}: unknown constraint")
        for field in ("bytes", "value"):
            metadata = rules.get(field)
            _require(
                type(metadata) is dict and metadata.get("type") == "uint64",
                f"event catalog.{kind}.{field}: expected uint64 metadata",
            )
            for suffix in ("equals", "min", "max"):
                constraint = f"{field}_{suffix}"
                if constraint in rules:
                    _uint(rules[constraint], f"event catalog.{kind}.{constraint}")
    return kinds


def _validate_kind(event, kinds, label):
    kind = event["kind"]
    _require(kind in kinds, f"{label}.kind: unknown event kind {kind}")
    rules = kinds[kind]
    for field in ("owner", "subject"):
        if rules[field] == "zero":
            _require(event[field] == 0, f"{label}.{field}: {kind} requires zero")
        elif rules[field] == "nonzero":
            _require(event[field] != 0, f"{label}.{field}: {kind} requires nonzero")
    for field in ("bytes", "value"):
        if f"{field}_equals" in rules:
            _require(event[field] == rules[f"{field}_equals"], f"{label}.{field}: violates {kind} exact value")
        if f"{field}_min" in rules:
            _require(event[field] >= rules[f"{field}_min"], f"{label}.{field}: below {kind} minimum")
        if f"{field}_max" in rules:
            _require(event[field] <= rules[f"{field}_max"], f"{label}.{field}: above {kind} maximum")


def _tally():
    return {"count": 0, "bytes": 0, "value": 0}


def _add_tally(tally, event):
    tally["count"] += 1
    tally["bytes"] += event["bytes"]
    tally["value"] += event["value"]


def _allocation_tally():
    return {
        "allocation_calls": 0,
        "allocated_requested_bytes": 0,
        "deallocation_calls": 0,
        "deallocated_requested_bytes": 0,
        "failed_allocation_calls": 0,
        "failed_requested_bytes": 0,
        "current_live_requested_bytes": 0,
        "peak_live_requested_bytes": 0,
        "current_live_allocations": 0,
        "peak_live_allocations": 0,
    }


def _apply_allocation(tally, kind, size):
    if kind == "host_allocate":
        tally["allocation_calls"] += 1
        tally["allocated_requested_bytes"] += size
        tally["current_live_requested_bytes"] += size
        tally["current_live_allocations"] += 1
        tally["peak_live_requested_bytes"] = max(
            tally["peak_live_requested_bytes"], tally["current_live_requested_bytes"]
        )
        tally["peak_live_allocations"] = max(
            tally["peak_live_allocations"], tally["current_live_allocations"]
        )
    elif kind == "host_deallocate":
        tally["deallocation_calls"] += 1
        tally["deallocated_requested_bytes"] += size
        tally["current_live_requested_bytes"] -= size
        tally["current_live_allocations"] -= 1
    else:
        tally["failed_allocation_calls"] += 1
        tally["failed_requested_bytes"] += size


def validate_trace(trace, *, event_kinds_path=None):
    """Validate a decoded ``n3-events-v1`` object and return a JSON-safe summary.

    Raises TraceValidationError explicitly (including under Python ``-O``).
    Sequences must be the complete global observation order 1..N. Allocation
    frees use the original allocation's owner/thread even when the current scope
    differs. A live allocation at trace end is reported rather than rejected:
    callers may collect before teardown, and registry buckets can be retained.

    Generic event byte/value sums are intentionally not combined across kinds:
    lock bytes/value are nanoseconds, whereas allocator bytes are requested bytes.
    Source identity, expected case counts, and end-of-case lifetime requirements
    remain the caller's responsibility.

    The event catalog defaults to the file alongside this module. A caller
    auditing a frozen artifact can explicitly select its frozen catalog path.
    """
    _exact_fields(trace, TRACE_FIELDS, "trace")
    _require(trace["schema"] == "n3-events-v1", "trace.schema: expected n3-events-v1")
    for key in ("diagnostic", "timing_evidence", "valid"):
        _require(type(trace[key]) is bool, f"trace.{key}: expected boolean")
    _require(trace["diagnostic"] is True, "trace.diagnostic: must be true")
    _require(trace["timing_evidence"] is False, "trace.timing_evidence: must be false")
    _uint(trace["dropped"], "trace.dropped")
    _uint(trace["thread_count"], "trace.thread_count")
    _require(trace["thread_count"] <= MAX_THREADS, "trace.thread_count: exceeds recorder lifetime thread limit")
    _require(trace["valid"] is True, "trace.valid: recorder invalidated; reject all interpretation")
    _require(trace["dropped"] == 0, "trace.dropped: incomplete recorder output")
    _require(type(trace["events"]) is list, "trace.events: expected JSON array")
    _require(
        len(trace["events"]) <= trace["thread_count"] * MAX_EVENTS_PER_THREAD,
        "trace.events: exceeds recorder storage capacity for thread_count",
    )
    kinds = _load_kind_catalog(DEFAULT_EVENT_KINDS_PATH if event_kinds_path is None else event_kinds_path)

    by_kind = defaultdict(_tally)
    grouped = {name: {} for name in ("by_owner", "by_thread", "by_phase")}
    allocations = _allocation_tally()
    domains = {domain: _allocation_tally() for domain in ALLOCATION_DOMAINS}
    origin_owners = {}
    origin_threads = {}
    live = {}
    registry = {}
    registry_generation_by_base = {}
    registry_generations = []
    registry_peak = 0
    thread_event_counts = defaultdict(int)

    for expected_sequence, event in enumerate(trace["events"], 1):
        label = f"event[{expected_sequence}]"
        _exact_fields(event, EVENT_FIELDS, label)
        for field in ("sequence", "thread", "owner", "subject", "bytes", "value"):
            _uint(event[field], f"{label}.{field}")
        _require(
            event["sequence"] == expected_sequence,
            f"{label}.sequence: expected complete global sequence {expected_sequence}",
        )
        _require(event["thread"] < trace["thread_count"], f"{label}.thread: outside thread_count")
        thread_event_counts[event["thread"]] += 1
        _require(
            thread_event_counts[event["thread"]] <= MAX_EVENTS_PER_THREAD,
            f"{label}.thread: exceeds per-thread recorder event limit",
        )
        for field in ("phase", "kind"):
            _require(
                type(event[field]) is str and bool(event[field]) and "\x00" not in event[field],
                f"{label}.{field}: expected nonempty string without NUL",
            )
            try:
                encoded_label = event[field].encode("utf-8")
            except UnicodeError as error:
                raise TraceValidationError(f"{label}.{field}: invalid UTF-8 label") from error
            _require(
                1 <= len(encoded_label) <= MAX_LABEL_UTF8_BYTES,
                f"{label}.{field}: exceeds recorder UTF-8 label byte limit",
            )

        kind = event["kind"]
        _validate_kind(event, kinds, label)
        owner, subject, size, value = (event[k] for k in ("owner", "subject", "bytes", "value"))

        _add_tally(by_kind[kind], event)
        for group, key in (("by_owner", owner), ("by_thread", event["thread"]), ("by_phase", event["phase"])):
            bucket = grouped[group].setdefault(key, {"event_count": 0, "by_kind": {}})
            bucket["event_count"] += 1
            _add_tally(bucket["by_kind"].setdefault(kind, _tally()), event)

        if kind in ALLOCATION_KINDS:
            _require(value in ALLOCATION_DOMAINS, f"{label}: allocation domain must be 1..5")
            origin_owner, origin_thread = owner, event["thread"]
            if kind == "host_allocate_failed":
                _require(subject == 0, f"{label}: failed allocation must have null subject")
            else:
                _require(subject != 0, f"{label}: allocation pointer must be nonzero")
                if kind == "host_allocate":
                    _require(subject not in live, f"{label}: duplicate live allocation pointer {subject}")
                    live[subject] = {
                        "subject": subject,
                        "domain": value,
                        "bytes": size,
                        "origin_owner": owner,
                        "origin_thread": event["thread"],
                        "allocation_sequence": expected_sequence,
                        "allocation_phase": event["phase"],
                    }
                else:
                    _require(subject in live, f"{label}: deallocation without matching live allocation {subject}")
                    allocation = live[subject]
                    _require(allocation["domain"] == value, f"{label}: deallocation domain differs from allocation")
                    _require(allocation["bytes"] == size, f"{label}: deallocation bytes differ from allocation")
                    origin_owner, origin_thread = allocation["origin_owner"], allocation["origin_thread"]
                    del live[subject]

            owner_stats = origin_owners.setdefault(origin_owner, {
                "all_domains": _allocation_tally(),
                "nonshared_domains": _allocation_tally(),
                "shared_metadata_causal": _allocation_tally(),
                "shared_registry_causal": _allocation_tally(),
                "shared_pool_causal": _allocation_tally(),
            })
            _apply_allocation(allocations, kind, size)
            _apply_allocation(domains[value], kind, size)
            _apply_allocation(owner_stats["all_domains"], kind, size)
            _apply_allocation(owner_stats["shared_metadata_causal" if value in SHARED_CAUSAL_DOMAINS else "nonshared_domains"], kind, size)
            if value in SHARED_CAUSAL_DOMAINS:
                _apply_allocation(owner_stats["shared_registry_causal" if value == 2 else "shared_pool_causal"], kind, size)
            _apply_allocation(origin_threads.setdefault(origin_thread, _allocation_tally()), kind, size)

        if kind in REGISTRY_KINDS:
            if kind == "registry_lock":
                _require(subject != 0, f"{label}: registry mutex subject must be nonzero")
                continue
            _require(subject != 0, f"{label}: registry base subject must be nonzero")
            _require(size == 0, f"{label}: registry operation bytes must be zero")
            _require(value == (1 if kind == "registry_erase" else 0), f"{label}: invalid registry operation value")
            if kind == "registry_lookup_miss":
                _require(owner == 0, f"{label}: registry miss owner must be zero")
                _require(subject not in registry, f"{label}: registry miss contradicts active registration")
            else:
                _require(owner != 0, f"{label}: registry C++ owner must be nonzero")
                if kind == "registry_register":
                    _require(subject not in registry, f"{label}: duplicate registry registration")
                    registry[subject] = owner
                    generation = {
                        "base": subject,
                        "generation": expected_sequence,
                        "begin_sequence": expected_sequence,
                        "end_sequence": None,
                        "initial_owner": owner,
                        "final_owner": owner,
                        "owner_intervals": [{
                            "owner": owner,
                            "begin_sequence": expected_sequence,
                            "end_sequence": None,
                        }],
                        "rebinds": [],
                    }
                    registry_generation_by_base[subject] = generation
                    registry_generations.append(generation)
                    registry_peak = max(registry_peak, len(registry))
                elif kind in ("registry_lookup_hit", "registry_erase", "registry_rebind"):
                    _require(subject in registry, f"{label}: registry operation without active registration")
                    if kind == "registry_rebind":
                        generation = registry_generation_by_base[subject]
                        generation["owner_intervals"][-1]["end_sequence"] = expected_sequence
                        generation["owner_intervals"].append({
                            "owner": owner,
                            "begin_sequence": expected_sequence,
                            "end_sequence": None,
                        })
                        generation["rebinds"].append({
                            "sequence": expected_sequence,
                            "from_owner": registry[subject],
                            "to_owner": owner,
                        })
                        generation["final_owner"] = owner
                        registry[subject] = owner
                    else:
                        _require(registry[subject] == owner, f"{label}: registry owner differs from active registration")
                        if kind == "registry_erase":
                            generation = registry_generation_by_base.pop(subject)
                            generation["end_sequence"] = expected_sequence
                            generation["owner_intervals"][-1]["end_sequence"] = expected_sequence
                            del registry[subject]
                # A failed registration has no effect on the active key map.

    def encoded(mapping):
        return {str(key): mapping[key] for key in sorted(mapping)}

    outstanding = [live[key] for key in sorted(live)]
    single_owner = [stats for owner, stats in origin_owners.items() if owner != 0]
    return {
        "schema": "n3-trace-summary-v1",
        "event_count": len(trace["events"]),
        "thread_count": trace["thread_count"],
        "by_kind": dict(sorted(by_kind.items())),
        **{name: encoded(mapping) for name, mapping in grouped.items()},
        "allocations": {
            **allocations,
            "domains": {
                str(domain): {"name": ALLOCATION_DOMAINS[domain], **domains[domain]}
                for domain in ALLOCATION_DOMAINS
            },
            "origin_owners": encoded(origin_owners),
            "origin_threads": encoded(origin_threads),
            "max_single_origin_owner_live_requested_bytes": max(
                (stats["all_domains"]["peak_live_requested_bytes"] for stats in single_owner), default=0
            ),
            "max_single_origin_owner_nonshared_live_requested_bytes": max(
                (stats["nonshared_domains"]["peak_live_requested_bytes"] for stats in single_owner), default=0
            ),
            "outstanding": outstanding,
            "outstanding_shared_registry": [item for item in outstanding if item["domain"] == 2],
        },
        "registry": {
            "active_count": len(registry),
            "peak_active_count": registry_peak,
            "active": encoded(registry),
            "generations": registry_generations,
        },
        "interpretation": {
            "allocation_bytes": "completed/requested backend allocator bytes in instrumented domains; excludes allocator overhead and RSS",
            "peak_order": "global event observation order; not physical allocator-internal simultaneity",
            "event_owner": "event field identity: registry C++ owner versus arena base/causal owner for other events; 0 is unscoped",
            "allocation_origin": "owner/thread from allocation observation; deallocation scope may differ",
            "single_origin_owner_peak": "maximum nonzero raw origin-owner identity bucket peak; excludes owner 0 and may merge reused arena generations; domains 2/3/4 are causal attribution, not exclusive ownership",
            "nonshared_domains": "domains 1/5 only; shared metadata domains 2/3/4 are excluded; raw owner identity is not a lifetime generation",
            "shared_metadata": "domain 2 shared registry, domain 3 shared pool cached slots, domain 4 shared pool state/control block; owner attribution is causal",
            "shared_registry": "domain 2 is shared registry storage; retained buckets and other live allocations are reported at trace end",
            "registry_generations": "generation is register event sequence; owner intervals begin inclusive and end at rebind/erase observation; null end means still active at collection",
            "generic_sums": "bytes/value units are event-specific; sums are grouped by kind and must not be combined across kinds",
            "coverage": "scoped diagnostic observations only; source identity and case-specific expected counts audited separately",
        },
    }
