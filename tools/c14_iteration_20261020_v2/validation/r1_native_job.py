"""Admit and dispatch only the exact reviewed five normal + five SAN R1 tests."""
import argparse
import datetime
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('current',H/'verify_current.py')
V=importlib.util.module_from_spec(s);s.loader.exec_module(V)
ROOT=V.ROOT;D=V.DOC;OUT=V.OUTPUT/'r1-native-v2';LEASE=V.OUTPUT/'r1-native-v2-lease'
QPATH=ROOT/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_R1_NATIVE_v2.json'
SOURCE=Path('/root/xoffset-c14-iteration/20261019/v1/validation/r1-v1/source')
RUNNER=V.OLD/'experiments/vm_coordination/run_job.py'
WORKER=ROOT/'tools/c14_iteration_20261020_v2/core/run_native_targets.py'
P1=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile')
VENDOR=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
NAMES=('test_canonical_zero_target','test_canonical_cold_no_callback','test_canonical_registry_v2','test_owner_identity','test_r1_thread_handoff')
def need(x,m):
    if not x:raise ValueError(m)
def matches(actual,registered):
    return actual['sha256']==registered['sha256'] and ('bytes' not in registered or actual['bytes']==registered['bytes'])
def write(p,x):
    with Path(p).open('x',encoding='utf-8') as f:json.dump(x,f,indent=2);f.write('\n')
