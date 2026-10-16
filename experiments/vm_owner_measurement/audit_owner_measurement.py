"""Pure schedule, native-record, and statistics checks for frozen D023.

This module starts no processes and grants no execution or timing authority.
The caller binds the accepted W1 checker, then calls ``audit_native`` for every
report before summarizing it.  ``summarize`` deliberately has no implicit
checker import: it checks the complete schedule and the native time envelope,
while provenance, approval, receipts, and producer semantics belong to the
collector's separate checks.  Native ``timing_evidence=False`` is preserved.
"""

import math
import statistics
from fractions import Fraction


UINT64_MAX = (1 << 64) - 1
REQUESTS = 8192
PHASES = ("create", "allocate", "free", "destroy")
CONDITIONS = (
    ("A", 8, 1),
    ("B", 64, 1),
    ("C", 256, 1),
    ("D", 256, 2),
    ("E", 256, 4),
    ("F", 256, 8),
)


class AuditError(ValueError):
    """A D023 schedule, native-record, or descriptive-statistics mismatch."""


def _require(ok, message):
    if not ok:
        raise AuditError(message)


def schedule():
    """Return fresh objects for the exact six-prerun, six-block schedule.

    ``position`` is zero-based within the prerun or within its 12-process
    measured block; ``index`` is zero-based across the complete 78 processes.
    """
    result = []

    def append(condition, stage, block, position):
        name, owners, threads = condition
        result.append(dict(index=len(result), stage=stage, block=block,
                           position=position, condition=name, owners=owners,
                           threads=threads, requests=REQUESTS, control="none"))

    for position, condition in enumerate(CONDITIONS):
        append(condition, "prerun", None, position)
    for block in range(6):
        forward = CONDITIONS[block:] + CONDITIONS[:block]
        for position, condition in enumerate(forward + tuple(reversed(forward))):
            append(condition, "measured", block, position)
    return result


def _slot(value, expected=None):
    _require(type(value) is dict, "slot: expected object")
    if expected is None:
        index = value.get("index")
        _require(type(index) is int and 0 <= index < 78,
                 "slot.index: expected integer in [0,77]")
        expected = schedule()[index]
    _require(set(value) == set(expected), "slot: exact frozen fields required")
    for key, wanted in expected.items():
        _require(type(value[key]) is type(wanted) and value[key] == wanted,
                 f"slot.{key}: differs from exact frozen schedule")
    return expected


