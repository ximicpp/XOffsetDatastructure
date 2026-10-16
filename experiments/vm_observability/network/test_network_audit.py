"""Synthetic audit controls; these are not native-network execution evidence."""

from pathlib import Path
import unittest


HERE = Path(__file__).resolve().parent
# Compile/exec avoids bytecode writes in immutable source/evidence packages.
audit = {"__file__": str(HERE / "network_audit.py"), "__name__": "_network_audit_test_subject"}
exec(compile((HERE / "network_audit.py").read_bytes(), str(HERE / "network_audit.py"), "exec"), audit)
audit_case, audit_fixture = audit["audit_case"], audit["audit_fixture"]
NetworkAuditError = audit["NetworkAuditError"]
# network/ -> vm_observability/ -> experiments/ -> repo/
CATALOG = (HERE.parent / 'foundation/event_kinds.json' if (HERE.parent / 'foundation/event_kinds.json').exists()
           else HERE.parents[2] / "docs/validation/vm-observability/frozen-implementation-v1/source/event_kinds.json")

C = 32 * 1024 * 1024
P = 4096 + 256
W = P + 88
PAGE = 16384


class Evidence:
    def __init__(self):
        self.events = []
        self.pointer = 1000

    def emit(self, kind, phase, owner=0, subject=0, size=0, value=0, thread=0):
        self.events.append(dict(sequence=len(self.events)+1, thread=thread, phase=phase,
                                kind=kind, owner=owner, subject=subject, bytes=size, value=value))
        return len(self.events)

    def allocate(self, domain, size, phase, owner, thread=0):
        self.pointer += 1
        self.emit("host_allocate", phase, owner, self.pointer, size, domain, thread)
        return self.pointer

    def free(self, domain, size, pointer, phase, owner, thread=0):
        self.emit("host_deallocate", phase, owner, pointer, size, domain, thread)

    def reserve(self, base, phase, thread=0, capacity=C, accessible=PAGE):
        self.emit("os_reserve", phase, subject=base, size=capacity, thread=thread)
        self.emit("os_enable", phase, subject=base, size=accessible, thread=thread)

    def load(self, phase, base, thread=0, *, new_mapping=True, pooled_hit=False, retained=False):
        self.emit("load_header_copy", phase, subject=800, size=88, value=801, thread=thread)
        if new_mapping:
            self.reserve(base, phase, thread)
        if pooled_hit:
            self.emit("pool_scan", phase, subject=base, size=C, thread=thread)
        self.emit("load_payload_copy", phase, base, base, P, 802, thread)
        node = self.allocate(2, 32, phase, base, thread)
        if retained:
            self.allocate(2, 16, phase, base, thread)
        owner = base + 900
        self.emit("registry_register", phase, owner, base, thread=thread)
        previous = None
        for size in (16, 32, 64, 128):
            pointer = self.allocate(5, size, phase, base, thread)
            if previous is not None:
                self.free(5, previous[1], previous[0], phase, base, thread)
            previous = pointer, size
        self.free(5, previous[1], previous[0], phase, base, thread)
        return node, owner

    def erase(self, phase, base, node, owner, thread=0, unmap=False, capacity=C):
        self.free(2, 32, node, phase, base, thread)
        self.emit("registry_erase", phase, owner, base, value=1, thread=thread)
        if unmap:
            self.emit("os_unmap", phase, subject=base, size=capacity, thread=thread)

    def trace(self, threads=2):
        return dict(schema="n3-events-v1", diagnostic=True, timing_evidence=False,
                    valid=True, dropped=0, thread_count=threads, events=self.events)


def endpoint():
    return dict(read=W, written=W, allocations=1, waits=0, published=True, terminal_count=1,
                rx_size=W, rx_capacity=W, rx_init_extent=W, header_copy_calls=1, header_copy_bytes=88)


def stats(pool, before):
    return dict(cached_blocks=int(before and pool != "none"),
                cached_reserved_bytes=C if before and pool != "none" else 0,
                cached_accessible_bytes=PAGE if before and pool != "none" else 0,
                hits=2 if pool == "warm" else 0,
                misses=(1 if pool == "warm" else 2) if pool != "none" else 0)


def stdout_base(command="run"):
    return dict(schema_version=1, contract_version="1.0.0", status="passed", command=command,
                diagnostic=True, timing_evidence=False, element_count=1024, semantic_checksum="1234")


