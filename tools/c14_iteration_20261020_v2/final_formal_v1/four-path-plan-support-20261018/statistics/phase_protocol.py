"""Source-only four-path Q32/F64 statistics; no runner or native I/O.

Only the identity-checked V5 source supplies the schedule and Student-t/macrocycle
helpers. Binary provenance, actual elapsed wall time, process cleanup and output
truth remain the external executor's responsibility.
"""
from __future__ import annotations

import hashlib
import math
from pathlib import Path
import re
import types


VERSION = "sc-four-path-cluster-t-v1"
FROZEN_BYTES = 13491
FROZEN_SHA256 = "444095c1d6640dd05e11b71e8f75ec2cbf7f3d150734623d015e9a4d7ec16852"
FROZEN_PATH = (Path(__file__).resolve().parents[2] /
               "performance-v5/measurement/comparison_protocol.py")


class ProtocolError(ValueError):
    pass


def _require(condition, message):
    if not condition:
        raise ProtocolError(message)


def _load_frozen():
    try:
        with FROZEN_PATH.open("rb") as stream:
            source = stream.read(FROZEN_BYTES + 1)
    except OSError as error:
        raise ProtocolError("frozen V5 source unavailable") from error
    _require(len(source) == FROZEN_BYTES and
             hashlib.sha256(source).hexdigest() == FROZEN_SHA256,
             "frozen V5 source identity mismatch")
    module = types.ModuleType("_four_path_identity_checked_v5")
    module.__file__ = str(FROZEN_PATH)
    # Execute exactly the verified bytes, avoiding a loader's second file read.
    exec(compile(source, str(FROZEN_PATH), "exec"), module.__dict__)
    return module


_FROZEN = _load_frozen()
ROLES = _FROZEN.ROLES
COMPETITORS = ROLES[2:]
CONTRASTS = ("old_xoffset",) + COMPETITORS
KINDS = ("qualification", "formal")
PHASE_KEYS = ("load", "query_before", "mutate", "emit_after",
              "retire_modified_state", "reload", "query_after", "cleanup")
AA_METRICS = ("read_sum", "mutate", "emit_after", "reload", "total")
PRIMARY_THRESHOLDS = {"read_sum": 1.05, "mutate": 1.05, "emit_after": 0.20}
DIAGNOSTIC_METRICS = ("read_sum", "mutate", "emit_after", "reload", "load",
                      "retire_modified_state", "cleanup", "total")
COST_TRANSFER_METRICS = ("load", "reload", "retire_modified_state", "cleanup", "total")
_DECIMAL = re.compile(r"(?:0|[1-9][0-9]*)", re.ASCII)


def policy(kind):
    _require(type(kind) is str and kind in KINDS, "invalid campaign kind")
    inherited = _FROZEN.policy(kind)
    return {
        "protocol": VERSION, "campaign": kind,
        "frozen_source": {"path": str(FROZEN_PATH), "bytes": FROZEN_BYTES,
                          "sha256": FROZEN_SHA256, "protocol": _FROZEN.VERSION},
        "roles": 6, "role_names": list(ROLES), "competitors": list(COMPETITORS),
        "blocks": inherited["blocks"], "roles_per_block": 6,
        "replicas_per_role_per_block": 2, "children_per_block": 12,
        "warmup_blocks": 2, "warmup_children": 24,
        "measured_children": inherited["measured_children"],
        "maximum_children": inherited["maximum_children"],
        "macrocycles": inherited["macrocycles"], "blocks_per_macrocycle": 6,
        "inference_units": inherited["macrocycles"],
        "degrees_of_freedom": inherited["degrees_of_freedom"],
        "maximum_child_seconds": 20,
        "maximum_window_seconds": inherited["maximum_window_seconds"],
        "schedule_seed": inherited["schedule_seed"],
        "AA_roles": 6, "AA_metrics": list(AA_METRICS), "AA_contrasts": 30,
        "AA_family_alpha": 0.05, "AA_tails": 60,
        "AA_equivalence_factor": 1.05,
        "primary_thresholds": dict(PRIMARY_THRESHOLDS), "primary_contrasts": 12,
        "primary_family_alpha": 0.05, "primary_tails": 12,
        "diagnostic_metrics": list(DIAGNOSTIC_METRICS), "diagnostic_contrasts": 40,
        "diagnostic_family_alpha": 0.05, "diagnostic_tails": 80,
        "cost_transfer_control": "old_xoffset",
        "cost_transfer_metrics": list(COST_TRANSFER_METRICS),
        "comparisons_published": kind == "formal",
        "confidence_families_are_separate": True,
        "combined_overall_95_percent_claimed": False,
        "log_roundoff_tolerance": inherited["log_roundoff_tolerance"],
        "no_outlier_filter": True, "no_optional_stopping": True,
        "performance_retries": 0, "measurement_windows": 1,
        "cpu_affinity_virtual_cpu": inherited["cpu_affinity_virtual_cpu"],
        "whole_cell_qualification": True,
        "fixed_sample_size_not_adapted_from_Q": True,
        "all_blocks_required": True, "zero_metrics_are_not_deleted": True,
        "binary_role_profiles_verified_by_this_module": False,
        "actual_wall_guard_verified_by_this_module": False,
        "executor_must_freeze_binary_role_profiles": True,
        "confidence_model": inherited["confidence_model"],
        "exact_distribution_free_coverage_guaranteed": False,
        "coverage_conditional_on_AA_passing_claimed": False,
    }