def _times(record, slot):
    """Recheck the time envelope, without claiming full W1 semantic audit."""
    _require(type(record) is dict, "native report: expected object")
    _require(type(record.get("schema_version")) is int and record["schema_version"] == 1,
             "native report.schema_version: expected 1")
    _require(record.get("profile") == "timing", "native report.profile: expected timing")
    _require(record.get("timing_evidence") is False,
             "native report.timing_evidence: raw false must be preserved")
    _require(record.get("control") == "none", "native report.control: expected none")
    _require(record.get("status") == "ok" and "reason" in record and record["reason"] is None,
             "native report: expected successful unmodified producer outcome")
    _require("trace" in record and record["trace"] is None,
             "native report.trace: timing report must have null trace")
    condition = record.get("condition")
    wanted = dict(owners=slot["owners"], threads=slot["threads"], requests=REQUESTS,
                  per_owner=REQUESTS // slot["owners"])
    _require(type(condition) is dict and set(condition) == set(wanted),
             "native report.condition: exact fields required")
    for key, value in wanted.items():
        _require(type(condition[key]) is int and condition[key] == value,
                 f"native report.condition.{key}: frozen workload mismatch")
    phases = record.get("phases")
    _require(type(phases) is dict and set(phases) == set(PHASES),
             "native report.phases: exact four phases required")
    result = {}
    for phase in PHASES:
        item = phases[phase]
        _require(type(item) is dict and set(item) == {"wall_ns"},
                 f"native report.phases.{phase}: expected only wall_ns")
        value = item["wall_ns"]
        _require(type(value) is int and 0 < value <= UINT64_MAX,
                 f"native report.phases.{phase}.wall_ns: expected positive uint64")
        result[phase] = value
    return result


def audit_native(record, slot, producer_checker):
    """Audit one immutable native report using the caller's bound W1 checker.

    The accepted W1 entry point is ``audit_record``.  This function supplements
    it with D023's strictly positive phase times, which W1 functionality alone
    does not require.  The slot must itself be a member of the frozen schedule.
    """
    frozen_slot = _slot(slot)
    checker = getattr(producer_checker, "audit_record", None)
    _require(callable(checker), "producer checker: audit_record callable required")
    try:
        checked = checker(record, expected_profile="timing",
                          expected_condition=(frozen_slot["owners"], frozen_slot["threads"], REQUESTS),
                          expected_control="none")
    except (ValueError, TypeError, KeyError, OverflowError) as error:
        raise AuditError("accepted W1 semantic audit: " + str(error)) from error
    times = _times(record, frozen_slot)
    return dict(schema="owner-measurement-native-audit-v1", status="passed",
                timing_evidence=False, condition=dict(record["condition"]),
                phases={phase: {"wall_ns": times[phase]} for phase in PHASES},
                producer_audit=checked)


def _metric(values, unit):
    return dict(unit=unit, block_values=values, median=statistics.median(values),
                min=min(values), max=max(values))


def _cell(condition, phase, pairs):
    name, owners, threads = condition
    # Products remain Python integers, so qualification at the inclusive 1.10
    # boundary never depends on a rounded square/fourth root or floating log.
    products = [first[1] * second[1] for first, second in pairs]
    gs = [math.sqrt(product) for product in products]
    aa = max(max(first[1], second[1]) / min(first[1], second[1])
             for first, second in pairs)
    anchor_products = [products[2 * e] * products[2 * e + 1] for e in range(3)]
    anchors = [math.sqrt(math.sqrt(product)) for product in anchor_products]
    checks = dict(
        aa=all(10 * max(first[1], second[1]) <= 11 * min(first[1], second[1])
               for first, second in pairs),
        block_dispersion=100 * max(products) <= 121 * min(products),
        anchor_drift=10000 * max(anchor_products) <= 14641 * min(anchor_products),
    )
    qualified = all(checks.values())
    metrics = dict(batch_ns=_metric(gs, "ns"))
    if phase in ("create", "destroy"):
        metrics["ns_per_owner"] = _metric([value / owners for value in gs], "ns/owner")
    else:
        metrics["ns_per8192_request"] = _metric([value / REQUESTS for value in gs], "ns/request")
        metrics["requests_per_second"] = _metric([REQUESTS * 1_000_000_000 / value for value in gs],
                                                 "requests/s")
    return dict(condition=name, owners=owners, threads=threads, requests=REQUESTS,
                phase=phase, sample_count=12,
                pairs=[dict(block=block, indices=[first[0], second[0]],
                            wall_ns=[first[1], second[1]], g_ns=gs[block])
                       for block, (first, second) in enumerate(pairs)],
                aa=aa, block_dispersion=math.sqrt(max(products) / min(products)),
                anchors_ns=anchors,
                anchor_drift=math.sqrt(math.sqrt(max(anchor_products) / min(anchor_products))),
                qualified=qualified, status="qualified" if qualified else "uncertain",
                checks=checks, metrics=metrics)


def _direction(case_product, reference_product):
    # A ratio of geometric block values is sqrt(P_case/P_reference).
    # "Within 10 percent" is the additive, inclusive [0.9, 1.1] interval.
    if 100 * case_product < 81 * reference_product:
        return "lower"
    if 100 * case_product > 121 * reference_product:
        return "higher"
    return "unresolved"


def _median_direction(case, reference):
    """Classify the mean of the two central radicals without float rounding."""
    squares = sorted(Fraction(a, b) for a, b in zip(case, reference))
    first, second = squares[2:4]

    def compare(threshold):
        # Compare sqrt(first)+sqrt(second) with 2*threshold.  Squaring
        # preserves the relation because both sides are strictly positive.
        remaining = 4 * threshold * threshold - first - second
        if remaining <= 0:
            return 1
        difference = 4 * first * second - remaining * remaining
        return (difference > 0) - (difference < 0)

    if compare(Fraction(9, 10)) < 0:
        return "lower"
    if compare(Fraction(11, 10)) > 0:
        return "higher"
    return "unresolved"


def summarize(records):
    """Summarize exactly one complete frozen window; never remove a sample.

    Each input is exactly ``{'slot': schedule_entry, 'report': native_record}``.
    The caller must first pass every report through ``audit_native`` with its
    identity-bound W1 checker.  Six preruns are checked but excluded from all
    statistics.  No timings, direction labels, or qualifications authorize a
    causal interpretation, inference about tails, or performance adoption.
    """
    _require(type(records) is list and len(records) == 78,
             "records: exactly 78 ordered native processes required")
    frozen = schedule()
    samples = {(name, phase, block): []
               for name, _, _ in CONDITIONS for phase in PHASES for block in range(6)}
    for index, entry in enumerate(records):
        _require(type(entry) is dict and set(entry) == {"slot", "report"},
                 f"records[{index}]: exact slot/report fields required")
        slot = _slot(entry["slot"], frozen[index])
        times = _times(entry["report"], slot)
        if slot["stage"] == "measured":
            for phase in PHASES:
                samples[(slot["condition"], phase, slot["block"])].append((index, times[phase]))
    cells = []
    products = {}
    for condition in CONDITIONS:
        name = condition[0]
        for phase in PHASES:
            pairs = [samples[(name, phase, block)] for block in range(6)]
            _require(all(len(pair) == 2 for pair in pairs), "statistics: exact six pairs required")
            products[(name, phase)] = [pair[0][1] * pair[1][1] for pair in pairs]
            cells.append(_cell(condition, phase, pairs))
    by_cell = {(cell["condition"], cell["phase"]): cell for cell in cells}
    comparisons = []
    for condition, _, _ in CONDITIONS:
        for phase in PHASES:
            case = products[(condition, phase)]
            reference = products[("C", phase)]
            ratios = [math.sqrt(a / b) for a, b in zip(case, reference)]
            directions = [_direction(a, b) for a, b in zip(case, reference)]
            if len(set(directions)) == 1:
                direction = {"lower": "all_lower", "higher": "all_higher",
                             "unresolved": "unresolved"}[directions[0]]
            else:
                direction = "mixed"
            middle = statistics.median(ratios)
            # Classification uses exact rational comparisons of the two
            # central radicals; the displayed median alone is floating point.
            median_direction = _median_direction(case, reference)
            qualified = by_cell[(condition, phase)]["qualified"] and by_cell[("C", phase)]["qualified"]
            comparisons.append(dict(condition=condition, reference_condition="C", phase=phase,
                                    ratios=ratios, median=middle, min=min(ratios), max=max(ratios),
                                    qualified=qualified, status="qualified" if qualified else "uncertain",
                                    ratio_directions=directions, median_direction=median_direction,
                                    direction=direction))
    return dict(
        schema="owner-measurement-statistics-v1", status="complete",
        native_processes=78, prerun_processes=6, measured_processes=72,
        samples_per_condition=12, reference_condition="C", phase_order=list(PHASES),
        cells=cells, comparisons=comparisons,
        interpretation=dict(
            time_basis="Existing main-clock barrier-to-barrier batch wall time; four phases separate",
            normalized_metrics="batch_ns divided by owners for create/destroy; by 8192 for allocate/free",
            throughput="8192 requests divided by batch seconds for allocate/free",
            qualification="Each cell requires all AA, block dispersion and anchor drift <= 1.10; comparisons require both cells",
            direction_interval=dict(lower=0.9, upper=1.1, inclusive=True),
            direction_scope="Each block ratio and its median describe this one window; mixed marks differing block direction labels",
            range="Observed six-block minimum/maximum, not a confidence interval",
            inference="Descriptive 10 percent granularity only; no 2 percent, causal, tail, adoption or optimization conclusion",
            uncertainty="Unqualified cells and comparisons remain present with uncertain status",
            preruns="Six whole fresh processes excluded; they do not prewarm later process-local state",
            native_claim="Producer timing_evidence=false is unchanged; execution authority and qualification are separate wrapper artifacts",
        ),
    )
