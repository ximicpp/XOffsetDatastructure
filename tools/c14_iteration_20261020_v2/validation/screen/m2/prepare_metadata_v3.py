"""M2-only source metadata/actual bare-profile observation. Never execute jobs.

All outputs are new M2_*v3 files under the M2 document directory. No worker,
shared publisher/index/observation, R1 file, config or input is modified.
"""
from pathlib import Path
import ast,datetime,hashlib,json,re,shlex,sys
HERE=Path(__file__).resolve().parent
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D=ROOT/'docs/coordination/c14-iteration-20261020-v2';OUT=D/'validation/screen/M2'
B=Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-business-v1')
RUNTIME=('screen.py','campaign.py','core.py','protocol.py','execute.py','audit_job.py','finalize.py','host/process_pause.py','host/pause-control.ps1')
def need(v,m):
    if not v:raise ValueError(m)
def fid(p):
    b=Path(p).read_bytes();return dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def ref(p):return dict(path=str(p),identity=fid(p))
def read(p):return json.loads(Path(p).read_text())
def checked(r):
    v=fid(r['path']);need(v['sha256']==r['identity']['sha256'] and v['bytes']==int(r['identity']['bytes']),'changed input ref: '+r['path']);return Path(r['path'])
def put(name,value):
    need(name.startswith('M2_') and name.endswith('_v3.json') and '/' not in name and '\\' not in name,'only new M2 revision3 filenames allowed')
    target=OUT/name;need(target.resolve().parent==OUT.resolve(),'M2-only output domain')
    with target.open('x') as stream:json.dump(value,stream,ensure_ascii=False,indent=2);stream.write('\n')
    return ref(target)
def normalize_adapter(text):
    for name in ('library_id','source_content_sha256'):
        pattern=r'(\b'+name+r'\s*=\s*")[^"\n]+(")';text,n=re.subn(pattern,r'\1<metadata-literal>\2',text);need(n==1,'single adapter metadata literal '+name)
    return text
