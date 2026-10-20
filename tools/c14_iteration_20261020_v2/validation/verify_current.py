"""Finite v2 continuation check; no build, test/native, or timing execution."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[3]
DOC=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation'
PRIOR=ROOT/'docs/coordination/c14-iteration-20261019-v1/validation/S0_RESULT.json'
OLD=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
OUTPUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation')

def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def fid(p):
    p=Path(p);h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return dict(bytes=p.stat().st_size,sha256=h.hexdigest())
def main():
    before=read(PRIOR);material=read(ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    host=read(DOC/'windows-current.json')
    actual={n:fid(ROOT/n) for n in material['non_external_files']}
    files={n:{'expected':v['actual'],'actual':fid(n)['sha256']} for n,v in before['toolchain']['files'].items()}
    sanroot=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/sanitizer-resource')
    san={n:fid(sanroot/n) for n in before['toolchain']['sanitizer_files']}
    runner=OLD/'experiments/vm_coordination/run_job.py'
    spec=importlib.util.spec_from_file_location('original_run_job',runner)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    lease=module.status(OLD/'build/vm-coordination')
    checks={'baseline154':len(actual)==154 and actual==material['non_external_files'],
        'production14':len(material['production_files'])==14 and all(actual[n]==v for n,v in material['production_files'].items()),
        'HEAD':host['head']=='1904b9c71230026ac6f2d58f919a9ab76447d264',
        'gitlinks':all(any(x.startswith('160000 '+v+' ') and x.endswith('\t'+n) for x in host['gitlinks']) for n,v in material['gitlinks'].items()),
        'fixed_ABI_compiler_runtime':all(x['actual']==x['expected'] for x in files.values()),
        'compatible_SAN_resource':san==before['toolchain']['sanitizer_files'],
        'original_runner':fid(runner)['sha256']=='6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909',
        'guest_reserve':shutil.disk_usage('/root').free>=8*1024**3,
        'backing_reserve':host['backing_free_bytes']>=8*1024**3}
    result=dict(schema_version=1,status='passed' if all(checks.values()) else 'rejected',
        stage='v2_finite_continuation',observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        checks=checks,root=str(ROOT),baseline_commit=host['head'],root_members_count=154,
        production_files=material['production_files'],windows_host=host,prior_S0=dict(path=str(PRIOR),identity=fid(PRIOR)),
        toolchain=dict(files=files,sanitizer_files=san,abi_conclusion='All retained actual compiler/loader/glibc/libc++/libc++abi/SAN byte identities unchanged; no build or runtime ABI probe.'),
        lease=lease,runner=dict(path=str(runner),identity=fid(runner)),boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
        memory=Path('/proc/meminfo').read_text(),cpu_affinity=sorted(os.sched_getaffinity(0)),
        disk={p:dict(zip(('total','used','free'),shutil.disk_usage(p))) for p in ('/root','/mnt/c')},output=str(OUTPUT),
        dependency_reuse='Exact v1 locked provider reused; each new candidate snapshot configure rechecks original dependency content locks.',
        reuse_scope='Same-SHA C14 control codegen/function receipts only; R1 changed code requires its own candidate gates.',
        new_CPP_compiles=0,new_native_executions=0,new_timing_samples=0,platform_test=False,
        M1_v1_window_closed=True,R1_codegen_recompiled=False,screen_or_QF_admitted=False,
        no_pause_power_or_old_PID_reuse=True)
    with (DOC/'S0_RESULT.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
    print(json.dumps(dict(status=result['status'],checks=checks,lease=lease)))
    return 0 if result['status']=='passed' else 1
if __name__=='__main__':raise SystemExit(main())