def schedule(kind):
    policy(kind)
    return _FROZEN.schedule(kind)


def _unsigned(value, label):
    _require(type(value) is str and len(value) <= 20 and
             _DECIMAL.fullmatch(value) is not None,
             label + " must be a canonical unsigned decimal string")
    number = int(value)
    _require(number < 2**64, label + " exceeds uint64")
    return number


def _validate(rows, expected, p):
    _require(type(rows) is list and len(rows) <= len(expected),
             "invalid sample list length")
    validated = []
    for index, row in enumerate(rows):
        _require(type(row) is dict and set(row) == {"job", "phase_ns", "total_ns"},
                 "sample fields differ at %d" % index)
        want = expected[index]
        job = row["job"]
        _require(type(job) is dict and set(job) == set(want) and
                 all(type(job[key]) is type(value) and job[key] == value
                     for key, value in want.items()),
                 "not exact executed schedule prefix at %d" % index)
        phases = row["phase_ns"]
        _require(type(phases) is dict and set(phases) == set(PHASE_KEYS),
                 "phase fields differ at %d" % index)
        total = _unsigned(row["total_ns"], "total_ns")
        _require(0 < total <= p["maximum_child_seconds"] * 10**9,
                 "total_ns exceeds positive domain/child guard")
        values = {key: _unsigned(phases[key], "phase_ns." + key) for key in PHASE_KEYS}
        _require(all(value <= total for value in values.values()), "phase exceeds total")
        _require(sum(values.values()) == total, "eight phase sum differs from total")
        values["read_sum"] = values["query_before"] + values["query_after"]
        values["total"] = total
        validated.append({"job": want, "metrics": values})
    _require(sum(row["metrics"]["total"] for row in validated) <=
             p["maximum_window_seconds"] * 10**9,
             "serialized child totals exceed window guard")
    return validated


def _cycle_values(blocks, p):
    # Reuse the frozen six-block grouping. Missing values keep their position.
    if all(value is not None for value in blocks):
        return _FROZEN._cycle_means(blocks, p)
    return [None if any(x is None for x in blocks[c * 6:(c + 1) * 6]) else
            _FROZEN._mean(blocks[c * 6:(c + 1) * 6])
            for c in range(p["macrocycles"])]


def _invalid(row, metric):
    return {"job_id": row["job"]["id"], "role": row["job"]["role"],
            "side": row["job"]["side"], "block": row["job"]["block"],
            "metric": metric, "value": str(row["metrics"][metric]),
            "reason": "nonpositive_inference_metric"}


def _interval(blocks, invalid, critical, p, one_sided=False):
    cycles = _cycle_values(blocks, p)
    result = {"available": not invalid, "invalid_jobs": invalid,
              "block_log_values": blocks, "macrocycle_log_means": cycles,
              "inference_units": p["macrocycles"], "df": p["degrees_of_freedom"],
              "critical_t": critical, "log_estimate": None, "standard_error": None,
              "ratio_point_estimate": None, "log_lower": None, "log_upper": None,
              "ratio_lower": None, "ratio_upper": None,
              "simultaneous_family_confidence": 0.95}
    if invalid:
        result["unavailable_reason"] = "zero_inference_metric_no_log_or_deletion"
        return result
    mean, se = _FROZEN._moments(cycles, p)
    lower, upper = mean - critical * se, mean + critical * se
    result.update(log_estimate=mean, standard_error=se,
                  ratio_point_estimate=math.exp(mean), log_upper=upper,
                  ratio_upper=math.exp(upper))
    if not one_sided:
        result.update(log_lower=lower, ratio_lower=math.exp(lower))
    return result


