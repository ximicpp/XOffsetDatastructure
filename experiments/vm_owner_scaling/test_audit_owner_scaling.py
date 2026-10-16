"""Owned synthetic records test rejection logic, never native execution evidence."""

import copy
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("owner_scaling_audit", Path(__file__).with_name("audit_owner_scaling.py"))
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


def synthetic_record(c=8, t=1, r=256, *, profile="diagnostic", control="none"):
    """Artificial native-Apple-arm64 align8 trace; never execution evidence."""
    made = 0 if control == "partial_startup" else 3 if control == "fail_create" else c
    allocated = 0 if control in ("partial_startup", "fail_create") else 7 if control == "fail_allocate" else r
    verified = r if control == "none" else 7 if control == "fail_verify" else 0
    checksum = 14695981039346656037
    for k in range(verified):
        checksum = ((checksum ^ (0x195A926A5CC24873 ^ k)) * 1099511628211) & ((1 << 64) - 1)
    q = r // c
    owners = []
    for i in range(c):
        n = min(q, max(0, allocated - i * q)) if i < made else 0
        end = 48 + 24 * n
        owners.append(dict(index=i, worker=i % t, core=str(0x70000000 + 256 * i) if i < made else "0",
                           base=str(0x10000000 + 0x20000 * i) if i < made else "0",
                           logical=65536 if i < made else 0, used_end=end if i < made else 0,
                           free_before=65536 - end if i < made else 0,
                           free_after=65536 - end + 24 * n if i < made else 0,
                           created=i < made, allocated=n, freed=n, destroyed=i < made))
    started = 2 if control == "partial_startup" else t
    record = dict(schema_version=1, profile=profile, timing_evidence=False,
                  condition=dict(owners=c, threads=t, requests=r, per_owner=q), control=control,
                  status="ok" if control == "none" else "expected_failure",
                  reason=None if control == "none" else "control:" + control,
                  counts=dict(threads_started=started, threads_joined=started, created=made,
                              allocated=allocated, freed=allocated, destroyed=made, validated=verified),
                  checksum=str(checksum),
                  phases={p: dict(wall_ns=None if profile == "diagnostic" or control == "partial_startup" else 123)
                          for p in ("create", "allocate", "free", "destroy")}, owners=owners,
                  validation=dict(values=control == "none", alignment=True, ranges=True,
                                  nonoverlap=True, conservation=True), trace=None)
    if profile == "timing":
        return record
    events, nodes, vectors = [], {}, {}
    serial = 0x90000000
    mutex = 0x71000000
    slot_for_lane = {lane: t - lane - 1 for lane in range(t)}  # slots are not OS lanes

    def pointer():
        nonlocal serial
        serial += 0x10000
        return serial

    def emit(phase, lane, kind, owner=0, subject=0, size=0, value=0):
        events.append(dict(sequence=len(events) + 1, thread=slot_for_lane[lane], phase=phase,
                           kind=kind, owner=owner, subject=subject, bytes=size, value=value))

    bucket, bucket_size, registered = None, 0, 0
    for lane in range(t):
        for item in owners[lane::t]:
            if not item["created"]:
                continue
            base, core = int(item["base"]), int(item["core"])
            emit("create", lane, "os_reserve", subject=base, size=65536)
            emit("create", lane, "os_enable", subject=base, size=65536)
            emit("create", lane, "grow_zero", base, base, 65536)
            nodes[base] = pointer()
            emit("create", lane, "host_allocate", base, nodes[base], 32, 2)
            registered += 1
            triggers, sizes = (1, 3, 6, 12, 24, 48, 98, 198), (2, 5, 11, 23, 47, 97, 197, 397)
            if registered in triggers:
                fresh, fresh_size = pointer(), sizes[triggers.index(registered)] * 8
                emit("create", lane, "host_allocate", base, fresh, fresh_size, 2)
                if bucket is not None:
                    emit("create", lane, "host_deallocate", base, bucket, bucket_size, 2)
                bucket, bucket_size = fresh, fresh_size
            emit("create", lane, "registry_register", core, base)
            emit("create", lane, "core_metadata_capacity", base, core, 0, 24)
            emit("create", lane, "registry_lock", base, mutex, 2, 3)
    for lane in range(t):
        for item in owners[lane::t]:
            if not item["created"]:
                continue
            base, core = int(item["base"]), int(item["core"])
            for j in range(item["allocated"]):
                emit("allocate", lane, "registry_lookup_hit", core, base)
                emit("allocate", lane, "registry_lock", base, mutex, 2, 3)
                emit("allocate", lane, "core_new", base, base + 64 + 24 * j, 8, 72 + 24 * j)
    for lane in range(t):
        for item in owners[lane::t]:
            if not item["created"]:
                continue
            base, core = int(item["base"]), int(item["core"])
            vector, capacity = None, 0
            for j in range(item["freed"]):
                emit("free", lane, "registry_lookup_hit", core, base)
                emit("free", lane, "registry_lock", base, mutex, 2, 3)
                if j == capacity:
                    fresh, fresh_capacity = pointer(), max(1, 2 * capacity)
                    emit("free", lane, "host_allocate", base, fresh, fresh_capacity * 16, 1)
                    if vector is not None:
                        emit("free", lane, "host_deallocate", base, vector, capacity * 16, 1)
                    vector, capacity = fresh, fresh_capacity
                emit("free", lane, "core_return", base, base + 64 + 24 * j, 24, j + 1)
                emit("free", lane, "core_metadata_capacity", base, core, 16 * capacity, 24)
            vectors[base] = (vector, capacity)
    for lane in range(t):
        for item in owners[lane::t]:
            if not item["created"]:
                continue
            base, core = int(item["base"]), int(item["core"])
            emit("destroy", lane, "host_deallocate", base, nodes[base], 32, 2)
            emit("destroy", lane, "registry_erase", core, base, 0, 1)
            emit("destroy", lane, "registry_lock", base, mutex, 2, 3)
            vector, capacity = vectors[base]
            if vector is not None:
                emit("destroy", lane, "host_deallocate", base, vector, 16 * capacity, 1)
            emit("destroy", lane, "os_unmap", base, base, 65536)
    record["trace"] = dict(schema="n3-events-v1", diagnostic=True, timing_evidence=False,
                           valid=True, dropped=0, thread_count=t if made else 0, events=events)
    return record


