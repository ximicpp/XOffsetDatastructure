#!/usr/bin/env python3
"""Seal the fixed D018/D022 delivery; run through the default queue."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tarfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
RUN = ROOT / "build/vm-owner-scaling"


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            h.update(block)
    return h.hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("owner_package_base", RUN / "build-v4/common/run_owner_scaling.py")
    base = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(base)
    permission = base.coordinated(ROOT)
    out, archive = args.out.resolve(), args.archive.resolve()
    assert out.is_relative_to(ROOT / "docs/validation/vm-owner-scaling")
    assert archive.is_relative_to(ROOT / "docs/validation/vm-owner-scaling")
    assert not out.exists() and not archive.exists()
    audited = read(RUN / "final-audit-v1.json")
    assert audited["status"] == "passed" and audited["workload_processes"] == 26
    sources = {name: RUN / name for name in ("build-v4", "tooling-errata-v2", "recovery-v2", "collection-v1", "attempts-d018",
               "python-tests-v1", "errata-tests-v1", "final-audit-v1.json", "run-plan-v1.json", "remaining-dispatch-v1.json")}
    sources.update({"history/" + name: RUN / name for name in
                    [*[f"prepared-v{i}" for i in range(1, 7)], "build-v1", "build-v2", "build-v3", "tooling-errata-v1", "recovery-v1"]})
    sources.update({"receipts/" + name: RUN / "receipts" / name for name in
                    [*[f"build-v{i}" for i in range(1, 5)], *[f"group-{i:02d}" for i in range(9)], "final-audit-v1"]})
    active = ROOT / "experiments/vm_owner_scaling"
    sources.update({"delivery-source/" + p.name: p for p in active.iterdir() if p.is_file() and p.suffix in (".py", ".json", ".md")
                    and p.name not in ("handoff.json", "HANDOFF.md")})
    sources["LICENSE"] = ROOT / "LICENSE"
    size = sum(p.stat().st_size for source in sources.values() for p in
               (source.rglob("*") if source.is_dir() else [source]) if p.is_file())
    base.evidence_size(ROOT, 2 * size + 4194304)
    out.mkdir(parents=True)
    for name, source in sources.items():
        target = out / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, target)
        else:
            shutil.copy2(source, target)
    reports = {}
    for group in (out / "collection-v1").iterdir():
        for path in group.glob("*.stdout"):
            reports[path.stem] = read(path)
    assert set(reports) == set(base.cases()) and len(list((out / "attempts-d018").glob("*.json"))) == 26
    rows = []
    for c, t in base.CONDITIONS:
        report = reports[f"diagnostic-r256-o{c}-t{t}"]
        kinds = Counter(e["kind"] for e in report["trace"]["events"])
        rows.append({"owners": c, "threads": t, "requests": report["counts"]["allocated"],
                     "events": len(report["trace"]["events"]), "lookup_hits": kinds["registry_lookup_hit"],
                     "registry_locks": kinds["registry_lock"], "metadata_snapshots": kinds["core_metadata_capacity"],
                     "final_shared_bucket_bytes": {8: 88, 64: 776, 256: 3176}[c]})
    summary = {"schema_version": 1, "status": "passed", "decisions": ["D018", "D022"], "timing_evidence": False,
               "workload_processes": 26, "successful_processes": 18, "expected_failure_processes": 8,
               "first_case_retried": False, "recompiled_after_D022": False, "diagnostic_conditions": rows,
               "binaries": audited["binaries"], "checker_sha256": audited["checker_sha256"],
               "first_original_queue_status": "failed", "first_new_offline_audit": "passed",
               "synthetic_tests_per_mode": 13, "negative_checks": audited["negative_checks"], "packaging_coordination": permission}
    write(out / "summary.json", summary)
    table = "\n".join(f"| {r['owners']} | {r['threads']} | {r['events']} | {r['lookup_hits']} | {r['registry_locks']} | {r['metadata_snapshots']} | {r['final_shared_bucket_bytes']} |" for r in rows)
    document = f"""# Fixed-request owner scaling：功能与诊断结果