def _aa(by_block, role, metric, critical, p):
    blocks, invalid = [], []
    for block in range(p["blocks"]):
        pair = by_block[block][role]
        first, second = sorted(pair.values(), key=lambda row: row["job"]["id"])
        bad = [row for row in (first, second) if row["metrics"][metric] == 0]
        invalid.extend(_invalid(row, metric) for row in bad)
        blocks.append(None if bad else
                      math.log(first["metrics"][metric] / second["metrics"][metric]))
    result = _interval(blocks, invalid, critical, p)
    margin, tol = math.log(1.05), p["log_roundoff_tolerance"]
    result.update(numerator="chronological_first", denominator="chronological_second",
                  role=role, metric=metric, family_contrasts=30,
                  per_contrast_confidence=1 - 0.05 / 30,
                  equivalence_ratio_limits=[1 / 1.05, 1.05],
                  equivalence_established=result["available"] and
                  result["log_lower"] >= -margin - tol and
                  result["log_upper"] <= margin + tol)
    return result


def _contrast(by_block, other, metric, critical, p, one_sided=False):
    blocks, invalid = [], []
    for block in range(p["blocks"]):
        numerator, denominator = by_block[block]["new_xoffset"], by_block[block][other]
        paired = [numerator["A"], denominator["A"], numerator["B"], denominator["B"]]
        bad = [row for row in paired if row["metrics"][metric] == 0]
        invalid.extend(_invalid(row, metric) for row in bad)
        blocks.append(None if bad else 0.5 * (
            math.log(numerator["A"]["metrics"][metric] / denominator["A"]["metrics"][metric]) +
            math.log(numerator["B"]["metrics"][metric] / denominator["B"]["metrics"][metric])))
    result = _interval(blocks, invalid, critical, p, one_sided)
    result.update(numerator="new_xoffset", denominator=other, metric=metric)
    if one_sided:
        threshold = PRIMARY_THRESHOLDS[metric]
        limit = math.log(threshold) + p["log_roundoff_tolerance"]
        result.update(family_contrasts=12, per_contrast_confidence=1 - 0.05 / 12,
                      threshold=threshold,
                      point_threshold_pass=result["available"] and result["log_estimate"] <= limit,
                      upper_threshold_pass=result["available"] and result["log_upper"] <= limit)
        result["target_pass"] = result["point_threshold_pass"] and result["upper_threshold_pass"]
    else:
        result.update(family_contrasts=40, per_contrast_confidence=1 - 0.05 / 40,
                      direction="unavailable" if not result["available"] else
                      "confirmed_increase" if result["log_lower"] > 0 else
                      "confirmed_decrease" if result["log_upper"] < 0 else "unknown")
    return result


