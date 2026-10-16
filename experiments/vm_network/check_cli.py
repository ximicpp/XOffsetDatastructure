#!/usr/bin/env python3
"""Bounded functional CLI checks; execute through the shared queue, never timing."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("network_runner", HERE / "run_network.py")
R = importlib.util.module_from_spec(spec)
spec.loader.exec_module(R)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--timing-build", type=Path, required=True)
    p.add_argument("--diagnostic-build", type=Path, required=True)
    p.add_argument("--reference-wire", type=Path, required=True)
    p.add_argument("--reference-output", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--coordinated-execution", action="store_true")
    p.add_argument("--queue-lock", type=Path, default=HERE.parents[1] / "build/vm-coordination/queue.lock")
    a = p.parse_args()
    coordination = R.coordinated(a)
    out = a.out.absolute()
    R.require(not out.exists(), "new CLI evidence directory required")
    out.mkdir(parents=True)
    shutil.copy2(__file__, out / "check_cli.py")
    rows, outputs = [], []
    reference = a.reference_wire.read_bytes()
    reference_output = a.reference_output.read_bytes()
    (out / "reference-input.wire").write_bytes(reference)
    (out / "reference-output.wire").write_bytes(reference_output)
    phase_keys = ["request_send_ns", "server_receive_ns", "server_load_verified_ns", "server_process_ns",
                  "server_save_verified_ns", "server_owner_release_ns", "response_send_ns", "response_receive_ns",
                  "client_load_verified_ns", "client_validate_ns"]
    for profile, build in (("timing", a.timing_build.absolute()), ("diagnostic", a.diagnostic_build.absolute())):
        parent = R.audit(build, "build")
        R.require(parent["measurement_profile"] == profile and parent["sanitizer"] == "none", "wrong CLI build profile")
        binary = build / "net_reference"
        contract = R.contract(build / "source/contract.json")
        R.require(R.sha(binary) == parent["binary_sha256"], "CLI binary changed")
        shutil.copy2(build / "result.json", out / (profile + "-build-result.json"))

        def execute(name, args, expected_code=0, reason=None):
            argv = [str(binary), *map(str, args)]
            run = subprocess.run(argv, capture_output=True, text=True, timeout=30)
            (out / (name + ".stdout")).write_text(run.stdout)
            (out / (name + ".stderr")).write_text(run.stderr)
            R.require(run.returncode == expected_code and run.stderr == "", name + ": exit/stderr differs")
            report = R.parse_binary_stdout(run.stdout, contract, args[0], profile == "diagnostic", expected_code)
            if reason:
                R.require(report["reason"] == reason, name + ": wrong failure category")
            rows.append({"id": name, "argv": argv, "returncode": run.returncode,
                         "stdout_sha256": R.sha(out / (name + ".stdout")), "reason": report.get("reason", "ok")})
            return report

        fixture = out / (profile + "-input.wire")
        made = execute(profile + ".fixture", ["fixture", "--output", fixture, "--elements", "257"])
        R.require(fixture.read_bytes() == reference and made["frame_bytes"] == len(reference), "fixture drift from file reference")
        for mode in ("none", "cold", "warm"):
            directory = out / (profile + "-" + mode)
            base = ["run", "--input", fixture, "--out-dir", directory, "--pool", mode, "--iterations", "2",
                    "--max-wire", "1048576", "--owner-capacity", "1048576", "--max-inflight-bytes", "4194304", "--timeout-ms", "2000"]
            report = execute(profile + ".run." + mode, base)
            R.require(set(contract["json"]["run"]) - {"exact_arrays"} <= set(report), "missing run fields")
            R.require(report["iterations"] == len(report["samples"]) == len(report["output_files"]) == 2, "run array count")
            R.require(report["end_to_end_ns"] == sum(s["end_to_end_ns"] for s in report["samples"]), "main total differs")
            R.require(report["pool_measured_hits"] == (4 if mode == "warm" else 0) and
                      report["pool_measured_misses"] == (4 if mode == "cold" else 0), "pool counters differ")
            for i, (sample, output) in enumerate(zip(report["samples"], report["output_files"])):
                R.require(set(contract["json"]["sample"]) <= set(sample), "missing sample field")
                R.require(output == f"output-{i}.wire", "output ordinal differs")
                R.require((directory / output).read_bytes() == reference_output, "processed wire differs from file reference")
                R.require(sample["input_bytes"] == len(reference) and sample["output_bytes"] == len(reference_output), "wire counters differ")
                for key in phase_keys:
                    R.require(type(sample[key]) is int and sample[key] >= 0 if profile == "diagnostic" else sample[key] is None,
                              "phase instrumentation differs: " + key)
                outputs.append({"profile": profile, "pool": mode, "iteration": i, "sha256": R.sha(directory / output)})
        negative = ["run", "--input", fixture, "--out-dir", out / (profile + "-must-not-exist"), "--pool", "none", "--iterations", "1",
                    "--max-wire", "1048576", "--owner-capacity", "1048576", "--max-inflight-bytes", "4194304", "--timeout-ms", "2000"]
        for suffix, key, value in (("wire_limit", "--max-wire", "4095"), ("owner_limit", "--owner-capacity", "4095"),
                                   ("inflight_limit", "--max-inflight-bytes", "4194303"), ("deadline_limit", "--timeout-ms", "0")):
            args = list(negative)
            args[args.index(key) + 1] = value
            execute(profile + ".reject." + suffix, args, 2, "precondition")
            R.require(not Path(args[args.index("--out-dir") + 1]).exists(), "bad CLI allocated output directory")
        R.require(R.sha(binary) == parent["binary_sha256"], "CLI binary changed during checks")
    result = {"schema_version": 1, "status": "passed", "scope": "bounded functional CLI checks; no performance evidence",
              "checks": len(rows), "commands": rows, "output_equivalence": outputs, "coordination": coordination,
              "reference_input_sha256": hashlib.sha256(reference).hexdigest(),
              "reference_output_sha256": hashlib.sha256(reference_output).hexdigest(),
              "contract_sha256": R.sha(a.timing_build / "source/contract.json")}
    R.write(out / "result.json", result)
    R.write(out / "manifest.json", R.files(out))
    print(json.dumps({"status": "passed", "checks": len(rows), "outputs_compared": len(outputs)}))


if __name__ == "__main__":
    main()