2026-10-16，D018 / D022。已完成26个原生 workload：18个成功样本（6 diagnostic/R256、6原始F/R256语义参照、6原始F/R8192容量功能检查），及两构建各4类精确失败控制。13项Python合成测试在正常与`-O`模式通过；原始记录的3项离线坏副本均拒绝。没有正式计时或性能结论。

每个条件固定全进程总请求数，owner i始终属于worker i%T。直接使用Core与uint64 allocator；四阶段create/allocate/free/destroy，全owner创建后才分配、全请求释放后才销毁。填值、逐值校验、对齐/范围/不重叠、free守恒、线程启动和输出在四段时钟外。功能记录的wall值包含barrier及调度，全部标记timing_evidence=false，不是单次registry lookup延迟。正式计时只由W3另行执行。

| owners | workers | 完整events | lookup hit | registry lock | metadata snapshot | 唯一末桶bytes |
|---:|---:|---:|---:|---:|---:|---:|
{table}

每条诊断均有256成功allocate与256对应free；register/erase各C、active峰C/结束0，完整generation、phase和固定worker归属通过审计。Core new/return各256，reserve/enable/grow_zero/unmap各C，zero bytes=65536*C。无scan/reuse、extent、pool、wire、growth或失败事件。domain1 vector分配/释放与容量序列完整且最终归零；domain2节点全归还，只保留表中一个共享registry bucket，按完整分配lineage核验。lock wait/hold只作受扰动诊断，不加总冒充阶段wall。

D022更正草稿检查器对齐假设：本Apple arm64 profile有效alignment=8，用户地址base+64+24*j，Q次后used_end=48+24*Q。最大Q1024时used_end24624，低于每owner逻辑/最大65536。harness本身使用真实alignof/sizeof及守恒检查，二进制无需重编。首份原生记录退出0、1981events，其原收集器退出1及failed队列回执保持原样；修正审计器单独冻结并离线补审该记录，然后只执行剩余25个唯一case，首例没有重跑。

失败控制精确覆盖部分线程启动（2个已起线程join、零Core）、第4个Core前失败（3创建/销毁）、第8个allocate前失败（7分配/清理free、8销毁）、主线程值核验失败（validated7、256free、8销毁）。均退出2、精确原因匹配、清理/join完成，不计为成功样本。另保留3次构建失败（依赖路径别名检查两次、引入的8MiB编译文件限额一次）和一次零原生进程的Python预检kind错误，未隐去或改写为通过。

生产F双头及已接受W2生成头/runtime未改；实际传递闭包为timing1018、diagnostic1019文件，证明诊断头未被F遮蔽。保持/usr/local/bin/clang++执行拼写，冻结flags/compiler/SDK/stdlib/unwind/link工具与源文件身份。W2 recorder仍为16 lifetime线程槽、每槽16384events，无reset/采样/截尾；最大实际整进程5135events，低于5631硬上限。

证据目录：`docs/validation/vm-owner-scaling/final-v1/`。`final-audit-v1.json`核验26个case/claim恰好覆盖、两profile同R checksum一致、原首例补充审计、原/新工具身份分离及完整队列回执。`recovery-v2/`明确历史队列failed与新离线passed的区别；`history/`保留原准备/构建/修正工具版本；`receipts/`包含所有构建/收集失败与成功。manifest与归档提供文件完整性校验，不是签名认证。
"""
    (ROOT / "docs/VM_OWNER_SCALING_RESULTS.md").write_text(document)
    (out / "RESULTS.md").write_text(document)
    (out / "README.md").write_text("D018/D022固定owner-scaling证据。审计入口为 tooling-errata-v2/continue_owner_scaling.py audit；参阅RESULTS.md。原始路径/argv作为历史身份保留，离线审计不执行C++，不要求原SDK。\n")
    files = base.inventory(out)
    write(out / "manifest.json", {"schema_version": 1, "files": files})
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(out, arcname=out.name)
    seal = {"schema_version": 1, "status": "sealed", "archive": str(archive), "archive_sha256": sha(archive),
            "archive_bytes": archive.stat().st_size, "manifest_sha256": sha(out / "manifest.json"),
            "regular_files": len(files) + 1, "evidence_bytes_at_seal": base.evidence_size(ROOT)}
    write(archive.with_suffix(".seal.json"), seal)
    print(json.dumps(seal, sort_keys=True))


if __name__ == "__main__":
    main()
