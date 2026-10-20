"""Collect the closed M1 timeout window; no native, build, or timing work."""
import datetime
import importlib.util
from pathlib import Path

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
D=J.DOC/'screen';OUT=J.OUTPUT/'M1-screen-20261019-v1'
PIN='e57468ed00157a0154f220b4c67a29879c3fc14bebb9ff04d65f621e4ed11685'

def ref(p):return {'path':str(p),'identity':J.fid(p)}

def main():
    review=J.read(D/'FAILURE_REVIEW.json')
    J.require(review['status']=='verified_closed_failure' and review['freeze_sha256']==PIN,'independent closed-failure review required')
    status=J.read(OUT/'status.json');audit=J.read(D/'AUDIT.json');dispatch=J.read(D/'TIMING_DISPATCH_RESULT.json')
    restored=J.read(D/'RESTORED.json');queue=J.read(OUT/'queue/result.json')
    J.require(status['status']=='failed' and len(status['attempted'])==119 and len(status['completed'])==118 and len(status['unexecuted'])==25,'exact closed prefix required')
    J.require(audit['status']=='preserved_failure' and audit['children']==118 and audit['summary'] is None,'failure cannot become statistical inference')
    J.require(audit['freeze_sha256']==restored['freeze_sha256']==PIN,'same freeze required')
    J.require(dispatch['returncode']==1 and dispatch['screen_attempts']==1 and dispatch['retries']==0,'one failed dispatch only')
    J.require(restored['status']=='passed' and restored['user_processes_restored'] is True,'restoration incomplete')
    J.require(queue['status']=='failed' and queue['command_returncode']==1 and all(not v for v in queue['cleanup'].values()),'exact original timing cleanup required')
    last=OUT/'commands/p059-1-candidate.receipt.json';r=J.read(last)
    J.require(r['timed_out'] is True and r['returncode']==-9 and r['timeout_seconds']==20 and r['launch_error'] is None,'actual final timeout receipt required')
    material=J.read(J.ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    actual={n:J.fid(J.ROOT/n) for n in material['non_external_files']}
    J.require(len(actual)==154 and actual==material['non_external_files'],'baseline154 changed')
    J.require(len(material['production_files'])==14 and all(J.fid(J.ROOT/n)==v for n,v in material['production_files'].items()),'baseline14 changed')
    gates=J.read(D/'GATES.json');J.require(gates['status']=='passed' and len(gates['gates'])==9,'full functional gates required')
    for item in gates['gates'].values():
        J.require(str(J.fid(item['path'])['bytes'])==str(item['identity']['bytes']) and J.fid(item['path'])['sha256']==item['identity']['sha256'],'functional gate changed')
    holder=J.read(J.LOCK/'queue.lock');J.require(holder['status']=='complete','original shared lease not idle')
    result={'schema':'c14-m1-validation-final-v1','status':'functional_pass_screen_closed_timeout_no_promotion',
        'observed_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'candidate':'m1-default-scalar-v2',
        'functional_gates':gates,'full_normal_tests':73,'full_sanitized_tests':73,
        'business_normal_cases':80,'business_sanitized_cases':80,'lifecycle_matrix_cases':52,
        'V1_main_cases':84,'V1_extra_cases':8,'V1_independent_AB_C_actions':4,
        'source_members_bound':154,'production_files_bound':14,'baseline154_unchanged':True,'baseline14_unchanged':True,
        'identity_equivalence':ref(J.DOC/'M1_IDENTITY_EQUIVALENCE.json'),
        'screen':{'freeze_sha256':PIN,'attempts':1,'retries':0,'replacement':0,'scheduled_children':144,
            'attempted_children':119,'complete_validated_children':118,'unexecuted_children':25,
            'failed_child':'p059-1-candidate','failed_receipt':ref(last),'failed_timeout_seconds':20,
            'failed_actual_elapsed_seconds':(int(r['finished_monotonic_ns'])-int(r['started_monotonic_ns']))/1e9,
            'failed_returncode':-9,'outer_queue_elapsed_seconds':queue['command_elapsed_seconds'],
            'window_closed':True,'audit':ref(D/'AUDIT.json'),'independent_review':ref(D/'FAILURE_REVIEW.json'),
            'postrun_pin':ref(D/'POSTRUN_PIN.json'),'complete_failure_prefix_preserved':True,
            'statistical_summary':None,'promotion_eligible':False,'speed_claim':'none',
            'cause_limit':'Timeout proves only that the final child exceeded20s. No native result or body was emitted, so the phase, semantic correctness, host scheduling, or other root cause is not established.'},
        'restoration':ref(D/'RESTORED.json'),'guard_completion':ref(J.DOC/'host/process-state-v1/completed.json'),
        'actual_final_global_lease':holder,'R1_codegen':ref(J.DOC/'R1_CODEGEN_RESULT.json'),
        'R1_native_or_screen_admitted':False,'QF_admitted':False,'performance_goal_complete':False,
        'candidate_integrated':False,'Windows_native_full_tested':False,'platform_scope':'WSL Ubuntu-22.04 fixed P2996 profile',
        'no_commit_push_merge_publish_remoteCI':True,
        'next':'Keep this M1 screen closed. Core/orchestrator may use separate R1 codegen opportunity to register its next finite correctness/mechanism request; no new heavy request or Q/F is implied.'}
    J.write(J.DOC/'M1_FINAL_RESULT.json',result)
    print(result['status'])

if __name__=='__main__':main()
