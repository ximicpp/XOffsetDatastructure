"""Fixed bounded failure cases; root executes once under the original lease."""
from pathlib import Path
import sys
import diag_wrapper as D

CASES=(
    ('normal',.75,None,None),
    ('nonzero',.75,None,None),
    ('signal',.75,None,None),
    ('no_event_hang',.25,None,None),
    ('event_hang',.25,None,None),
    ('partial_hang',.25,None,None),
    ('truncated',.75,None,None),
    ('epipe',.75,'before_launch',None),
    ('close_after_first',.75,'after_first_event',None),
    ('large_logs',.75,None,None),
    ('log_limit',.75,None,None),
    ('sequence_gap',.75,None,None),
    ('event65',.75,None,None),
    ('wakeups',.12,None,None),
    ('exec_failure',.75,None,None),
    ('collector_error',.75,None,'collector_error_after_first'),
    ('register_error',.75,None,'register_error'),
    ('echild',.75,None,'echild_after_reap'),
    ('descendant_fd',.75,None,None),
)

def run(out):
    out=Path(out);D.require(out.is_absolute() and not out.exists(),'fresh selfcheck output required');out.mkdir(parents=True)
    rows=[]
    for case,cap,close,fault in CASES:
        dummycase={'collector_error':'event_hang','register_error':'event_hang','echild':'normal'}.get(case,case)
        command=[str(Path(sys.executable).resolve()),'-B',str(Path(__file__).parent/'dummy_child.py'),dummycase]
        if case=='exec_failure':command=[str(out/'missing-executable')]
        receipt=D.supervise(command,out/case,{'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C'},timeout=cap,close_reader=close,dummy=True,fault=fault)
        # Concrete expected failure behavior, never production implementation tests.
        ok=receipt['successful_wait4_calls']==1 and receipt['cpu_seconds'] is not None
        if case=='normal':ok &= receipt['status']=='complete' and receipt['returncode']==0 and receipt['last_event']['label']=='main.end'
        elif case=='nonzero':ok &= receipt['returncode']==7 and not receipt['timed_out']
        elif case=='signal':ok &= receipt['returncode']==-15 and not receipt['timed_out']
        elif case=='no_event_hang':ok &= receipt['timed_out'] and receipt['events']==[] and receipt['returncode']==-9
        elif case=='event_hang':ok &= receipt['timed_out'] and receipt['last_event']['label']=='t2' and receipt['returncode']==-9
        elif case=='partial_hang':ok &= receipt['timed_out'] and 'truncated_event_at_eof' in receipt['errors'] and (out/case/'events.raw').read_bytes().endswith(b'input.read.')
        elif case=='truncated':ok &= 'truncated_event_at_eof' in receipt['errors'] and (out/case/'events.raw').read_bytes().endswith(b'XD1 1')
        elif case in ('epipe','close_after_first'):ok &= b'EPIPE-observed' in (out/case/'stdout.raw').read_bytes() and receipt['status']=='failed'
        elif case=='large_logs':ok &= receipt['returncode']==0 and receipt['received_log_bytes']['stdout']==3*65536 and receipt['received_log_bytes']['stderr']==3*65536
        elif case=='log_limit':ok &= receipt['kill_reason']=='log_limit' and sum(receipt['persisted_log_bytes'].values())==D.LIMITS['combined_log_bytes']
        elif case=='sequence_gap':ok &= any('sequence_gap' in e for e in receipt['errors'])
        elif case=='event65':ok &= receipt['status']=='failed' and len(receipt['events'])<=64
        elif case=='wakeups':ok &= receipt['timed_out'] and receipt['wall_seconds']<.5
        elif case=='exec_failure':ok &= receipt['returncode']==127 and b'diag exec:' in (out/case/'stderr.raw').read_bytes()
        elif case=='collector_error':ok &= receipt['kill_reason']=='collector_error' and receipt['last_event']['label']=='main.begin'
        elif case=='register_error':ok &= receipt['kill_reason']=='collector_error' and any('injected second selector registration failure' in e for e in receipt['errors']) and receipt['returncode']==-9
        elif case=='echild':ok=receipt['reaper_error'] is not None and receipt['cpu_seconds'] is None and receipt['status']=='failed'
        elif case=='descendant_fd':ok &= receipt['status']=='failed' and receipt['kill_reason'] in ('descendant_pipe_open','owned_group_remaining')
        # Subreaper must reap known adopted descendants: zombies are a failure,
        # including the final deliberately leaking-parent fixture.
        ok &= receipt['cleanup_remaining']==[]
        row={'case':case,'passed':bool(ok),'native_child_cap_seconds':cap,'body_files':0,'log_cap_bytes':D.LIMITS['combined_log_bytes'],
             'receipt':D.file_id(out/case/'receipt.json'),'status':receipt['status'],'cleanup_remaining':receipt['cleanup_remaining']}
        rows.append(row)
        if not ok:break
    result={'schema':'xoffset-diag-dummy-check-v1','status':'passed' if len(rows)==len(CASES) and all(r['passed'] for r in rows) else 'failed',
        'cases':rows,'planned_cases':len(CASES),'actual_python_dummy_children':len(rows),'Cxx_compiles':0,'business_native_runs':0,'real_diagnostic_runs':0,
        'timing_inference_samples':0,'outer_cap_seconds':30,'event_pipe':'inherited O_NONBLOCK; parent persists while child runs',
        'single_reaper':'only wait4 successful reaping; injected ECHILD discards the sole real wait4 result and intentionally reports resources unavailable'}
    D.exclusive(out/'receipt.json',result);return result
