"""One source-only, byte-identical WSL input copy; no executable benchmark action."""
import importlib.util
from pathlib import Path
import shutil
import time

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('phase_c', HERE / 'diag/phase_c_job.py')
C = importlib.util.module_from_spec(spec)
spec.loader.exec_module(C)
OUT = Path('/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1')
DOC = C.DOC / 'validation/screen'


def main():
    start = time.monotonic_ns()
    freeze = C.DOC / 'validation/diag/C_FREEZE_DRAFT.json'
    C.need(C.fid(freeze)['sha256'] == '250ce7daf7e1fb20513c8977928524c008aa6079211dbb66b9f5ef7efc4c7f0c', 'C source freeze changed')
    f = C.read(freeze)
    fixture, wire = Path(f['fixture']), Path(f['wire'])
    sources = [(p, OUT / 'fixed-M' / p.relative_to(fixture)) for p in sorted(fixture.rglob('*')) if p.is_file()]
    sources.append((wire, OUT / 'initial-wire.bin'))
    C.need(not OUT.exists() and sources, 'one fresh staging attempt required')
    C.need(sum(p.stat().st_size for p, _ in sources) <= 256 * 1024**2, 'fixed source-only copy budget256MiB exceeded')
    for p, _ in sources:
        C.need(not p.is_symlink() and C.fid(p) == f['fixed_files'][str(p)], 'frozen input source differs: ' + str(p))
    OUT.mkdir()
    rows = []
    for p, target in sources:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, target)
        C.need(C.fid(p) == C.fid(target) == f['fixed_files'][str(p)], 'byte-identical copy failed')
        rows.append(dict(source=str(p), target=str(target), identity=C.fid(target)))
    result = dict(status='staged_pending_screen_policy_admission', files=rows, fixture=str(OUT / 'fixed-M'), wire=str(OUT / 'initial-wire.bin'),
        file_count=len(rows), bytes=sum(v['identity']['bytes'] for v in rows), actual_allocated_bytes=C.allocated(OUT),
        elapsed_seconds=(time.monotonic_ns()-start)/1e9, tool=dict(path=str(Path(__file__)), identity=C.fid(__file__)),
        freeze=dict(path=str(freeze), identity=C.fid(freeze)), new_compiles=0, new_native=0, new_timing_samples=0,
        policy='Same byte-identical staged fixture/wire for all roles; API/model/trace unchanged. Charge this staging cost in engineering admission/projection. Requires independent final screen-policy/freeze admission.',
        evidence='C exact boundary input.read interval C14=887.414ms,R1=1101.559ms; no historical M1 cause inferred; no native preview was performed.')
    C.write(DOC / 'INPUT_STAGING_RESULT.json', result)
    print('Source-only input copy complete; all bytes identical; no native; screen input policy still unadmitted')


if __name__ == '__main__':
    main()
