"""Synthetic unit tests of the N3 event auditor, not native diagnostic evidence."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_events import MAX_EVENTS_PER_THREAD, MAX_THREADS, TraceValidationError, validate_trace


def event(kind, *, thread=0, phase="synthetic", owner=100, subject=200, bytes=0, value=0):
    return {
        "sequence": 0,
        "thread": thread,
        "phase": phase,
        "kind": kind,
        "owner": owner,
        "subject": subject,
        "bytes": bytes,
        "value": value,
    }


def trace(*events, thread_count=1):
    events = copy.deepcopy(list(events))
    for sequence, item in enumerate(events, 1):
        item["sequence"] = sequence
    return {
        "schema": "n3-events-v1",
        "diagnostic": True,
        "timing_evidence": False,
        "valid": True,
        "dropped": 0,
        "thread_count": thread_count,
        "events": events,
    }


class AuditEventsTests(unittest.TestCase):
    def assertInvalid(self, value, message=None):
        with self.assertRaises(TraceValidationError) as caught:
            validate_trace(value)
        if message is not None:
            self.assertIn(message, str(caught.exception))

    def test_empty_trace(self):
        result = validate_trace(trace(thread_count=0))
        self.assertEqual(result["event_count"], 0)
        self.assertEqual(result["allocations"]["peak_live_requested_bytes"], 0)
        self.assertEqual(result["registry"]["active"], {})
        json.dumps(result)

    def test_interleaved_owner_peaks_and_allocation_origin(self):
        result = validate_trace(trace(
            event("host_allocate", owner=100, subject=1000, bytes=80, value=1),
            event("host_allocate", thread=1, owner=200, subject=2000, bytes=120, value=5),
            event("host_allocate", owner=100, subject=3000, bytes=40, value=2),
            event("host_deallocate", thread=1, owner=999, subject=1000, bytes=80, value=1),
            event("host_allocate", thread=1, owner=200, subject=1000, bytes=30, value=3),
            event("host_deallocate", owner=999, subject=2000, bytes=120, value=5),
            event("host_deallocate", owner=999, subject=1000, bytes=30, value=3),
            event("host_allocate_failed", owner=200, subject=0, bytes=900, value=1),
            thread_count=2,
        ))
        stats = result["allocations"]
        self.assertEqual(stats["allocation_calls"], 4)
        self.assertEqual(stats["allocated_requested_bytes"], 270)
        self.assertEqual(stats["deallocation_calls"], 3)
        self.assertEqual(stats["deallocated_requested_bytes"], 230)
        self.assertEqual(stats["peak_live_requested_bytes"], 240)
        self.assertEqual(stats["current_live_requested_bytes"], 40)
        self.assertEqual(stats["max_single_origin_owner_live_requested_bytes"], 150)
        self.assertEqual(stats["max_single_origin_owner_nonshared_live_requested_bytes"], 120)
        self.assertEqual(stats["failed_allocation_calls"], 1)
        self.assertEqual(stats["failed_requested_bytes"], 900)
        self.assertEqual(stats["origin_owners"]["100"]["all_domains"]["deallocation_calls"], 1)
        self.assertNotIn("999", stats["origin_owners"])
        self.assertEqual(stats["origin_threads"]["0"]["deallocation_calls"], 1)
        self.assertEqual(stats["origin_threads"]["1"]["deallocation_calls"], 2)
        self.assertEqual(stats["domains"]["2"]["current_live_requested_bytes"], 40)
        self.assertEqual(stats["outstanding_shared_registry"][0]["subject"], 3000)
        self.assertEqual(result["by_owner"]["999"]["by_kind"]["host_deallocate"]["count"], 3)

    def test_zero_owner_is_unscoped_not_single_owner_peak(self):
        stats = validate_trace(trace(event("host_allocate", owner=0, subject=1, bytes=99, value=4)))["allocations"]
        self.assertEqual(stats["peak_live_requested_bytes"], 99)
        self.assertEqual(stats["max_single_origin_owner_live_requested_bytes"], 0)
        self.assertEqual(stats["origin_owners"]["0"]["all_domains"]["peak_live_requested_bytes"], 99)

    def test_all_domains_retained_allocations_and_domain_peaks(self):
        events = [event("host_allocate", subject=domain, bytes=domain * 10, value=domain) for domain in range(1, 6)]
        stats = validate_trace(trace(*events))["allocations"]
        self.assertEqual(stats["current_live_requested_bytes"], 150)
        self.assertEqual(len(stats["outstanding"]), 5)
        self.assertEqual(len(stats["outstanding_shared_registry"]), 1)
        for domain in range(1, 6):
            self.assertEqual(stats["domains"][str(domain)]["peak_live_requested_bytes"], domain * 10)

    def test_registry_and_pool_domains_are_shared_causal_metadata(self):
        events = [event("host_allocate", subject=domain, bytes=domain * 10, value=domain) for domain in range(1, 6)]
        events.append(event("host_deallocate", owner=999, subject=4, bytes=40, value=4))
        allocations = validate_trace(trace(*events))["allocations"]
        owner = allocations["origin_owners"]["100"]
        self.assertEqual(owner["all_domains"]["peak_live_requested_bytes"], 150)
        self.assertEqual(owner["nonshared_domains"]["peak_live_requested_bytes"], 60)
        self.assertEqual(owner["nonshared_domains"]["current_live_requested_bytes"], 60)
        self.assertEqual(owner["shared_metadata_causal"]["peak_live_requested_bytes"], 90)
        self.assertEqual(owner["shared_metadata_causal"]["current_live_requested_bytes"], 50)
        self.assertEqual(owner["shared_registry_causal"]["current_live_requested_bytes"], 20)
        self.assertEqual(owner["shared_pool_causal"]["peak_live_requested_bytes"], 70)
        self.assertEqual(owner["shared_pool_causal"]["current_live_requested_bytes"], 30)
        self.assertEqual(allocations["max_single_origin_owner_nonshared_live_requested_bytes"], 60)

    def test_domain_peak_sum_differs_from_concurrent_peak(self):
        stats = validate_trace(trace(
            event("host_allocate", subject=1, bytes=100, value=1),
            event("host_deallocate", subject=1, bytes=100, value=1),
            event("host_allocate", subject=2, bytes=200, value=2),
        ))["allocations"]
        self.assertEqual(stats["peak_live_requested_bytes"], 200)
        self.assertEqual(sum(s["peak_live_requested_bytes"] for s in stats["domains"].values()), 300)

    def test_registry_move_lookup_failure_and_erase(self):
        result = validate_trace(trace(
            event("registry_lookup_miss", owner=0),
            event("registry_register_failed"),
            event("registry_register"),
            event("registry_lookup_hit"),
            event("registry_rebind", owner=101),
            event("registry_lookup_hit", owner=101),
            event("registry_register", owner=102, subject=202),
            event("registry_erase", owner=101, value=1),
            event("registry_lookup_miss", owner=0),
        ))
        self.assertEqual(result["registry"]["active_count"], 1)
        self.assertEqual(result["registry"]["peak_active_count"], 2)
        self.assertEqual(result["registry"]["active"], {"202": 102})
        first = result["registry"]["generations"][0]
        self.assertEqual(first["generation"], 3)
        self.assertEqual(first["begin_sequence"], 3)
        self.assertEqual(first["end_sequence"], 8)
        self.assertEqual(first["rebinds"], [{"sequence": 5, "from_owner": 100, "to_owner": 101}])
        self.assertEqual(first["owner_intervals"], [
            {"owner": 100, "begin_sequence": 3, "end_sequence": 5},
            {"owner": 101, "begin_sequence": 5, "end_sequence": 8},
        ])

    def test_registry_base_reuse_has_distinct_generations(self):
        registry = validate_trace(trace(
            event("registry_register"),
            event("registry_erase", value=1),
            event("registry_register", owner=101),
        ))["registry"]
        self.assertEqual([item["generation"] for item in registry["generations"]], [1, 3])
        self.assertEqual([item["base"] for item in registry["generations"]], [200, 200])
        self.assertEqual([item["end_sequence"] for item in registry["generations"]], [2, None])

    def test_generic_kind_bytes_value_and_phase_thread_tallies(self):
        result = validate_trace(trace(
            event("registry_lock", subject=77, bytes=3, value=5, phase="a"),
            event("pool_lock", subject=88, bytes=7, value=11, phase="a"),
            event("pool_lock", subject=88, bytes=13, value=17, phase="b", thread=1),
            thread_count=2,
        ))
        self.assertEqual(result["by_kind"]["pool_lock"], {"count": 2, "bytes": 20, "value": 28})
        self.assertEqual(result["by_thread"]["1"]["event_count"], 1)
        self.assertEqual(result["by_phase"]["a"]["event_count"], 2)

    def test_sequence_missing_duplicate_reordered_or_not_starting_one(self):
        for seq in ([2, 3], [1, 1], [1, 3], [2, 1], [0, 1]):
            with self.subTest(sequence=seq):
                value = trace(event("core_scan"), event("core_scan"))
                for item, number in zip(value["events"], seq):
                    item["sequence"] = number
                self.assertInvalid(value, "complete global sequence")

    def test_invalidated_or_dropped_trace(self):
        for field, invalid in (("valid", False), ("dropped", 1), ("diagnostic", False), ("timing_evidence", True)):
            with self.subTest(field=field):
                value = trace()
                value[field] = invalid
                self.assertInvalid(value)

    def test_thread_exhaustion_and_per_thread_storage_overflow(self):
        self.assertInvalid(trace(thread_count=MAX_THREADS + 1), "thread limit")
        self.assertEqual(validate_trace(trace(thread_count=MAX_THREADS))["thread_count"], MAX_THREADS)
        events = [event("pool_scan", owner=0) for _ in range(MAX_EVENTS_PER_THREAD + 1)]
        # Two declared slots leave global capacity available; one overflowing
        # slot must still invalidate the trace even if valid/dropped were forged.
        self.assertInvalid(trace(*events, thread_count=2), "per-thread")
        events[-1]["thread"] = 1
        result = validate_trace(trace(*events, thread_count=2))
        self.assertEqual(result["by_thread"]["0"]["event_count"], MAX_EVENTS_PER_THREAD)
        self.assertEqual(result["by_thread"]["1"]["event_count"], 1)

    def test_utf8_label_byte_limits_not_character_count(self):
        for field in ("phase", "kind"):
            for invalid in ("x" * 64, "界" * 22, "\ud800"):
                with self.subTest(field=field, invalid=repr(invalid)):
                    changed = trace(event("pool_scan", owner=0))
                    changed["events"][0][field] = invalid
                    self.assertInvalid(changed, "UTF-8")
        for valid_phase in ("x" * 63, "界" * 21):
            with self.subTest(phase=valid_phase):
                self.assertEqual(validate_trace(trace(event("pool_scan", owner=0, phase=valid_phase)))["event_count"], 1)

    def test_duplicate_unmatched_free_wrong_size_domain_and_extra_free(self):
        allocate = event("host_allocate", subject=1, bytes=8, value=1)
        free = event("host_deallocate", subject=1, bytes=8, value=1)
        for events in (
            (allocate, allocate),
            (free,),
            (allocate, event("host_deallocate", subject=1, bytes=9, value=1)),
            (allocate, event("host_deallocate", subject=1, bytes=8, value=2)),
            (allocate, free, free),
        ):
            with self.subTest(events=events):
                self.assertInvalid(trace(*events))

    def test_bad_allocation_pointer_domain_and_failed_pointer(self):
        for invalid in (
            event("host_allocate", subject=0, value=1),
            event("host_deallocate", subject=0, value=1),
            event("host_allocate_failed", subject=1, value=1),
            event("host_allocate", subject=1, value=0),
            event("host_allocate", subject=1, value=6),
            event("host_allocate_fail", subject=0, value=1),
        ):
            with self.subTest(event=invalid):
                self.assertInvalid(trace(invalid))

    def test_unknown_generic_kind_and_catalog_identity_constraints(self):
        self.assertInvalid(trace(event("invented_generic_kind")), "unknown event kind")
        for changes in ({"owner": 0}, {"subject": 0}, {"bytes": 1}, {"value": 1}):
            with self.subTest(changes=changes):
                self.assertInvalid(trace(event("core_scan", **changes)))
        self.assertInvalid(trace(event("extent_request", subject=200)))
        self.assertInvalid(trace(event("os_reserve", value=1)))

    def test_catalog_missing_malformed_or_wrong_rule_rejected(self):
        with mock.patch("audit_events.Path.read_text", side_effect=FileNotFoundError("missing")):
            self.assertInvalid(trace(), "catalog unavailable")
        with mock.patch("audit_events.Path.read_text", return_value="{"):
            self.assertInvalid(trace(), "catalog unavailable")
        for catalog in (
            None,
            {"schema": "other", "kinds": {}},
            {"schema": "n3-event-kinds-v1", "kinds": {}},
            {"schema": "n3-event-kinds-v1", "kinds": {"x": {}}},
            {"schema": "n3-event-kinds-v1", "kinds": {"x": {
                "owner": "any", "subject": "any", "bytes": {"type": "int"}, "value": {"type": "uint64"},
            }}},
            {"schema": "n3-event-kinds-v1", "kinds": {"x": {
                "owner": "any", "subject": "any", "bytes": {"type": "uint64"}, "value": {"type": "uint64"},
                "value_min": True,
            }}},
        ):
            with self.subTest(catalog=catalog):
                with mock.patch("audit_events.Path.read_text", return_value=json.dumps(catalog)):
                    self.assertInvalid(trace(), "catalog")

    def test_explicit_frozen_catalog_path_is_used(self):
        catalog = {"schema": "n3-event-kinds-v1", "kinds": {"frozen_kind": {
            "owner": "any", "subject": "any", "bytes": {"type": "uint64"}, "value": {"type": "uint64"},
            "bytes_min": 2, "bytes_max": 3,
        }}}
        with mock.patch("audit_events.Path.read_text", return_value=json.dumps(catalog)) as read:
            result = validate_trace(trace(event("frozen_kind", bytes=2)), event_kinds_path="/frozen/event_kinds.json")
            self.assertEqual(result["event_count"], 1)
            read.assert_called_once_with(encoding="utf-8")
        for invalid_bytes in (1, 4):
            with self.subTest(bytes=invalid_bytes):
                with mock.patch("audit_events.Path.read_text", return_value=json.dumps(catalog)):
                    self.assertInvalid(trace(event("frozen_kind", bytes=invalid_bytes)))

    def test_registry_negative_ownership_and_balance(self):
        register = event("registry_register")
        for events in (
            (register, register),
            (event("registry_lookup_hit"),),  # Removed register.
            (event("registry_erase", value=1),),
            (event("registry_rebind"),),
            (register, event("registry_lookup_hit", owner=999)),
            (register, event("registry_erase", owner=999, value=1)),
            (register, event("registry_lookup_miss", owner=0)),
            (register, event("registry_erase", value=1), event("registry_lookup_hit")),
        ):
            with self.subTest(events=events):
                self.assertInvalid(trace(*events))

    def test_registry_invalid_subject_owner_count_and_bytes(self):
        for invalid in (
            event("registry_register", owner=0),
            event("registry_register", subject=0),
            event("registry_register_failed", subject=0),
            event("registry_register", value=1),
            event("registry_register", bytes=1),
            event("registry_lookup_miss", owner=1),
            event("registry_lookup_miss", owner=0, subject=0),
            event("registry_erase", value=0),
            event("registry_lock", subject=0),
            event("registry_unregister"),
        ):
            with self.subTest(event=invalid):
                self.assertInvalid(trace(invalid))

    def test_exact_object_fields(self):
        value = trace(event("core_scan"))
        for container, field in (("trace", "valid"), ("event", "owner")):
            with self.subTest(container=container, missing=field):
                changed = copy.deepcopy(value)
                del (changed if container == "trace" else changed["events"][0])[field]
                self.assertInvalid(changed, "fields differ")
        for container in ("trace", "event"):
            with self.subTest(container=container, extra=True):
                changed = copy.deepcopy(value)
                (changed if container == "trace" else changed["events"][0])["surprise"] = 0
                self.assertInvalid(changed, "fields differ")

    def test_invalid_primitive_types_in_every_numeric_event_field(self):
        for field in ("sequence", "thread", "owner", "subject", "bytes", "value"):
            for invalid in (True, False, None, "1", 1.0, -1, 1 << 64, [], {}):
                with self.subTest(field=field, invalid=invalid):
                    changed = trace(event("core_scan"))
                    changed["events"][0][field] = invalid
                    self.assertInvalid(changed, "uint64")

    def test_invalid_envelope_primitive_types(self):
        for field in ("diagnostic", "timing_evidence", "valid"):
            for invalid in (0, 1, "true", None):
                with self.subTest(field=field, invalid=invalid):
                    changed = trace()
                    changed[field] = invalid
                    self.assertInvalid(changed, "boolean")
        for field in ("dropped", "thread_count"):
            for invalid in (True, -1, 0.0, "0", None, 1 << 64):
                with self.subTest(field=field, invalid=invalid):
                    changed = trace()
                    changed[field] = invalid
                    self.assertInvalid(changed, "uint64")
        for invalid in (None, (), {}, "events"):
            changed = trace()
            changed["events"] = invalid
            self.assertInvalid(changed, "JSON array")
        for invalid in (None, [], "trace"):
            self.assertInvalid(invalid, "JSON object")

    def test_invalid_schema_thread_event_object_and_strings(self):
        changed = trace()
        changed["schema"] = "n3-events-v2"
        self.assertInvalid(changed, "schema")
        self.assertInvalid(trace(event("core_scan", thread=1)), "thread_count")
        self.assertInvalid(trace(event("core_scan"), thread_count=0), "thread_count")
        changed = trace()
        changed["events"] = [None]
        self.assertInvalid(changed, "JSON object")
        for field in ("kind", "phase"):
            for invalid in ("", "bad\x00string", 1, True, None):
                with self.subTest(field=field, invalid=invalid):
                    changed = trace(event("core_scan"))
                    changed["events"][0][field] = invalid
                    self.assertInvalid(changed, "nonempty string")

    def test_validation_remains_active_with_python_optimization(self):
        code = (
            "from audit_events import validate_trace, TraceValidationError\n"
            "try:\n validate_trace({})\n"
            "except TraceValidationError:\n pass\n"
            "else:\n raise RuntimeError('validation disabled')\n"
        )
        completed = subprocess.run(
            [sys.executable, "-O", "-c", code],
            cwd=Path(__file__).resolve().parent,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)


if __name__ == "__main__":
    unittest.main()