class AuditorTests(unittest.TestCase):
    def reject(self, record, **kwargs):
        with self.assertRaises(audit.AuditError):
            audit.audit_record(record, **kwargs)

    def test_synthetic_six_complete_diagnostic_traces(self):
        for c, t in sorted(audit.CONDITIONS):
            with self.subTest(c=c, t=t):
                record = synthetic_record(c, t)
                result = audit.audit_record(record, expected_profile="diagnostic", expected_condition=(c, t, 256))
                self.assertEqual(result["status"], "passed")
                self.assertEqual(result["trace_summary"]["registry"]["active_count"], 0)

    def test_timing_semantic_comparator_and_large_capacity(self):
        for c, t in sorted(audit.CONDITIONS):
            for r in (256, 8192):
                with self.subTest(c=c, t=t, r=r):
                    result = audit.audit_record(synthetic_record(c, t, r, profile="timing"))
                    self.assertIsNone(result["trace_summary"])

    def test_four_precise_failure_controls_both_profiles(self):
        for profile in ("timing", "diagnostic"):
            for control in sorted(audit.CONTROLS - {"none"}):
                c, t = (256, 4) if control == "partial_startup" else (8, 1)
                with self.subTest(profile=profile, control=control):
                    audit.audit_record(synthetic_record(c, t, profile=profile, control=control), expected_control=control)

    def test_owned_copy_wrong_request_count(self):
        record = copy.deepcopy(synthetic_record())
        record["condition"].update(requests=512, per_owner=64)
        self.reject(record)

    def test_owned_copy_deleted_lookup_renumbered_generic_valid(self):
        record = copy.deepcopy(synthetic_record())
        events = record["trace"]["events"]
        del events[next(i for i, e in enumerate(events) if e["kind"] == "registry_lookup_hit")]
        for i, event in enumerate(events, 1):
            event["sequence"] = i
        module, catalog = audit._foundation()
        module.validate_trace(record["trace"], event_kinds_path=catalog)
        self.reject(record)

    def test_owned_copy_missing_free_generic_reports_residual(self):
        record = copy.deepcopy(synthetic_record())
        events = record["trace"]["events"]
        del events[next(i for i, e in enumerate(events)
                        if e["phase"] == "destroy" and e["kind"] == "host_deallocate" and e["value"] == 1)]
        for i, event in enumerate(events, 1):
            event["sequence"] = i
        module, catalog = audit._foundation()
        summary = module.validate_trace(record["trace"], event_kinds_path=catalog)
        self.assertEqual(len(summary["allocations"]["outstanding"]), 2)
        self.reject(record)

    def test_ledger_conservation_and_worker_mapping(self):
        for key, value in (("free_after", 1), ("worker", 1), ("destroyed", False), ("base", "0")):
            record = synthetic_record()
            record["owners"][0][key] = value
            self.reject(record)

    def test_phase_and_owner_identity_and_byte_mismatches(self):
        for key, value in (("phase", "create"), ("owner", 42), ("bytes", 9)):
            record = synthetic_record()
            event = next(e for e in record["trace"]["events"] if e["kind"] == "core_new")
            event[key] = value
            self.reject(record)

    def test_incomplete_or_overflow_record_rejected(self):
        for key, value in (("valid", False), ("dropped", 1), ("thread_count", 17)):
            record = synthetic_record()
            record["trace"][key] = value
            self.reject(record)

    def test_schema_and_expected_identity(self):
        for field, value in (("schema_version", True), ("checksum", "01"), ("timing_evidence", True)):
            record = synthetic_record()
            record[field] = value
            self.reject(record)
        self.reject(synthetic_record(), expected_profile="timing")
        self.reject(synthetic_record(), expected_condition=(64, 1, 256))
        self.reject(synthetic_record(control="fail_create"))

    def test_exact_control_cause_and_progress(self):
        for field, value in (("reason", "some exception"), ("status", "ok")):
            record = synthetic_record(control="fail_allocate")
            record[field] = value
            self.reject(record, expected_control="fail_allocate")
        record = synthetic_record(control="fail_verify")
        record["counts"]["validated"] = 256
        self.reject(record, expected_control="fail_verify")

    def test_wall_ns_requires_exact_uint64(self):
        for value in (0.5, 1 << 64, 1e300, -1, True):
            with self.subTest(value=value):
                record = synthetic_record(profile="timing")
                record["phases"]["allocate"]["wall_ns"] = value
                self.reject(record)
        record = synthetic_record(profile="timing")
        record["phases"]["allocate"]["wall_ns"] = (1 << 64) - 1
        audit.audit_record(record)

    def test_wrong_platform32_stride_fixture_rejected(self):
        record = synthetic_record()
        by_base = {int(o["base"]): o for o in record["owners"]}
        for owner in record["owners"]:
            end = 40 + 32 * owner["allocated"]
            owner.update(used_end=end, free_before=65536 - end,
                         free_after=65536 - end + 24 * owner["allocated"])
        for kind in ("core_new", "core_return"):
            index = {}
            for event in record["trace"]["events"]:
                if event["kind"] != kind:
                    continue
                base = event["owner"]
                self.assertIn(base, by_base)
                j = index.get(base, 0)
                event["subject"] = base + 64 + 32 * j
                if kind == "core_new":
                    event["value"] = 72 + 32 * j
                index[base] = j + 1
        # The generic structural audit has no allocation-geometry contract;
        # the workload audit must enforce the fixed native ABI expectation.
        module, catalog = audit._foundation()
        module.validate_trace(record["trace"], event_kinds_path=catalog)
        self.reject(record)


if __name__ == "__main__":
    unittest.main()