def analyze(rows, kind):
    p = policy(kind)
    expected = schedule(kind)
    validated = _validate(rows, expected, p)
    complete = len(rows) == len(expected)
    result = {
        "schema_version": 1, "protocol": VERSION, "policy": p, "complete": complete,
        "raw_row_count": len(rows), "successful_children": len(rows),
        "excluded_warmup_children": min(24, len(rows)),
        "measured_children": max(0, len(rows) - 24),
        "unexecuted_children": len(expected) - len(rows),
        "completed_blocks": max(0, (len(rows) - 24) // 12),
        "required_blocks": p["blocks"],
        "retained_job_ids": [row["job"]["id"] for row in validated],
        "excluded_warmup_job_ids": [row["job"]["id"] for row in validated[:24]],
        "retained_measured_job_ids": [row["job"]["id"] for row in validated[24:]],
        "all_measured_rows_retained": True, "no_outlier_filter": True,
        "measurement_qualified": False, "AA": None,
        "primary_targets": None, "auxiliary_diagnostics": None,
        "competitor_contrasts_computed": False,
        "confirmed_cost_transfer": False, "confirmed_cost_transfer_contrasts": [],
        "all_inference_defined": False, "primary_targets_all_pass": False,
        "target_achieved": False, "promotion_eligible": False, "failure_reasons": [],
        "confidence_scope": "Separate nominal model-conditional 95% families; not a combined "
                            "overall 95% or coverage conditional on AA passing.",
        "external_binary_identity_verified": False,
    }
    if not complete:
        result.update(status="preserved_incomplete", target_status="not_established_incomplete",
                      failure_reasons=["incomplete_exact_prefix"])
        return result

    by_block = {block: {role: {} for role in ROLES} for block in range(p["blocks"])}
    for row in validated[24:]:
        job = row["job"]
        by_block[job["block"]][job["role"]][job["side"]] = row
    result["block_job_ids"] = {
        str(block): {role: {side: row["job"]["id"] for side, row in pair.items()}
                     for role, pair in roles.items()}
        for block, roles in by_block.items()}
    df = p["degrees_of_freedom"]
    aa_critical = _FROZEN.student_t_quantile(1 - 0.05 / (2 * 30), df)
    aa = {role: {metric: _aa(by_block, role, metric, aa_critical, p)
                 for metric in AA_METRICS} for role in ROLES}
    aa_tests = [test for metrics in aa.values() for test in metrics.values()]
    qualified = all(test["equivalence_established"] for test in aa_tests)
    result.update(AA=aa, measurement_qualified=qualified,
                  status="complete_qualified" if qualified else "complete_unqualified",
                  all_inference_defined=all(test["available"] for test in aa_tests),
                  inference={"units": p["macrocycles"], "blocks_per_unit": 6, "df": df,
                             "native_children_are_independent_units": False,
                             "model": "independent approximately normal balanced six-block macrocycle log means",
                             "bootstrap_used": False, "selection_after_outcome": False})
    for role, metrics in aa.items():
        for metric, test in metrics.items():
            if not test["equivalence_established"]:
                result["failure_reasons"].append("AA_not_established:" + role + ":" + metric)
    if kind == "qualification":
        result.update(qualification_passed=qualified,
                      target_status="not_applicable_qualification_AA_only")
        return result

    primary_critical = _FROZEN.student_t_quantile(1 - 0.05 / 12, df)
    diagnostic_critical = _FROZEN.student_t_quantile(1 - 0.05 / (2 * 40), df)
    primary = {other: {metric: _contrast(by_block, other, metric, primary_critical, p, True)
                       for metric in PRIMARY_THRESHOLDS} for other in COMPETITORS}
    diagnostics = {other: {metric: _contrast(by_block, other, metric, diagnostic_critical, p)
                           for metric in DIAGNOSTIC_METRICS} for other in CONTRASTS}
    primary_tests = [test for metrics in primary.values() for test in metrics.values()]
    diagnostic_tests = [test for metrics in diagnostics.values() for test in metrics.values()]
    all_defined = all(test["available"] for test in aa_tests + primary_tests + diagnostic_tests)
    transfers = [metric for metric in COST_TRANSFER_METRICS
                 if diagnostics["old_xoffset"][metric]["available"] and
                 diagnostics["old_xoffset"][metric]["log_lower"] > 0]
    targets_pass = all(test["target_pass"] for test in primary_tests)
    achieved = qualified and targets_pass and all_defined and not transfers
    for other, metrics in primary.items():
        for metric, test in metrics.items():
            if not test["target_pass"]:
                result["failure_reasons"].append("primary_not_established:" + other + ":" + metric)
    if not all_defined:
        result["failure_reasons"].append("inference_unavailable_zero_metrics")
    result["failure_reasons"].extend("confirmed_cost_transfer:old_xoffset:" + metric
                                     for metric in transfers)
    result.update(primary_targets=primary, auxiliary_diagnostics=diagnostics,
                  competitor_contrasts_computed=True, all_inference_defined=all_defined,
                  primary_targets_all_pass=targets_pass,
                  confirmed_cost_transfer=bool(transfers),
                  confirmed_cost_transfer_contrasts=transfers,
                  target_achieved=achieved, promotion_eligible=achieved,
                  target_status="qualified_target_established" if achieved else
                  "not_established_measurement_unqualified" if not qualified else
                  "not_established_undefined_inference" if not all_defined else
                  "not_established_confirmed_cost_transfer" if transfers else "target_not_established")
    return result
