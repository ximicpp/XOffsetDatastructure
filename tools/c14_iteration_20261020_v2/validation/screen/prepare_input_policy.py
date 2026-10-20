"""Source-only adapter of parent's actual staged-input acceptance; no native."""
from pathlib import Path
import hashlib,json
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发');D=ROOT/'docs/coordination/c14-iteration-20261020-v2'
def fid(p):
    b=Path(p).read_bytes();return dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def ref(p):return dict(path=str(p),identity=fid(p))
def read(p):return json.loads(Path(p).read_text())
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def main():
    acceptance=D/'reviews/SCREEN_INPUT_POLICY_ACCEPTANCE.json';a=read(acceptance);staging=D/'validation/screen/INPUT_STAGING_RESULT.json';s=read(staging);cpath=D/'validation/diag/C_RESULT.json';c=read(cpath)
    assert a['status']=='accepted_for_new_screen_environment_policy_only' and a['mismatches']==0 and fid(staging)==a['staging_result']['identity']
    assert fid(cpath)['sha256']=='c13e761ba41a67a7abcac600fbcea2a1edfe517032c782ccc98f1d41cbeab2c4' and c['status']=='passed'
    boundaries={}
    for role in ('c14','r1'):
        observe=c['roles'][role]['observer'];assert fid(observe['path'])==observe['identity'];o=read(observe['path']);events={e['label']:e for e in o['events']}
        assert o['returncode']==0 and not o['timed_out'] and o['observer_complete']
        before=events['input.read.before'];after=events['input.read.after'];elapsed=int(after['child_monotonic_ns'])-int(before['child_monotonic_ns']);phases=int(events['t8']['child_monotonic_ns'])-int(events['t0']['child_monotonic_ns'])
        boundaries[role]=dict(observer=observe,input_read_before=before,input_read_after=after,input_read_interval_ns=elapsed,original_eight_stage_interval_ns=phases,interpretation='Actual input.read outside t0..t8 exceeds all original measured stages combined in this instrumented diagnostic. Observer/replay are additional work; no bare AB speed inference or historical M1 cause.')
    for row in s['files']:assert fid(row['source'])==fid(row['target'])==row['identity']
    assert len(s['files'])==9 and sum(r['identity']['bytes'] for r in s['files'])==s['bytes']
    for label,candidate in (('R1','r1-canonical-hotcold-v1'),('M2','m2-exact-owns-v1')):
        policy=dict(kind='v2-screen-staged-input-policy',status='passed',approval_scope='Parent accepted new environment policy only; capture/screen requests still require their own independent exact admission.',candidate=candidate,all_roles_same_paths=True,API_model_trace_unchanged=True,staging_receipt=ref(staging),independent_review=ref(acceptance),C_result=ref(cpath),actual_diagnostic_boundaries=boundaries,staging_engineering_cost_accounted=True,staging_seconds=s['elapsed_seconds'],staging_retained_allocated_bytes=s['actual_allocated_bytes'],diagnostic_times_not_AB_speed=True,new_native_preview=0,old_frozen_windows_unchanged=True)
        write(D/f'validation/screen/{label}/INPUT_POLICY.json',policy)
    print(json.dumps({'status':'derived_only_parent_environment_acceptance','input_read_intervals_ns':{r:b['input_read_interval_ns'] for r,b in boundaries.items()},'staging_seconds':s['elapsed_seconds'],'allocated_bytes':s['actual_allocated_bytes'],'new_native':0,'compile':0,'queue':0}))
if __name__=='__main__':main()