def members(q):return {n:V.fid(SOURCE/n) for n in q['source_members']}
def verify():
    q=V.read(QPATH);a=V.read(D/'A_RESULT.json');s0=V.read(D/'S0_RESULT.json')
    need(a['status']==s0['status']=='passed','A prerequisite')
    need(q['status']=='ready_for_validation_review' and q['stage']=='B_R1_targeted' and q['candidate']=='r1-canonical-hotcold-v1','wrong registration')
    need(q['source']==str(SOURCE) and q['output']==str(OUT) and q['global_lease_file']==str(V.OLD/'build/vm-coordination/queue.lock'),'source/output/global lease differs')
    need(q['task_timeout_seconds']==1500 and q['compile_budget']==q['native_budget']==10 and len(q['commands'])==20,'finite command budget differs')
    need(q['timing_admitted'] is False and q['screening_admitted'] is False,'native request cannot grant timing')
    ident=V.read(ROOT/'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json')
    need(q['source_members']==ident['members'] and len(q['source_members'])==154,'complete154 identity differs')
    actual=members(q);need(actual=={n:dict(bytes=v['bytes'],sha256=v['candidate_sha256']) for n,v in q['source_members'].items()},'actual candidate source differs')
    need(actual['xoffsetdatastructure.hpp']['sha256']==q['production_main_sha256']=='b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f','R1 main differs')
    for p,v in q['input_files'].items():need(matches(V.fid(p),v),'input changed: '+p)
    for p,v in s0['toolchain']['files'].items():need(V.fid(p)['sha256']==v['actual'],'fixed ABI/tool changed: '+p)
    includes=['external/typelayout/include']+['external/boost/libs/'+n+'/include' for n in ('config','container','container_hash','core','intrusive','predef','static_assert','throw_exception','type_traits','winapi')]
    base=[str(P1/'bin/clang++-libcxx'),'-std=gnu++26','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread','-O3','-DNDEBUG']+['-I'+str(VENDOR/n) for n in includes]+['-UNDEBUG','-I'+str(SOURCE),'-I'+str(SOURCE/'tests')]
    expected=[]
    for mode in ('normal','san'):
        flags=[] if mode=='normal' else ['-resource-dir='+str(P1/'sanitizer-resource'),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
        for name in NAMES:
            src=ROOT/'tools/c14_iteration_20261020_v2/core/test_r1_thread_handoff_v2.cpp' if name=='test_r1_thread_handoff' else SOURCE/'tests'/(name+'.cpp')
            binary=OUT/(mode+'-'+name)
            suffix='-v2' if name=='test_r1_thread_handoff' else ''
            expected += [dict(label=mode+'-'+name+'-compile'+suffix,kind='compile',timeout_seconds=120,argv=base+flags+[str(src),'-o',str(binary)]),dict(label=mode+'-'+name+'-run'+suffix,kind='native',timeout_seconds=20,argv=[str(binary)])]
    need(q['commands']==expected,'actual command closure differs from finite5x2 tests')
    need(q['worker_argv']==['python3','-I','-B',str(WORKER),'--request',str(QPATH)],'worker argv differs')
    need(V.fid(WORKER)==q['input_files'][str(WORKER)],'worker not pinned')
    review=V.read(D/'review/R1_NATIVE_REVIEW.json')
    need(review['status']=='passed' and review['request_sha256']==V.fid(QPATH)['sha256'],'exact independent request review required')
    core_review=ROOT/'docs/coordination/c14-iteration-20261020-v2/core/REVIEW_R1_NATIVE.md'
    need(core_review.is_file(),'core test/source review missing')
    need(review['core_review']['path']==str(core_review) and matches(V.fid(core_review),review['core_review']['identity']),'exact core review identity changed')
    host=V.read(D/'windows-native-admission.json');age=(datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(host['observed_utc'].replace('Z','+00:00'))).total_seconds()
    need(0<=age<=900 and host['backing_free_bytes']>=8*1024**3 and shutil.disk_usage('/root').free>=8*1024**3,'actual fresh guest/backing reserve required')
    need(not OUT.exists() and not LEASE.exists(),'fresh one-shot output required')
    need(V.fid(RUNNER)['sha256']=='6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909','original runner changed')
    return q,actual,review,core_review
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('check','run'));args=p.parse_args()
    q,source_before,review,core_review=verify()
    cmd=['/usr/bin/python3','-I','-B',str(RUNNER),'run','--owner','W1','--kind','validation','--out',str(LEASE),'--timeout-seconds','1500','--wait-seconds','0','--',*q['worker_argv']]
    if args.action=='check':print(json.dumps(dict(status='ready',source154=len(source_before),compile_budget=10,native_budget=10,argv=cmd)));return
    V.OUTPUT.mkdir(parents=True,exist_ok=True)
    admission=dict(status='passed',request=dict(path=str(QPATH),identity=V.fid(QPATH)),review=dict(path=str(D/'review/R1_NATIVE_REVIEW.json'),identity=V.fid(D/'review/R1_NATIVE_REVIEW.json')),core_review=dict(path=str(core_review),identity=V.fid(core_review)),host_resource_observation=dict(path=str(D/'windows-native-admission.json'),identity=V.fid(D/'windows-native-admission.json')),source_before=source_before,argv=cmd,resource_reservation_bytes=8*1024**3,new_timing_samples=0)
    write(D/'R1_NATIVE_ADMISSION.json',admission)
    r=subprocess.run(cmd,cwd=ROOT,env=dict(os.environ,PATH='/usr/bin:/bin'))
    queue=V.read(LEASE/'result.json');receipt=V.read(OUT/'receipt.json') if (OUT/'receipt.json').is_file() else None
    after=members(q);inputs_after={n:V.fid(n) for n in q['input_files']}
    passed=r.returncode==0 and queue['status']=='complete' and queue['command_returncode']==0 and all(not v for v in queue['cleanup'].values()) and receipt and receipt['status']=='passed' and len(receipt['commands'])==20 and all(x.get('returncode')==0 for x in receipt['commands']) and after==source_before and all(matches(inputs_after[n],v) for n,v in q['input_files'].items())
    write(D/'R1_NATIVE_RESULT.json',dict(status='passed' if passed else 'failed',candidate=q['candidate'],request_identity=V.fid(QPATH),admission_identity=V.fid(D/'R1_NATIVE_ADMISSION.json'),returncode=r.returncode,receipt=dict(path=str(OUT/'receipt.json'),identity=V.fid(OUT/'receipt.json')) if receipt else None,queue_result=queue,source154_before=source_before,source154_after=after,actual_compiles=sum(x['kind']=='compile' for x in receipt['commands']) if receipt else 0,actual_native_executions=sum(x['kind']=='native' for x in receipt['commands']) if receipt else 0,normal_programs=5 if passed else None,sanitized_programs=5 if passed else None,new_timing_samples=0,performance_accepted=False,diagnostic_admitted=False,screen_admitted=False))
    need(passed,'finite batch failed; preserve and diagnose without blind retry')
    print('R1 targeted:5 normal+5 SAN passed;0 timing')
if __name__=='__main__':main()
