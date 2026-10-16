#!/usr/bin/env python3
"""Lightweight binder tests. Synthetic archives and mocks are never native evidence."""
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
import bind_network_window as B

class BindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Load source for API tests only. Root later supplies externally accepted
        # frozen-v2 hashes; no native or archived heavy dependency is accessed.
        tools = Path(__file__).resolve().parent
        cls.N = B.load_protocol(tools, B.sha(tools / "network_measurement.py"), B.sha(tools / "paired_measurement.py"))
        cls.P = cls.N.P
        cls.runtime = cls.N.collector_runtime_identity()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        self.N = self.__class__.N
        self.P = self.__class__.P

    def record(self):
        spec = self.N.preregistration()
        expected = {}
        for index, work in enumerate(spec["workloads"]):
            expected[work["id"]] = dict(input_sha256=f"{index*2+1:064x}", output_sha256=f"{index*2+2:064x}",
                input_bytes=work["nominal_bytes"]+352, output_bytes=work["nominal_bytes"]+352,
                semantic_checksum=str(index+1), elements=work["elements"], schema_id=spec["controls"]["schema_id"],
                processing_id=spec["controls"]["processing_id"])
        return dict(expected_data=expected, producer_identity={key: "a"*64 for key in self.N.PRODUCER_KEYS})

    def spec(self):
        with mock.patch.object(self.N, "collector_runtime_identity", return_value=copy.deepcopy(self.runtime)):
            return B.make_spec(self.N, self.record(), self.root / "accepted-build", self.root / "window", self.root / "queue", "network-aa-1")

    def selection_fixture(self, directory=None):
        # Small fabricated summaries exercise selection plumbing only; they do
        # not claim native execution or replace any real acceptance document.
        directory = directory or self.root / "selection-source"
        directory.mkdir()
        rows, accepted_rows = [], []
        for size, local, drift, stable in ((4096,False,False,False),(65536,True,False,False),
                                           (1048576,True,True,True),(16777216,False,True,False)):
            rows.append(dict(workload=str(size), comparison="F-AA", local_aa_qualified=local,
                stable_window_qualified=stable, anchor_drift=dict(qualified=drift),
                calibration={a:dict(qualified=True) for a in ("pre","mid","post")}))
            accepted_rows.append(dict(nominal_bytes=size, local_aa_qualified=local, anchor_drift_qualified=drift,
                stable_window_qualified=stable, anchor_qualification=dict(pre=True,mid=True,post=True)))
        self.P.write_new(directory/"summary.json",dict(evidence_class="native",purpose="calibration",process_count=112,
            adoption_allowed=False,all_stable_windows_qualified=False,rows=rows))
        self.P.write_new(directory/"result.json",dict(schema_version=1,schema="w1-native-network-window",status="audited",
            approval_sha256="a"*64,adoption_allowed=False,timing_claim_qualified=False,
            files={"summary.json":B.identity(directory/"summary.json")}))
        self.P.write_new(directory/"acceptance.json",dict(schema_version=1,status="accepted_executed_partial_qualification",
            comparison_execution_authorized=False,processes=112,all_stable_windows_qualified=False,
            approval_sha256="a"*64,rows=accepted_rows,
            reviewed_sha256={name:B.sha(directory/name) for name in ("result.json","summary.json")}))
        acceptance=directory/"acceptance.json"
        lineage=B.validate_selection(self.N,acceptance,B.sha(acceptance),directory)
        return directory, acceptance, lineage

    def screening_spec(self):
        directory,acceptance,lineage=self.selection_fixture()
        return B.make_spec(self.N,self.record(),self.root/"accepted-build",self.root/"new-screen",self.root/"new-screen-queue",
                           "network-pool-screen-1",mode="pool-screening-1m",selection_lineage=lineage)

    def test_112_schedule_is_pure_calibration_without_dummy_cold_warm(self):
        spec, jobs = self.spec()
        self.assertEqual(len(jobs), 112)
        self.assertEqual(spec["comparisons"], [dict(id="F-AA", left="F-none", right="F-none-B", calibration="F-none")])
        self.assertEqual(set(spec["conditions"]), {"F-none", "F-none-B"})
        self.assertEqual({j["condition"] for j in jobs}, {"F-none"})
        self.assertEqual({j["phase"] for j in jobs}, {"warmup", "calibration"})
        self.assertEqual(sum(j["phase"] == "warmup" for j in jobs), 16)
        for invocation in spec["invocations"].values():
            self.assertEqual(invocation[invocation.index("--pool")+1], "none")
        self.assertEqual(len(set(spec["paths"]["outputs"].values())), 112)
        self.N.validate_preregistration(spec, bound=True)

    def test_bound_spec_has_exact_sizes_iterations_and_separate_capacities(self):
        spec, jobs = self.spec()
        self.assertEqual([(w["nominal_bytes"], w["iterations"]) for w in spec["workloads"]],
                         [(4096,32),(65536,16),(1048576,4),(16777216,1)])
        self.assertEqual(spec["controls"]["owner_capacity_bytes"], 33554432)
        self.assertEqual(spec["controls"]["max_inflight_bytes"], 134217728)
        expected = sum(j["iterations"] * spec["expected_data"][j["workload"]]["output_bytes"] for j in jobs)
        self.assertEqual(spec["retained_output_bytes_expected"], expected)
        self.assertLess(expected, 2*1024**3)
        self.assertFalse(spec["native_execution_authorized"])

    def test_screening_exact_88_jobs_projection_and_full_invocations_budget(self):
        spec,jobs=self.screening_spec()
        self.assertEqual(spec["purpose"],"screening")
        self.assertEqual(spec["workloads"],[dict(id="1048576",nominal_bytes=1048576,elements=262144,iterations=4)])
        self.assertEqual(set(spec["expected_data"]),{"1048576"})
        self.assertEqual(set(spec["conditions"]),{"F-none","F-cold","F-warm"})
        self.assertEqual([c["id"] for c in spec["comparisons"]],["none-cold","none-warm"])
        self.assertEqual(len(jobs),88)
        self.assertEqual({p:sum(j["phase"]==p for j in jobs) for p in ("warmup","calibration","comparison")},
                         dict(warmup=8,calibration=48,comparison=32))
        self.assertEqual(spec["retained_output_bytes_expected"],369222656)
        self.assertEqual(set(spec["invocations"]),{j["id"] for j in jobs})
        self.assertEqual(len(set(spec["paths"]["outputs"].values())),88)
        for job in jobs:
            self.assertEqual(job["iterations"],4)
            self.assertEqual(spec["invocations"][job["id"]],self.N.argv(spec,job))
        for comparison in ("none-cold","none-warm"):
            group=[j for j in jobs if j["comparison"]==comparison]
            self.assertEqual(len(group),44)
            for anchor in ("pre","mid","post"):
                aa=[j for j in group if j["phase"]=="calibration" and j["anchor"]==anchor]
                self.assertEqual(len(aa),8)
                self.assertEqual({j["condition"] for j in aa},{"F-none"})
                self.assertEqual([j["order"] for j in aa].count("AB"),4)
        self.N.validate_preregistration(spec,bound=True)
        self.assertFalse(spec["selection_lineage"]["qualification_carryover"])

    def test_screening_refuses_other_size_iterations_threshold_metric_or_data_mutations(self):
        spec,_=self.screening_spec()
        changes=(lambda s:s["workloads"].append(dict(id="4096",nominal_bytes=4096,elements=1024,iterations=32)),
                 lambda s:s["workloads"][0].update(iterations=8),
                 lambda s:s["workloads"][0].update(elements=1),
                 lambda s:s["policy"].update(noise_relative_limit=.049),
                 lambda s:s["policy"].update(absolute_anchor_drift_relative_limit=.02),
                 lambda s:s["controls"].update(owner_capacity_bytes=67108864),
                 lambda s:s.update(primary_metric="primary_mean_ns"),
                 lambda s:s["selection_lineage"].update(qualification_carryover=True),
                 lambda s:s["selection_lineage"].update(comparison_execution_authorized=True),
                 lambda s:s.update(native_execution_authorized=True),
                 lambda s:s.update(retained_output_bytes_expected=369222657),
                 lambda s:s["conditions"]["F-cold"].update(pool="warm"))
        for change in changes:
            altered=copy.deepcopy(spec)
            change(altered)
            with self.assertRaises(ValueError):
                B.validate_mode(self.N,altered,self.record()["expected_data"])
        altered=copy.deepcopy(spec)
        altered["expected_data"]["1048576"]["semantic_checksum"]="12345"
        with self.assertRaises(ValueError):
            B.validate_mode(self.N,altered,self.record()["expected_data"])

    def test_selection_is_portable_small_metadata_and_rejects_wrong_external_or_resealed_hashes(self):
        directory,acceptance,lineage=self.selection_fixture()
        pin=B.sha(acceptance)
        moved=self.root/"portable-selection"
        shutil.copytree(directory,moved)
        shutil.rmtree(directory)
        before=self.P.inventory(moved)
        self.assertEqual(B.validate_selection(self.N,moved/"acceptance.json",pin,moved),lineage)
        self.assertEqual(self.P.inventory(moved),before)
        with self.assertRaises(ValueError):
            B.validate_selection(self.N,moved/"acceptance.json","0"*64,moved)
        raw=self.P.read(moved/"summary.json")
        raw["rows"][0]["stable_window_qualified"]=True
        (moved/"summary.json").write_bytes(self.P.canonical(raw))
        with self.assertRaises(ValueError):
            B.validate_selection(self.N,moved/"acceptance.json",pin,moved)

    def test_selection_semantic_limits_and_only_one_mib_stable_are_required(self):
        for index,change in enumerate((lambda a:a.update(comparison_execution_authorized=True),
                                      lambda a:a.update(status="approved"),
                                      lambda a:a["rows"][0].update(stable_window_qualified=True),
                                      lambda a:a["rows"][2].update(stable_window_qualified=False),
                                      lambda a:a.update(processes=True))):
            directory,acceptance,_=self.selection_fixture(self.root/f"selection-{index}")
            raw=self.P.read(acceptance)
            change(raw)
            acceptance.write_bytes(self.P.canonical(raw))
            with self.assertRaises(ValueError):
                B.validate_selection(self.N,acceptance,B.sha(acceptance),directory)

    def test_screening_requires_fresh_outputs_and_new_unapproved_parent_gate(self):
        spec,_=self.screening_spec()
        proposal=B.parent_proposal(self.N,spec,"1"*64,{},"2"*64,"3"*64,spec["collector_execution_path"],900)
        self.assertFalse(proposal["native_execution_authorized"])
        self.assertEqual(proposal["status"],"ready_for_parent_review")
        self.assertEqual(proposal["execution_root"],str(self.root/"new-screen"))
        self.assertNotIn("approval_sha256",proposal)
        self.assertFalse((self.root/"new-screen/approval.json").exists())
        with self.assertRaises(ValueError):
            B.parent_proposal(self.N,spec,"1"*64,{},"2"*64,"3"*64,spec["collector_execution_path"],899)
        previous=self.root/"selection-source"
        with self.assertRaises(ValueError):
            B.disjoint_new_outputs(previous,self.root/"new-screen-queue",[previous])

    def test_wrong_expected_data_missing_hash_and_output_budget_rejected(self):
        for mutate in (lambda r: r["expected_data"].pop("4096"),
                       lambda r: r["expected_data"]["4096"].update(output_sha256="bad"),
                       lambda r: r["expected_data"]["4096"].update(output_bytes=33554433),
                       lambda r: r["expected_data"]["16777216"].update(elements=True)):
            record = self.record()
            mutate(record)
            with self.assertRaises((ValueError, KeyError)):
                B.make_spec(self.N, record, self.root/"build", self.root/"window", self.root/"queue", "one")

    def test_outputs_and_queue_cannot_overlap_sources_existing_paths_or_symlinks(self):
        source = self.root / "source"
        source.mkdir()
        for out, queue in ((source/"window", self.root/"queue"), (self.root/"window", self.root/"window/queue"),
                           (source, self.root/"queue"), (self.root/"window", source/"queue")):
            with self.subTest(out=out, queue=queue), self.assertRaises(ValueError):
                B.disjoint_new_outputs(out, queue, [source])
        link = self.root / "link"
        link.symlink_to(source, target_is_directory=True)
        with self.assertRaises(ValueError):
            B.disjoint_new_outputs(link/"window", self.root/"queue", [source])
        B.disjoint_new_outputs(self.root/"new-window", self.root/"new-queue", [source])

    def test_external_source_pin_failure_precedes_any_module_execution(self):
        tools = Path(__file__).resolve().parent
        with mock.patch.object(B.importlib.util, "module_from_spec") as loader:
            with self.assertRaises(ValueError):
                B.load_protocol(tools, "0"*64, B.sha(tools/"paired_measurement.py"))
            loader.assert_not_called()

    def test_parent_proposal_has_no_approval_and_digest_is_external_environment(self):
        spec, jobs = self.spec()
        proposal = B.parent_proposal(self.N, spec, "1"*64, {"data/hash":dict(sha256="2"*64,bytes=5)}, "3"*64, "4"*64,
                                     spec["collector_execution_path"], 900)
        self.assertEqual(proposal["window_command"], [sys.executable, spec["collector_execution_path"], "collect-window", spec["execution_root"],
                                                       "--parent-approval-sha256-env", "VM_NETWORK_APPROVAL_SHA256"])
        self.assertEqual(proposal["status"], "ready_for_parent_review")
        self.assertFalse(proposal["native_execution_authorized"])
        self.assertNotIn("approval_sha256", proposal)
        self.assertFalse((self.root / "approval.json").exists())
        for timeout in (True, 0, float('inf'), 901):
            with self.assertRaises(ValueError):
                B.parent_proposal(self.N, spec, "1"*64, {}, "3"*64, "4"*64, spec["collector_execution_path"], timeout)
        with self.assertRaises(ValueError):
            B.parent_proposal(self.N, spec, "1"*64, {}, "3"*64, "4"*64, "/other/network_measurement.py", 900)

    def test_exact_finish_argv_uses_terminal_sibling_archive_then_seal_audit(self):
        spec, _ = self.spec()
        proposal=B.parent_proposal(self.N,spec,"1"*64,{},"2"*64,"3"*64,spec["collector_execution_path"],900)
        instructions=B.handoff_instructions(spec,proposal,spec["queue_runner_execution_path"],"4"*64,"5"*64)
        self.assertEqual(instructions["queue_run_argv"][-6:],proposal["window_command"])
        archive=instructions["archive_terminal_queue_argv"]
        self.assertIn("archive-queue",archive)
        self.assertEqual(archive[archive.index("--queue-source")+1],spec["queue_receipt_out"])
        self.assertIn("N.seal_native",instructions["seal_argv"][3])
        self.assertIn("N.mark_failed_native",instructions["failed_seal_argv"][3])
        self.assertEqual(instructions["audit_argv"][-2:],["--parent-approval-sha256","PARENT_ACCEPTED_APPROVAL_SHA256"])
        with self.assertRaises(ValueError):
            B.handoff_instructions(spec,proposal,"/other/run_job.py","4"*64,"5"*64)

    def test_fixture_proof_failure_or_wrong_build_never_fabricates_queue_acceptance(self):
        fixtures, queue = self.root/"fixtures", self.root/"queue"
        fixtures.mkdir()
        self.P.write_new(fixtures/"fixture-preparation.json", self.record())
        with mock.patch.object(self.N, "audit_fixture_preparation", return_value=dict(status="failed",timing_evidence=False)), \
             mock.patch.object(self.N, "validate_queue") as validate:
            with self.assertRaises(ValueError):
                B.verify_fixture_sources(self.N, fixtures, queue, self.record()["producer_identity"],"c"*64)
            validate.assert_not_called()
        with mock.patch.object(self.N, "audit_fixture_preparation", return_value=dict(status="fixture_preparation_audited",timing_evidence=False,functional_queue_verified=True)), \
             mock.patch.object(self.N, "validate_queue") as validate:
            wrong = {k:"b"*64 for k in self.N.PRODUCER_KEYS}
            with self.assertRaises(ValueError):
                B.verify_fixture_sources(self.N, fixtures, queue, wrong,"c"*64)
            validate.assert_not_called()

    def test_copy_rejects_symlink_and_does_not_replace_existing_evidence(self):
        source, target = self.root/"source", self.root/"copy"
        source.mkdir()
        (source/"a").write_bytes(b'original')
        B.copy_tree_checked(self.N, source, target)
        self.assertEqual(self.P.inventory(source), self.P.inventory(target))
        with self.assertRaises(FileExistsError):
            B.copy_tree_checked(self.N, source, target)
        (source/"link").symlink_to(source/"a")
        with self.assertRaises(ValueError):
            B.copy_tree_checked(self.N, source, self.root/"unsafe")
        self.assertFalse((self.root/"unsafe").exists())

    def write_archive(self, root, value):
        value = dict(value, schema_version=1, status="passed", timing_evidence=False)
        value["files"] = {k:e["sha256"] for k,e in self.P.inventory(root, ("result.json","COMPLETE")).items()}
        (root/"result.json").write_bytes(self.P.canonical(value))
        (root/"COMPLETE").write_text(B.sha(root/"result.json")+'\n')
        return value

    def archives(self):
        build, functional = self.root/"copied-build", self.root/"copied-functional"
        (build/"source").mkdir(parents=True)
        functional.mkdir()
        for name, data in (("net_reference.cpp",b'fake source'),("contract.json",b'fake contract'),("h.hpp",b'fake F header')):
            (build/"source"/name).write_bytes(data)
        (build/"net_reference").write_bytes(b'not executable')
        source = {k:e["sha256"] for k,e in self.P.inventory(build/"source").items()}
        self.P.write_new(build/"source-inventory.json",source)
        for name in ("toolchain.json","dependencies.json"):
            self.P.write_new(build/name,{"synthetic":True})
        build_result=self.write_archive(build,dict(kind="build",build_root="/original/accepted-build",measurement_profile="timing",sanitizer="none",
            contract_sha256=source["contract.json"],baseline_headers={"h.hpp":source["h.hpp"]},binary_sha256=B.sha(build/"net_reference"),
            commands=[dict(argv=["-O2","-DN7_NET_DIAGNOSTICS=0"])]))
        contract=dict(smoke=dict(case_ids=[f'case{i}' for i in range(34)]))
        self.P.write_new(functional/"smoke.stdout",dict(status="passed",diagnostic=False,timing_evidence=False,checks=34,
            schema_version=1,contract_version="1.0.0",command="smoke",
            case_ids=contract["smoke"]["case_ids"],owned_sockets_after_cases=0))
        self.write_archive(functional,dict(kind="smoke",binary_sha256=B.sha(build/"net_reference"),build_result_sha256=B.sha(build/"result.json"),
            executed_build_root="/original/accepted-build",measurement_profile="timing",sanitizer="none"))
        producer=dict(binary_sha256=B.sha(build/"net_reference"),source_inventory_sha256=B.sha(build/"source-inventory.json"),
            toolchain_sha256=B.sha(build/"toolchain.json"),dependencies_sha256=B.sha(build/"dependencies.json"))
        pins=dict(G1_BUILD_SHA=B.sha(build/"result.json"),G1_SOURCE_SHA=source["net_reference.cpp"],CONTRACT_SHA=source["contract.json"],G1_BINARY_SHA=producer["binary_sha256"])
        return build,functional,contract,producer,pins,source

    def accepted_members(self,build,functional):
        return dict(manifest_sha256="a"*64,
            build=dict(path=B.W1_BUILD_RESULT_MEMBER,**B.identity(build/"result.json")),
            functional=dict(path=B.W1_FUNCTIONAL_RESULT_MEMBER,**B.identity(functional/"result.json")))

    def test_portable_w1_archive_review_uses_recorded_original_path_without_reopening(self):
        build,functional,contract,producer,pins,source=self.archives()
        with mock.patch.multiple(self.N,**pins), mock.patch.object(self.P,"BASELINE",{"h.hpp":source["h.hpp"]}):
            result=B.validate_w1_archives(self.N,build,functional,contract,producer,self.accepted_members(build,functional),"/original/accepted-build")
        self.assertEqual(result[0]["build_root"],"/original/accepted-build")
        self.assertFalse(Path('/original/accepted-build').exists())

    def test_wrong_native_binary_source_build_or_smoke_lineage_rejected(self):
        build,functional,contract,producer,pins,source=self.archives()
        accepted=self.accepted_members(build,functional)
        with mock.patch.multiple(self.N,**pins), mock.patch.object(self.P,"BASELINE",{"h.hpp":source["h.hpp"]}):
            for altered in ({"G1_BINARY_SHA":"f"*64},{"G1_SOURCE_SHA":"f"*64},{"G1_BUILD_SHA":"f"*64}):
                with mock.patch.multiple(self.N,**altered), self.assertRaises(ValueError):
                    B.validate_w1_archives(self.N,build,functional,contract,producer,accepted,"/original/accepted-build")
            result=self.P.read(functional/"result.json")
            result["build_result_sha256"]="f"*64
            self.write_archive(functional,result)
            with self.assertRaises(ValueError):
                B.validate_w1_archives(self.N,build,functional,contract,producer,accepted,"/original/accepted-build")

    def test_g1_manifest_chain_pins_exact_build_and_functional_members(self):
        manifest=self.root/"accepted-manifest.json"
        g1=self.root/"G1.json"
        document=dict(schema_version=1,members={
            B.W1_BUILD_RESULT_MEMBER:dict(sha256="b"*64,bytes=100),
            B.W1_FUNCTIONAL_RESULT_MEMBER:dict(sha256="f"*64,bytes=200)})
        self.P.write_new(manifest,document)
        self.P.write_new(g1,dict(evidence_sha256={B.W1_MANIFEST_MEMBER:B.sha(manifest)}))
        with mock.patch.multiple(self.N,G1_SHA=B.sha(g1),G1_BUILD_SHA="b"*64):
            accepted=B.accepted_w1_members(self.N,g1,manifest)
            self.assertEqual(accepted["functional"],dict(path=B.W1_FUNCTIONAL_RESULT_MEMBER,sha256="f"*64,bytes=200))
            (manifest).write_bytes(self.P.canonical(dict(document,schema_version=True)))
            with self.assertRaises(ValueError):
                B.accepted_w1_members(self.N,g1,manifest)
        for mutate in (lambda d:d["members"].pop(B.W1_FUNCTIONAL_RESULT_MEMBER),
                       lambda d:d["members"][B.W1_FUNCTIONAL_RESULT_MEMBER].update(bytes=True),
                       lambda d:d["members"][B.W1_BUILD_RESULT_MEMBER].update(sha256="0"*64)):
            altered=copy.deepcopy(document)
            mutate(altered)
            manifest.write_bytes(self.P.canonical(altered))
            g1.write_bytes(self.P.canonical(dict(evidence_sha256={B.W1_MANIFEST_MEMBER:B.sha(manifest)})))
            with mock.patch.multiple(self.N,G1_SHA=B.sha(g1),G1_BUILD_SHA="b"*64), self.assertRaises((ValueError,KeyError)):
                B.accepted_w1_members(self.N,g1,manifest)

    def test_resealed_synthetic_smoke_cannot_replace_parent_accepted_result(self):
        build,functional,contract,producer,pins,source=self.archives()
        accepted=self.accepted_members(build,functional)
        raw=self.P.read(functional/"smoke.stdout")
        raw.update(schema_version=True,command="fixture")
        (functional/"smoke.stdout").write_bytes(self.P.canonical(raw))
        self.write_archive(functional,self.P.read(functional/"result.json"))
        with mock.patch.multiple(self.N,**pins), mock.patch.object(self.P,"BASELINE",{"h.hpp":source["h.hpp"]}):
            with self.assertRaisesRegex(ValueError,"G1-manifest accepted member"):
                B.validate_w1_archives(self.N,build,functional,contract,producer,accepted,"/original/accepted-build")
            # Even a newly pinned synthetic fixture must obey raw schema and
            # command. This does not change any real acceptance or evidence.
            with self.assertRaisesRegex(ValueError,"smoke cases/profile"):
                B.validate_w1_archives(self.N,build,functional,contract,producer,self.accepted_members(build,functional),"/original/accepted-build")

    def test_archive_boolean_schema_failed_status_and_raw_tamper_rejected(self):
        root=self.root/"archive"
        root.mkdir()
        (root/"raw").write_bytes(b'raw')
        value=self.write_archive(root,dict(kind="smoke"))
        B.archive_result(self.N,root,"smoke")
        for key,bad in (("schema_version",True),("status","failed"),("timing_evidence",True)):
            changed=dict(value,**{key:bad})
            (root/"result.json").write_bytes(self.P.canonical(changed))
            (root/"COMPLETE").write_text(B.sha(root/"result.json")+'\n')
            with self.assertRaises(ValueError):
                B.archive_result(self.N,root,"smoke")
        (root/"result.json").write_bytes(self.P.canonical(value))
        (root/"COMPLETE").write_text(B.sha(root/"result.json")+'\n')
        (root/"raw").write_bytes(b'tamper')
        with self.assertRaises(ValueError):
            B.archive_result(self.N,root,"smoke")

    def queue_archive_setup(self):
        spec,_=self.spec()
        window=Path(spec["execution_root"])
        window.mkdir()
        (window/"protocol").mkdir()
        self.P.write_new(window/"preregister.json",spec)
        queue=Path(spec["queue_receipt_out"])
        queue.mkdir()
        self.P.write_new(queue/"manifest.json",{"synthetic":"terminal queue only; no native calls"})
        gate=dict(accepted_inputs={"protocol/bind_network_window.py":dict(sha256=B.sha(B.__file__))})
        return window,queue,spec,gate

    def test_archive_queue_external_approval_checked_before_any_copy(self):
        window,queue,spec,gate=self.queue_archive_setup()
        with mock.patch.object(B,"load_protocol",return_value=self.N), \
             mock.patch.object(self.N,"validate_parent_gate",side_effect=ValueError("wrong external approval digest")), \
             mock.patch.object(B,"copy_tree_checked") as copy:
            with self.assertRaises(ValueError):
                B.archive_queue(window,queue,"a"*64,"b"*64,"bad")
            copy.assert_not_called()
        self.assertFalse((window/"queue").exists())

    def test_archive_queue_unique_destination_and_terminal_metadata(self):
        window,queue,spec,gate=self.queue_archive_setup()
        with mock.patch.object(B,"load_protocol",return_value=self.N), \
             mock.patch.object(self.N,"validate_parent_gate",return_value=gate), \
             mock.patch.object(self.N,"validate_queue",return_value=dict(run_id="synthetic",job_status="complete")) as validate:
            result=B.archive_queue(window,queue,"a"*64,"b"*64,"c"*64)
            self.assertEqual(result["status"],"terminal_queue_archived")
            self.assertEqual(self.P.inventory(queue),self.P.inventory(window/"queue"))
            with self.assertRaises(ValueError):
                B.archive_queue(window,queue,"a"*64,"b"*64,"c"*64)
            self.assertEqual(validate.call_count,2)

    def test_archive_queue_changed_or_copy_failure_retains_partial_bytes(self):
        window,queue,spec,gate=self.queue_archive_setup()
        with mock.patch.object(B,"load_protocol",return_value=self.N), \
             mock.patch.object(self.N,"validate_parent_gate",return_value=gate), \
             mock.patch.object(self.N,"validate_queue",side_effect=[dict(run_id="before"),dict(run_id="changed")]):
            with self.assertRaises(ValueError):
                B.archive_queue(window,queue,"a"*64,"b"*64,"c"*64)
        self.assertTrue((window/"queue/manifest.json").is_file())
        self.assertFalse((window/"result.json").exists())
        shutil.rmtree(window/"queue")
        def partial_copy(N,source,target):
            target.mkdir()
            (target/"partial").write_bytes(b'preserve failed copy')
            raise OSError("synthetic interrupted copy")
        with mock.patch.object(B,"load_protocol",return_value=self.N), \
             mock.patch.object(self.N,"validate_parent_gate",return_value=gate), \
             mock.patch.object(self.N,"validate_queue",return_value=dict(run_id="before")), \
             mock.patch.object(B,"copy_tree_checked",side_effect=partial_copy):
            with self.assertRaises(OSError):
                B.archive_queue(window,queue,"a"*64,"b"*64,"c"*64)
        self.assertEqual((window/"queue/partial").read_bytes(),b'preserve failed copy')
        self.assertFalse((window/"COMPLETE").exists())


if __name__ == "__main__":
    unittest.main()
