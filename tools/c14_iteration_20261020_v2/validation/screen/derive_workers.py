"""Source-only instantiation of two isolated v2 workers; never execute a child."""
from pathlib import Path
import ast,hashlib,json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OLD=ROOT/'tools/c14_iteration_20261019/validation/screen'
WSL='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发'

def fid(p):
    b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def function(text,name,new):
    start=text.index('def '+name+'(')
    end=text.find('\ndef ',start+1)
    if end<0:raise ValueError('function boundary '+name)
    return text[:start]+new.rstrip()+'\n'+text[end:]

def main():
    records={}
    for label,candidate,primary in (('R1','r1-canonical-hotcold-v1','read_sum'),('M2','m2-exact-owns-v1','mutate')):
        target=HERE/label.lower();target.mkdir(exist_ok=True)
        design=ROOT/f'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_{label}.json'
        base=(OLD/'m1_screen.py').read_text(encoding='utf-8')
        # New output/schema namespaces only; historical artifacts are inputs,
        # never renamed into a new timing window.
        text=base.replace('m1_screen.py','screen.py').replace('M1','%LABEL%').replace('m1-','%LOWER%-')
        text=text.replace('20261019/v1/validation','20261020/v2/validation').replace('20261019-v1','20261020-v2')
        text=text.replace('M1-only','v2-only')
        text=text.replace("POLICY=dict(candidate='%LABEL%',primary='mutate'", "POLICY=dict(candidate='%LABEL%',primary='%PRIMARY%'")
        pos=text.index("GATES=(")
        end=text.index('\n\ndef need',pos)
        text=text[:pos]+'''ROOT=Path('%ROOT%')
DESIGN_PATH=ROOT/'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_%LABEL%.json'
DESIGN_SHA='%DESIGN_SHA%'
DESIGN=C.read(DESIGN_PATH)
C.require(C.sha(DESIGN_PATH)==DESIGN_SHA and DESIGN['candidate']=='%CANDIDATE%', 'exact candidate design changed')
GATES=('codegen','mechanism','targeted','full_normal','full_san','business_SML','v1_bidirectional','consumers','lifecycle','safety','engineering')
''' +text[end:]
        for name in ('schedule','source_snapshot','bindings','config','validate_cfg','projection','freeze','summary','active','capture'):
            text=function(text,name,(HERE/'templates'/(name+'.py')).read_text(encoding='utf-8'))
        text=function(text,'checked_file',"def checked_file(record):\n    need(set(record)=={'path','identity'},'file binding requires path and identity')\n    p=path(record['path']);actual=fid(p);expected=record['identity']\n    need(set(expected)=={'bytes','sha256'} and int(actual['bytes'])==int(expected['bytes']) and actual['sha256']==expected['sha256'],'file identity changed: '+str(p));return p\n")
        helpers='\n\n'.join((HERE/'templates'/(name+'.py')).read_text(encoding='utf-8') for name in ('input_policy','project_capture','request_capture'))
        text=text.replace('\ndef main():','\n'+helpers+'\n\ndef main():')
        text=text.replace("need(2 in os.sched_getaffinity(0),'registered CPU2 no longer allowed')", "need(cfg['cpu'] in os.sched_getaffinity(0),'fresh registered CPU no longer allowed')")
        text=text.replace("checked_file(f['gate_admission']);resources(cfg,f['projection']['physical_peak_bytes'])", "checked_file(f['gate_admission']);capture_path=path(f['capture']);need(fid(capture_path/'manifest.json' if capture_path.is_dir() else capture_path)==f['projection']['capture_manifest'],'registered functional projection changed');resources(cfg,f['projection']['physical_peak_bytes'])")
        text=text.replace("roots=(path(out),path(f['capture']),path(f['frozen_path']).parent)", "roots=(path(out),path(f['capture']),path(f['frozen_path']).parent,Path('/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1'))")
        text=text.replace("need(restored['power_before']==restored['power_after']=='381b4222-f694-41f0-9685-ff5bb260df2e','Balanced power preservation evidence differs')", "need(restored['power_before']==restored['power_after']==cfg['host_policy']['power_guid'],'fresh registered power preservation evidence differs')")
        text=text.replace("    a=sub.add_parser('capture')\n    for n in ('config','out','queue'):a.add_argument('--'+n,type=Path,required=True)\n", "    a=sub.add_parser('capture')\n    for n in ('config','out','queue','request','review'):a.add_argument('--'+n,type=Path,required=True)\n    a.add_argument('--request-sha256',required=True)\n    a=sub.add_parser('request-capture')\n    for n in ('config','out','request'):a.add_argument('--'+n,type=Path,required=True)\n")
        text=text.replace("elif args.action=='capture':r=capture(args.config,args.out,args.queue)", "elif args.action=='capture':r=capture(args.config,args.out,args.queue,args.request,args.request_sha256,args.review)\n        elif args.action=='request-capture':r=request_capture(args.config,args.out,args.request)")
        text=text.replace("for n in ('root','out','output','control-profile','candidate-profile'):a.add_argument('--'+n,type=Path,required=True)", "for n in ('root','out','output','control-profile','candidate-profile','preconditions'):a.add_argument('--'+n,type=Path,required=True)")
        text=text.replace('config(args.root,args.out,args.output,args.control_profile,args.candidate_profile)', 'config(args.root,args.out,args.output,args.control_profile,args.candidate_profile,args.preconditions)')
        text=text.replace("need(f['schema']=='%LOWER%-screen-freeze-v1' and f['policy']==POLICY and f['schedule']==schedule(),'fixed freeze policy/schedule mismatch')", "need(f['schema']=='%LOWER%-screen-freeze-v2' and f['policy']==POLICY and f['schedule']==schedule() and f['design']==dict(path=str(DESIGN_PATH),identity=fid(DESIGN_PATH)),'fixed v2 policy/design/randomized schedule mismatch')")
        # No older screen/postrun PIN can authenticate the new protocol.
        text=text.replace('%LABEL%-SCREEN-POSTRUN-v1','%LABEL%-SCREEN-POSTRUN-v2')
        text=text.replace('%LOWER%-screen-run-v1','%LOWER%-screen-run-v2').replace('%LOWER%-screen-audit-v1','%LOWER%-screen-audit-v2').replace('%LOWER%-screen-unsealed-failure-v1','%LOWER%-screen-unsealed-failure-v2')
        # Tool and design references form the actual independent runtime closure.
        text=text.replace("HERE/'protocol.py',RUNNER,Path(sys.executable).resolve()", "HERE/'protocol.py',HERE/'execute.py',HERE/'audit_job.py',HERE/'finalize.py',HERE/'host/process_pause.py',HERE/'host/pause-control.ps1',DESIGN_PATH,ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/STATISTICAL_PRECHECK.md',ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/SCREEN_SCHEDULE_REVIEW.md',RUNNER,Path(sys.executable).resolve()")
        replacements={'%ROOT%':WSL,'%LABEL%':label,'%LOWER%':label.lower(),'%PRIMARY%':primary,'%CANDIDATE%':candidate,'%DESIGN_SHA%':fid(design)['sha256'],'%TARGET_N%':str(5 if label=='R1' else 4)}
        for a,b in replacements.items():text=text.replace(a,b)
        ast.parse(text);(target/'screen.py').write_text(text,encoding='utf-8',newline='\n')
        for name in ('campaign.py','core.py','protocol.py'):(target/name).write_bytes((OLD/name).read_bytes())
        for name in ('execute.py','audit_job.py','finalize.py'):
            content=(HERE/'templates'/name).read_text(encoding='utf-8')
            for a,b in replacements.items():content=content.replace(a,b)
            ast.parse(content);(target/name).write_text(content,encoding='utf-8',newline='\n')
        host=target/'host';host.mkdir(exist_ok=True)
        oldhost=OLD.parent/'host';content=(oldhost/'process_pause.py').read_text(encoding='utf-8')
        before="STATE=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261019-v1/validation/host/process-state-v1')"
        after="STATE=Path("+repr(WSL+f'/docs/coordination/c14-iteration-20261020-v2/validation/screen/{label}/host/process-state-v1')+")"
        assert content.count(before)==1;content=content.replace(before,after);ast.parse(content)
        (host/'process_pause.py').write_text(content,encoding='utf-8',newline='\n');(host/'pause-control.ps1').write_bytes((oldhost/'pause-control.ps1').read_bytes())
        records[label]={'design':fid(design),'worker':fid(target/'screen.py'),'borrowed':{n:fid(target/n) for n in ('campaign.py','core.py','protocol.py')},'native':0,'compile':0}
    (HERE/'DERIVATION.json').write_text(json.dumps({'schema_version':1,'original_worker':fid(OLD/'m1_screen.py'),'candidates':records,'scope':'Source-only v2; new random schedule and correct primary/auxiliary sets, typed gates/projection and current host bindings; old v1 untouched'},indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
