"""Prepare exact existing two-name watchdog in a new state domain; read-only plan."""
import importlib.util
from pathlib import Path
import subprocess

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
out=H/'host';out.mkdir(exist_ok=False)
doc=J.DOC/'host';doc.mkdir(exist_ok=False)
prior=J.OLD/'build/receive-completion-20261019/process-pause-allocation-v1'
module=(prior/'process_pause.py').read_text()
old="STATE=HERE.parent/'process-pause-allocation-state-v1'"
J.require(module.count(old)==1,'watchdog state binding changed')
module=module.replace(old,'STATE=Path('+repr(str(doc/'process-state-v1'))+')',1)
(out/'process_pause.py').write_text(module)
(out/'pause-control.ps1').write_bytes((prior/'pause-control.ps1').read_bytes())
original=J.OLD/'docs/coordination/receive-completion-20261019/process-pause-authorization-v1.json'
standing=J.OLD/'docs/coordination/receive-completion-20261019/standing-process-pause-human-authorization-v1.json'
grant=J.read(standing)
J.require(J.fid(original)==grant['original_root_record']['identity'],'original actual human grant provenance changed')
J.require(grant['answer']=='允许临时暂停并恢复' and grant['authorized_targets']==['client_ue4-Win64-Shipping','TGitCache'],'standing grant changed')
J.write(doc/'WATCHDOG_DERIVATION.json',{'status':'passed','original_module':{'path':str(prior/'process_pause.py'),**J.fid(prior/'process_pause.py')},
     'module':{'path':str(out/'process_pause.py'),**J.fid(out/'process_pause.py')},'helper':{'path':str(out/'pause-control.ps1'),**J.fid(out/'pause-control.ps1')},
     'delta':'Exactly one state-directory assignment relocated to new M1 validation-owned C: domain; helper bytes and identity/handle/suspend/resume/watchdog checks unchanged.',
     'original_human_grant':{'path':str(original),**J.fid(original)},'standing_interpretation':{'path':str(standing),**J.fid(standing)},
     'scope':'One new M1-only72pair/144child screen; only these2 names, actual fresh PID/creation/image/SID/held handles, restore on success/failure.',
     'power_policy':'Keep current Balanced scheme381b4222-f694-41f0-9685-ff5bb260df2e; read before/after, no scheme switch.',
     'new_human_reply_claimed':False,'old_pid_or_handle_reused':False,'processes_paused_here':0})
cmd=['/usr/bin/python3','-I','-B',str(out/'process_pause.py'),'plan','--out',str(doc/'process-plan-initial.json')]
r=subprocess.run(cmd,capture_output=True,text=True)
J.write(doc/'PLAN_INVOCATION.json',{'argv':cmd,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
J.require(r.returncode==0,'fresh read-only target plan failed')
print(r.stdout)