def network_evidence(pool="none"):
    out = Evidence()
    preflight = 100_000_000
    node, owner = out.load("init.preflight", preflight, retained=True)
    out.emit("string_assign_copy", "init.preflight", preflight, preflight+128, 21, 888)
    out.erase("init.preflight", preflight, node, owner, unmap=True)
    metadata = []
    if pool != "none":
        for index, name in enumerate(("server", "client")):
            for domain, size in ((3, 64), (4, 128)):
                pointer = out.allocate(domain, size, "init.pool."+name, 0)
                metadata.append((domain, size, pointer))
    cycles = (["warm"] if pool == "warm" else []) + ["work.0", "work.1"]
    raw_cycles = []
    for index, cycle in enumerate(cycles):
        for endpoint_name, base, thread in (("server", 200_000_000, 1), ("client", 300_000_000, 0)):
            phase = f"{endpoint_name}.{cycle}.load"
            is_new = pool != "warm" or index == 0
            node, owner = out.load(phase, base, thread, new_mapping=is_new,
                                   pooled_hit=pool == "warm" and index > 0)
            if endpoint_name == "server":
                out.emit("string_assign_copy", f"server.{cycle}.process", base, base+128, 21, 888, thread)
                out.emit("save_header_copy", f"server.{cycle}.save", base, 990, 88, 991, thread)
                out.emit("save_payload_copy", f"server.{cycle}.save", base, 992, P, 993, thread)
            out.erase(f"{endpoint_name}.{cycle}.release", base, node, owner, thread,
                      unmap=pool == "none")
        raw_cycles.append(dict(cycle=cycle, input_bytes=W, output_bytes=W, server=endpoint(), client=endpoint(),
                               server_saved_size=W, server_saved_capacity=W, client_output_capacity=W,
                               server_strings_released=True))
        if pool == "cold" and index == 0:
            for base in (200_000_000, 300_000_000):
                out.emit("os_unmap", "cold.clear.1", subject=base, size=C)
    if pool != "none":
        for base in (200_000_000, 300_000_000):
            out.emit("os_unmap", "final.clear", subject=base, size=C)
        for domain, size, pointer in metadata:
            out.free(domain, size, pointer, "run.teardown", 0)
    raw = dict(schema="n3-network-raw-v1", diagnostic=True, timing_evidence=False, joined=True,
               command="run", pool=pool, elements=1024, page_size=PAGE, cycles=raw_cycles,
               before_final={ep: stats(pool, True) for ep in ("server", "client")},
               after_final={ep: stats(pool, False) for ep in ("server", "client")},
               logical_ledger=dict(inflight_bytes=0, queue_frames=0, peak_inflight_bytes=4*C))
    stdout = stdout_base()
    stdout.update(pool=pool, iterations=2, max_wire_bytes=C, owner_capacity_bytes=C, max_inflight_bytes=4*C,
                  reserved_inflight_peak_bytes=4*C, connection_count=1, max_inflight_requests=1, timeout_ms=30000,
                  pool_measured_hits=4 if pool == "warm" else 0, pool_measured_misses=4 if pool == "cold" else 0,
                  pool_warmup_hits=0, pool_warmup_misses=2 if pool == "warm" else 0,
                  output_files=["output-0.wire", "output-1.wire"], input_bytes_per_iteration=W,
                  output_bytes_per_iteration=W, samples=[dict(input_bytes=W, output_bytes=W) for _ in range(2)])
    for key in ("cached_blocks", "cached_reserved_bytes", "cached_accessible_bytes"):
        stdout["pool_"+key+"_before_final_clear"] = 2 * stats(pool, True)[key]
    return out.trace(), raw, stdout


def fixture_evidence():
    out = Evidence()
    base, loaded_base = 100_000_000, 200_000_000
    out.reserve(base, "fixture.build")
    node = out.allocate(2, 32, "fixture.build", base)
    out.allocate(2, 16, "fixture.build", base)
    out.emit("registry_register", "fixture.build", base+900, base)
    out.emit("extent_request", "fixture.reserve", base, size=8000, value=4096)
    out.emit("extent_request", "fixture.reserve", base, size=8000, value=4096)
    out.emit("auto_growth", "fixture.reserve", base, size=8000, value=4096)
    out.emit("save_header_copy", "fixture.save", base, 900, 88, 901)
    out.emit("save_payload_copy", "fixture.save", base, 902, P, 903)
    loaded_node, owner = out.load("fixture.readback_verify", loaded_base)
    out.erase("fixture.readback_verify", loaded_base, loaded_node, owner, unmap=True)
    out.erase("fixture.teardown", base, node, base+900, unmap=True)
    stdout = stdout_base("fixture")
    stdout["frame_bytes"] = W
    return out.trace(1), stdout