def main():
    # No R1/raw144 scan: enforce write isolation and verify selected immutable
    # shared/R1 metadata and all nine R1 worker files before/after this producer.
    protected=[D/'validation/screen/CURRENT_BARE_PROFILE_OBSERVATION.json',D/'validation/screen/SOURCE_READY_v2.json',D/'validation/screen/R1/RESOURCE_CAPTURE_REQUEST.json',D/'validation/screen/R1/config.json',D/'validation/screen/R1/PRECONDITIONS.json']
    protected += [HERE.parent/'r1'/n for n in RUNTIME]
    before={str(p):fid(p) for p in protected}
    design_path=D/'core/SCREEN_DESIGN_M2.json';design=read(design_path);need(design['candidate']=='m2-exact-owns-v1','M2-only design')
    identity_path=checked(design['candidate_identity']);identity=read(identity_path);need(identity['candidate']=='m2-exact-owns-v1' and identity['member_count']==len(identity['members'])==154,'M2 exact154 identity')
    for n in RUNTIME:
        if n.endswith('.py'):ast.parse((HERE/n).read_text(),filename=str(HERE/n))
    profile_path=B/'profile-normal-v2.json';profile=read(profile_path);entry=profile['binaries']['xoffset']
    need(entry['library']=='xoffset-c14-m2-exact-owns-v1' and entry['load_contract']=='detached','actual M2 library/owning profile required')
    need(profile['platform_class']=='wsl2-vm-x86_64','fixed WSL2 profile required')
    comp=Path(profile['closure']['compile-commands']);rows=[r for r in read(comp) if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')];need(len(rows)==1,'one actual M2 compile row required')
    argv=rows[0].get('arguments') or shlex.split(rows[0]['command'])
    need(all(v in argv for v in ('-O3','-DNDEBUG','-freflection','-fexpansion-statements')),'actual bare O3/NDEBUG/P2996 required')
    forbidden=[v for v in argv if v=='-UNDEBUG' or 'sanitize' in v.lower() or v in ('-include','-include-pch') or 'common_diag' in v or 'C14_DIAG' in v]
    need(not forbidden,'actual bare profile contains diagnostic/assert/SAN instrumentation')
    launcher=design['compiler_launcher'];checked(launcher);need(argv[0]==launcher['path'] and launcher['identity']['bytes']==61,'fixed actual outer61B compiler launcher')
    source=Path(profile['closure']['candidate-source-tree']);need(source==B.parent/'m2-full-v1-normal/source','own normal full source required')
    actual={n:fid(source/n) for n in identity['members']};need(actual==design['source']['candidate_members154'],'actual compiled source154 differs from M2 design')
    production={n:actual[n] for n in design['source']['candidate_production14']};need(len(production)==14 and production==design['source']['candidate_production14'],'actual production14 differs')
    digest=hashlib.sha256(json.dumps(production,sort_keys=True,separators=(',',':')).encode()).hexdigest();need(entry['source_content_sha256']==digest,'actual M2 profile14 content digest differs')
    dep_path=B/'compiled-source-binding-normal-v2.json';dep=read(dep_path);need(dep['status']=='passed' and dep['candidate_actual_header_binding_verified'] is True and dep['production_files']==production,'actual M2 compiled-header binding required')
    need(dep['profile']==fid(profile_path) and dep['new_binary']==fid(entry['path']),'actual compiled profile/binary binding differs')
    adapter=Path(profile['closure'].get('candidate-adapter',str(source/'experiments/serialization_comparison/adapters/xoffset/adapter.hpp')))
    driver=adapter.parents[2]/'driver.cpp';control_source=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source');old_driver=control_source/'experiments/serialization_comparison/driver.cpp';old_adapter=control_source/'experiments/serialization_comparison/adapters/xoffset/adapter.hpp'
    need(fid(driver)==fid(old_driver),'original bare run driver changed')
    need(normalize_adapter(adapter.read_text())==normalize_adapter(old_adapter.read_text()),'adapter differs outside two admitted metadata literals')
    for name,want in (('library_id',entry['library']),('commit',entry['commit']),('trust_mode',entry['trust_mode']),('source_content_sha256',entry['source_content_sha256'])):
        match=re.search(r'\b'+name+r'\s*=\s*"([^"\n]+)"',adapter.read_text());need(match is not None and match.group(1)==want,'actual adapter literal differs: '+name)
    full_path=D/'validation/m2_full/M2_FULL_RESULT.json';full=read(full_path)
    need(full['status']=='passed' and full['normal_count']==full['sanitized_count']==73 and full['production_files']==production,'own M2 complete full73+73 required')
    functional=B/'final-business-functional-v2/result.json';f=read(functional);need(f['status']=='passed' and f['native_business_children_each']==80 and f['main_V1_cases']==84 and f['extra_cases']==8 and f['AB_native_actions']==f['C_native_actions']==4,'actual own80+80/V1/AB/C inventory')
    policy_path=OUT/'INPUT_POLICY.json';policy=read(policy_path);staging=read(checked(policy['staging_receipt']));checked(policy['independent_review']);need(policy['candidate']=='m2-exact-owns-v1' and policy['status']=='passed' and policy['all_roles_same_paths'] is True,'approved common staged input policy required')
    for row in staging['files']:
        need(fid(row['source'])==fid(row['target'])==row['identity'],'original/staged bytes changed')
    need(len(staging['files'])==9 and staging['elapsed_seconds']==1.275457023 and staging['actual_allocated_bytes']==62218240,'retained actual staging cost differs')
    observed=dict(status='observed_actual_bare_profile_only',candidate='m2-exact-owns-v1',observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),profile=ref(profile_path),binary=ref(Path(entry['path'])),actual_metadata=entry,actual_compile_argv=argv,compile_commands=ref(comp),compiled_source_binding=ref(dep_path),actual_compiled_source=str(source),source154=actual,production14=production,source_content_sha256=digest,candidate_identity=design['candidate_identity'],bare_O3_NDEBUG=True,UNDEBUG_SAN_diagnostic_flags=[],original_driver=ref(driver),original_driver_matches_control=True,adapter=ref(adapter),adapter_delta='Exactly library_id/source_content_sha256 metadata literals; legacy commit remains original benchmark base metadata, not actual M2 Git identity.',launcher61=launcher,own_full73plus73=ref(full_path),own_functional80plus80_V1=ref(functional),new_compiles_required_for_bare=False,typed_gate_admission=False,new_native=0,new_compile=0,new_queue=0)
    # This is already-completed own functional evidence, never a new preview or
    # a proxy for projected staged-screen wall time. Do not compute contrasts.
    rp=B/'functional-normal-v2/commands/xoffset-M-run.receipt.json';receipt=read(rp);prefix=B/'functional-normal-v2/commands/xoffset-M';need(receipt['returncode']==0 and receipt['timed_out'] is False and entry['path'] in receipt['argv'],'actual own complete M functional receipt')
    observed['existing_functional_M']=dict(receipt=ref(rp),argv=receipt['argv'],full_process_seconds=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9,outputs={e:ref(prefix.with_suffix('.'+e)) for e in ('wire','canonical','queries','native.json')},not_screen_or_capture=True,not_resource_feasibility_admission=True,performance_inference=False)
    observation=put('M2_BARE_PROFILE_OBSERVATION_v3.json',observed)
    runtime={n:ref(HERE/n) for n in RUNTIME};previous=read(OUT/'TOOL_MANIFEST_v2.json')
    need(all(previous['files'][n]['identity']==runtime[n]['identity'] for n in RUNTIME),'M2 execution worker changed; new independent source review would be required')
    manifest=put('M2_TOOL_MANIFEST_v3.json',dict(status='source_ready_not_execution_admission',candidate=design['candidate'],revision=3,runtime_files=runtime,execution_tools_modified=False,source_metadata_producer=ref(Path(__file__)),previous=ref(OUT/'TOOL_MANIFEST_v2.json'),design=ref(design_path),approved_input_policy=ref(policy_path)))
    worker=str(HERE/'screen.py');doc=str(OUT);capture=str(B.parent/'m2-screen-resource-capture-v1')
    pending={n:None for n in ('own_final_functional_binding','all11_typed_gate_receipts','M2_parent_source_cost_gate_acceptance','fresh_M2_power_observation','fresh_M2_affinity_resource_observation','preconditions','config','exact_resource_capture_request','independent_resource_capture_admission','actual_resource_capture','complete_process_allocated_projection','freeze','independent_freeze_admission','fresh_host_review','actual_global_quiet','actual_restoration')}
    plan=put('M2_RESOURCE_CAPTURE_PLAN_v3.json',dict(status='draft_not_execution_admission',candidate=design['candidate'],source_metadata_only=True,tool_manifest=manifest,bare_profile_observation=observation,input_policy=ref(policy_path),all_roles_same_staged_paths=True,staged_wire=staging['wire'],staged_fixture=staging['fixture'],staging_cost=dict(seconds=staging['elapsed_seconds'],retained_allocated_bytes=staging['actual_allocated_bytes']),roles=['control','candidate'],new_bare_compiles=0,resource_children=2,child_seconds=20,outer_seconds=120,body_file_limit_bytes=64*1024**2,RSS_limit_bytes=16*1024**3,allocated_limit_bytes=4*1024**3,reserve_bytes=2*1024**3,warmup=0,retry=0,replacement=0,timing_inference_samples=0,output=capture,queue=capture+'/queue',pending=pending,source_only_entry_argv=dict(config=['/usr/bin/python3','-B',worker,'config','--root',str(ROOT),'--out',doc+'/config.json','--output',str(B.parent/'M2-screen-20261020-v2-v1'),'--control-profile','/root/xoffset-receive-completion/20261019/v1/profile-normal-v2.json','--candidate-profile',str(profile_path),'--preconditions',doc+'/PRECONDITIONS.json'],request_capture=['/usr/bin/python3','-B',worker,'request-capture','--config',doc+'/config.json','--out',capture,'--request',doc+'/RESOURCE_CAPTURE_REQUEST.json']),execution='No executable capture argv/pin is issued until own complete gates/preconditions/config bind, a finite exact request is written and independently admitted. Future capture uses original global W3 functional120; screen remains W3 timing600/audit3600. No R1 capture or144 sample receipt is borrowed.'))
    draft=read(OUT/'REQUEST_DRAFT_v2.json');draft.update(revision=3,status='M2_source_ready_actual_bare_profile_observed_gates_pending',tool_manifest=manifest,actual_M2_bare_profile_observation=observation,current_candidate_bare=observed,resource_capture_plan=plan,pending_refs=pending,statistics={**draft['statistics'],'stages':design['statistics']['stages'],'metrics':design['statistics']['metrics'],'primary':design['statistics']['primary'],'auxiliary':design['statistics']['auxiliary']},source_ready_scope='Only actual M2 bare profile/tool metadata is refreshed in new M2 files. Own full73+73/80+80/V1 facts are observed; package/shared-runtime/lifecycle/final independent typed gates and mechanism acceptance remain pending in this draft. No configuration, request admission, capture or freeze is issued.',actual_completed_fact_refs=[ref(full_path),ref(functional)],typed_gates={n:dict(status='pending_actual_typed_receipt_and_independent_acceptance',receipt=None) for n in ('codegen','mechanism','targeted','full_normal','full_san','business_SML','v1_bidirectional','consumers','lifecycle','safety','engineering')},input_policy=ref(policy_path),current_control_policy='Same accepted C14 control profile/metadata-only manifest exception is available. Its actual runtime/binary/input closure will be freshly bound by the future exact M2 request; no R1 performance result is reused.')
    request=put('M2_REQUEST_DRAFT_v3.json',draft)
    after={str(p):fid(p) for p in protected};need(before==after,'protected shared/R1 selected metadata changed during producer')
    ready=put('M2_SOURCE_READY_v3.json',dict(status='source_ready_pending_independent_review',candidate=design['candidate'],revision=3,request=request,tool_manifest=manifest,bare_profile_observation=observation,resource_capture_plan=plan,execution_worker_changes=0,producer=ref(Path(__file__)),protected_selected_shared_R1_before_after=before,protected_selected_bytes_unchanged=True,write_scope='Only new M2_*_v3.json documents under validation/screen/M2; producer itself is new under tools/.../screen/m2. R1 raw144/freeze/disposition were not opened for write or rescanned. No old publisher executed.',pending_functional_mechanism_typed_gates=True,configuration_and_capture_request_not_generated=True,counts=dict(compile=0,native=0,queue=0,host_plan=0,host_pause=0,freeze=0),new_bare_compile_required=False,screen_admitted=False))
    print(json.dumps(dict(source_ready=ready,observation=observation,manifest=manifest,request=request,plan=plan,screen_worker=runtime['screen.py'],legacy_commit='base metadata only',bare_O3_NDEBUG=True,source154=154,production14=14,compile=0,native=0,queue=0)))
if __name__=='__main__':main()
