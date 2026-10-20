"""Read-only WSL source/profile evidence; write only new screen review docs."""
from pathlib import Path
import datetime,hashlib,json,shlex
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/screen'
def fid(p):
    b=Path(p).read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def ref(p):return {'path':str(p),'identity':fid(p)}
def read(p):return json.loads(Path(p).read_text())
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def main():
    design=read(ROOT/'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_R1.json');expected=design['source']['control_members154'];actual_root=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
    rows={n:{'expected':v,'actual':fid(actual_root/n),'identical':v==fid(actual_root/n)} for n,v in expected.items()}
    production=design['source']['control_production14']
    request={'kind':'control-source-manifest-metadata-equivalence-request','status':'pending_independent_parent_review','actual_source':str(actual_root),'expected_source':design['source']['control'],'design':ref(ROOT/'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_R1.json'),'members154':rows,'different_members':[n for n,v in rows.items() if not v['identical']],'identical_member_count':sum(v['identical'] for v in rows.values()),'production14_actual':{n:fid(actual_root/n) for n in production},'production14_expected':production,'production14_identical':all(fid(actual_root/n)==v for n,v in production.items()),'actual_manifest':ref(actual_root/'SOURCE_MANIFEST.json'),'expected_manifest':ref(Path(design['source']['control'])/'SOURCE_MANIFEST.json'),'candidate_exemption':False,'source_modified':False,'compile':0,'native':0,'queue':0,'requested_decision':'Only historical C14 compiled control SOURCE_MANIFEST packaging metadata may differ;153 other members and production14 must be byte-identical. Parent must bind historical profile/source-binding provenance and review actual metadata difference before passing. No candidate/source code exception.'}
    write(D/'CONTROL_METADATA_EQUIVALENCE_REQUEST.json',request)
    profiles={'control':Path('/root/xoffset-receive-completion/20261019/v1/profile-normal-v2.json'),'R1':Path('/root/xoffset-c14-iteration/20261020/v2/validation/r1-business-v1/profile-normal-v2.json'),'M2':Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-business-v1/profile-normal-v2.json')}
    result={'status':'source_ready_observation_only','observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'profiles':{},'compile':0,'native':0,'queue':0,'host_actions':0,'performance_inference':False}
    for name,p in profiles.items():
        if not p.exists():result['profiles'][name]={'status':'pending_actual_profile','expected_path':str(p)};continue
        value=read(p);cf=Path(value['closure']['compile-commands']);rows=[r for r in read(cf) if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')];assert len(rows)==1
        argv=rows[0].get('arguments') or shlex.split(rows[0]['command']);bare=all(x in argv for x in ('-O3','-DNDEBUG','-freflection','-fexpansion-statements')) and not any('sanitize' in x or x=='-UNDEBUG' or 'common_diag' in x or x=='-include' for x in argv)
        record={'profile':ref(p),'binary':ref(Path(value['binaries']['xoffset']['path'])),'metadata':value['binaries']['xoffset'],'compile_commands':ref(cf),'actual_compile_argv':argv,'bare_O3_NDEBUG':bare,'needs_new_bare_compile':not bare}
        functional=p.parent/'functional-normal-v2';receipt=functional/'commands/xoffset-M-run.receipt.json'
        if receipt.exists():
            r=read(receipt);prefix=functional/'commands/xoffset-M';mounted=ROOT/'handoff/continuation-20261019-v1/portable-inputs';fixture=functional/'prepared/M';fixture_members=('fixture.json','trace.bin','input.canonical.bin','final.canonical.bin','query-before.bin','query-after.bin','queries.bin')
            record['existing_full_M']={'receipt':ref(receipt),'returncode':r['returncode'],'timed_out':r['timed_out'],'argv':r['argv'],'full_process_seconds':(int(r['finished_monotonic_ns'])-int(r['started_monotonic_ns']))/1e9,'input_wire':ref(prefix.with_suffix('.input.wire')),'input_wire_matches_original_mounted_oldV8':fid(prefix.with_suffix('.input.wire'))==fid(mounted/'old-V8-initial-wire.bin'),'fixture_members':{n:{'actual':fid(fixture/n),'original_mounted':fid(mounted/'fixed-M'/n)} for n in fixture_members},'fixture7_equal_original_mounted':all(fid(fixture/n)==fid(mounted/'fixed-M'/n) for n in fixture_members),'outputs':{ext:ref(prefix.with_suffix('.'+ext)) for ext in ('wire','canonical','queries','native.json')},'use':'Feasibility resource projection only after exact fixture/input/profile/full-audit/before-after verification plus separately registered full online body-audit margin; zero AB inference.'}
        result['profiles'][name]=record
    write(D/'CURRENT_BARE_PROFILE_OBSERVATION.json',result)
    print(json.dumps({'different_control_members':request['different_members'],'identical153':request['identical_member_count'],'production14_identical':request['production14_identical'],'profiles':{k:{'status':v.get('status','observed'),'bare_O3_NDEBUG':v.get('bare_O3_NDEBUG'),'existing_M_process_seconds':v.get('existing_full_M',{}).get('full_process_seconds')} for k,v in result['profiles'].items()}},ensure_ascii=False))
if __name__=='__main__':main()