def renumber(trace):
    for i, event in enumerate(trace["events"], 1):
        event["sequence"] = i


class NetworkAuditTests(unittest.TestCase):
    def run_case(self, evidence):
        return audit_case(*evidence, kind_catalog_path=CATALOG, nominal_bytes=4096, pool=evidence[1]["pool"])

    def reject(self, evidence, fragment):
        with self.assertRaisesRegex(NetworkAuditError, fragment):
            self.run_case(evidence)

    def test_three_pools_positive_and_base_generations(self):
        for pool in ("none", "cold", "warm"):
            with self.subTest(pool=pool):
                result = self.run_case(network_evidence(pool))
                self.assertFalse(result["timing_evidence"])
                self.assertEqual(len(result["loads"]), 7 if pool == "warm" else 5)
                if pool == "warm":
                    self.assertEqual(len(result["mapping_lifetimes"]), 3)
                    self.assertEqual([len(m["registrations"]) for m in result["mapping_lifetimes"]], [1, 3, 3])

    def test_fixture_positive(self):
        result = audit_fixture(*fixture_evidence(), kind_catalog_path=CATALOG, nominal_bytes=4096)
        self.assertEqual(result["trace_summary"]["registry"]["peak_active_count"], 2)

    def test_missing_phase(self):
        e = network_evidence()
        next(x for x in e[0]["events"] if x["kind"] == "load_header_copy")["phase"] = "unscoped"
        self.reject(e, "unexpected/missing network phase")

    def test_missing_warm_record(self):
        e = network_evidence("warm")
        del e[1]["cycles"][0]
        self.reject(e, "warm retained")

    def test_deleted_warm_trace_prefix(self):
        e = network_evidence("warm")
        e[0]["events"] = [x for x in e[0]["events"] if ".warm." not in x["phase"]]
        renumber(e[0])
        self.reject(e, "deallocation without|registration without|count|unmap")

    def test_wrong_payload_copy_bytes(self):
        e = network_evidence()
        next(x for x in e[0]["events"] if x["kind"] == "load_payload_copy")["bytes"] += 4
        self.reject(e, "load_payload_copy bytes")

    def test_wrong_prefix_owner_generation(self):
        e = network_evidence("warm")
        x = next(x for x in e[0]["events"] if x["kind"] == "load_payload_copy" and x["phase"] == "client.work.0.load")
        x["owner"] = 200_000_000
        self.reject(e, "construction-prefix base")

    def test_wrong_active_copy_generation(self):
        e = network_evidence()
        x = next(x for x in e[0]["events"] if x["kind"] == "save_payload_copy")
        x["owner"] = 100_000_000
        self.reject(e, "active arena generation")

    def test_domain5_geometry_wrong_even_when_total_retained(self):
        e = network_evidence()
        for x in e[0]["events"]:
            if x["phase"] == "init.preflight" and x["kind"] in ("host_allocate", "host_deallocate") and x["value"] == 5:
                if x["bytes"] == 16:
                    x["bytes"] = 20
                elif x["bytes"] == 32:
                    x["bytes"] = 28
        self.reject(e, "libc\\+\\+ geometry")

    def test_domain5_swapped_to_domain1(self):
        e = network_evidence()
        x = next(x for x in e[0]["events"] if x["kind"] == "host_allocate" and x["value"] == 5)
        pointer = x["subject"]
        for x in e[0]["events"]:
            if x["subject"] == pointer:
                x["value"] = 1
        self.reject(e, "domain5 allocations")

    def test_wrong_retained_original_phase(self):
        e = network_evidence()
        next(x for x in e[0]["events"] if x["kind"] == "host_allocate" and x["value"] == 2 and x["bytes"] == 16)["phase"] = "run.teardown"
        self.reject(e, "original phase")

    def test_wrong_retained_original_owner(self):
        e = network_evidence()
        next(x for x in e[0]["events"] if x["kind"] == "host_allocate" and x["value"] == 2 and x["bytes"] == 16)["owner"] = 200_000_000
        self.reject(e, "causal preflight base")

    def test_additional_outstanding_domain2_not_whitelisted(self):
        e = network_evidence()
        e[0]["events"].append(dict(sequence=len(e[0]["events"])+1, thread=0, phase="run.teardown", kind="host_allocate",
                                    owner=0, subject=98765, bytes=16, value=2))
        self.reject(e, "retained allocation count")

    def test_unpaired_deallocation(self):
        e = network_evidence()
        next(x for x in e[0]["events"] if x["kind"] == "host_deallocate")["subject"] = 999999
        self.reject(e, "deallocation without matching")

    def test_strict_bad_types(self):
        for field, value in (("valid", 1), ("dropped", False), ("thread_count", True)):
            e = network_evidence()
            e[0][field] = value
            with self.subTest(field=field):
                self.reject(e, "expected boolean|expected uint64")
        for key, value in (("joined", 1), ("elements", True), ("page_size", "16384")):
            e = network_evidence()
            e[1][key] = value
            with self.subTest(key=key):
                self.reject(e, "expected")

    def test_invalid_dropped_unknown_kind_and_domain(self):
        mutations = (("valid", False, "recorder invalidated"), ("dropped", 1, "incomplete recorder"))
        for key, value, error in mutations:
            e = network_evidence()
            e[0][key] = value
            self.reject(e, error)
        e = network_evidence()
        e[0]["events"][0]["kind"] = "new_unknown_kind"
        self.reject(e, "unknown event kind")
        e = network_evidence()
        next(x for x in e[0]["events"] if x["kind"] == "host_allocate")["value"] = 6
        self.reject(e, "above host_allocate maximum")

    def test_2048_hard_budget_not_runtime16384(self):
        e = network_evidence()
        for _ in range(2049):
            e[0]["events"].append(dict(sequence=len(e[0]["events"])+1, thread=0, phase="run.teardown",
                                        kind="registry_lock", owner=0, subject=345, bytes=0, value=0))
        self.reject(e, "hard per-thread 2048")

    def test_oversubscribed_threads(self):
        e = network_evidence()
        e[0]["thread_count"] = 3
        self.reject(e, "workload thread budget")

    def test_join_and_diagnostic_timing_flags(self):
        for target, field, value in ((1, "joined", False), (1, "timing_evidence", True), (2, "diagnostic", False)):
            e = network_evidence()
            e[target][field] = value
            self.reject(e, "expected")

    def test_missing_event_and_sequence_reorder(self):
        e = network_evidence()
        del e[0]["events"][3]
        self.reject(e, "complete global sequence")
        e = network_evidence()
        e[0]["events"][1], e[0]["events"][2] = e[0]["events"][2], e[0]["events"][1]
        self.reject(e, "complete global sequence")

    def test_deleted_semantic_event_resequenced(self):
        e = network_evidence()
        e[0]["events"] = [x for x in e[0]["events"] if not (x["kind"] == "string_assign_copy" and x["phase"] == "server.work.1.process")]
        renumber(e[0])
        self.reject(e, "string_assign_copy count")

    def test_wrong_header_observation_and_pool_resources(self):
        e = network_evidence()
        e[1]["cycles"][0]["server"]["header_copy_bytes"] = 87
        self.reject(e, "header_copy_bytes")
        e = network_evidence("warm")
        e[1]["before_final"]["server"]["cached_accessible_bytes"] += PAGE
        self.reject(e, "cached_accessible_bytes")
        e = network_evidence("cold")
        e[1]["after_final"]["client"]["cached_blocks"] = 1
        self.reject(e, "cached_blocks")

    def test_endpoint_terminal_is_receive_completion_not_sample_completion(self):
        # Frozen W1 receive_frame owns one TerminalEvent; send_exact owns none.
        # exchange's independent ++sample.terminal_count is not IOProgress.
        for endpoint_name in ("server", "client"):
            e = network_evidence()
            e[1]["cycles"][0][endpoint_name]["terminal_count"] = 2
            self.reject(e, "terminal_count: expected 1")

    def test_stdout_iteration_and_output_mismatch(self):
        for key, value in (("iterations", 3), ("output_files", ["output-0.wire", "output-0.wire"])):
            e = network_evidence()
            e[2][key] = value
            self.reject(e, "stdout")

    def test_fixture_growth_and_peak_negative(self):
        for kind, expected in (("auto_growth", "auto_growth count"), ("extent_request", "extent_request count")):
            trace, stdout = fixture_evidence()
            index = next(i for i, x in enumerate(trace["events"]) if x["kind"] == kind)
            del trace["events"][index]
            renumber(trace)
            with self.assertRaisesRegex(NetworkAuditError, expected):
                audit_fixture(trace, stdout, kind_catalog_path=CATALOG, nominal_bytes=4096)


if __name__ == "__main__":
    unittest.main()
