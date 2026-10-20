"""Publish consistent current revision without modifying any earlier draft."""
from pathlib import Path
import hashlib,json
H=Path(__file__).resolve().parent;ROOT=H.parents[3]
W='/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发';D=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/screen'
def fid(p):
    b=Path(p).read_bytes();return dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def ref(p):return dict(path=W+'/'+Path(p).relative_to(ROOT).as_posix(),identity=fid(p))
def read(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def write(p,v):
    with p.open('x',encoding='utf-8',newline='\n') as stream:stream.write(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def main():
    manifests={};requests={};prior={}
    for label in ('R1','M2'):
        lower=label.lower();dest=D/label;old=read(dest/'REQUEST_DRAFT.json');design=read(ROOT/f'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_{label}.json');doc=W+'/'+dest.relative_to(ROOT).as_posix();worker=W+'/'+(H/lower).relative_to(ROOT).as_posix()
        files={p.relative_to(H/lower).as_posix():ref(p) for p in sorted((H/lower).rglob('*')) if p.is_file()}
        prior[label]={n:ref(dest/n) for n in ('TOOL_MANIFEST.json','REQUEST_DRAFT.json')}
        manifest=dest/'TOOL_MANIFEST_v2.json';write(manifest,dict(status='source_ready_not_execution_admission',revision=2,candidate=design['candidate'],files=files,previous_unexecuted_draft=prior[label]['TOOL_MANIFEST.json'],previous_draft_is_not_current_tool_binding=True,derivation=ref(H/'DERIVATION.json'),root_approved_input_policy=ref(dest/'INPUT_POLICY.json')));manifests[label]=ref(manifest)
        old.update(revision=2,status='source_ready_pending_independent_exact_capture_and_screen_admission',tool_manifest=ref(manifest),previous_unexecuted_draft=prior[label]['REQUEST_DRAFT.json'],statistics={**old['statistics'],'metrics':design['statistics']['metrics'],'stages':design['statistics']['stages'],'primary':design['statistics']['primary'],'auxiliary':design['statistics']['auxiliary']},input_policy=ref(dest/'INPUT_POLICY.json'),input_policy_parent_acceptance=ref(ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/SCREEN_INPUT_POLICY_ACCEPTANCE.json'),C_result=ref(ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/diag/C_RESULT.json'),control_metadata_parent_acceptance=ref(ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/CONTROL_METADATA_EQUIVALENCE.json'))
        old['paths'].update(original_fixture=old['paths']['fixture'],original_input_wire=old['paths']['input_wire'],fixture='/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1/fixed-M',input_wire='/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1/initial-wire.bin',resource_capture=f'/root/xoffset-c14-iteration/20261020/v2/validation/{lower}-screen-resource-capture-v1')
        old['budgets'].update(new_bare_compiles=0,new_projection_native=2,resource_capture_children=2,resource_capture_outer_seconds=120,resource_capture_child_seconds=20,resource_capture_retry=0)
        old['staging_engineering_cost']=dict(seconds=1.275457023,retained_allocated_bytes=62218240,source_only=True,native_preview=0,charged_to_physical_projection_and_conservative_online_seconds=True)
        old['projection_policy']='Prefer a separately root-admitted finite resource capture: exactly one control then one candidate on identical approved WSL staged inputs, each20s/outer120s, full bare process and complete byte truth. Capture is not warmup or a timing sample; no AB difference/selection/retry. Complete measured process+full body audit plus predeclared positive tail margin and staging cost must fit144children+nonchild60+seal30 inside600s, all-wire-unique physical projection4GiB. Existing /root M receipts never represent original /mnt/c paths. No M1 prefix or C observer/replay times enter projection or statistics.'
        old['pending_refs'].update(control_metadata_equivalence_passed=old['control_metadata_parent_acceptance'],input_policy=old['input_policy'],resource_capture=None,resource_capture_admission=None)
        old['entry_argv']['freeze'][old['entry_argv']['freeze'].index('--capture')+1]=old['paths']['resource_capture']
        old['entry_argv']['request_capture']=['/usr/bin/python3','-B',worker+'/screen.py','request-capture','--config',doc+'/config.json','--out',old['paths']['resource_capture'],'--request',doc+'/RESOURCE_CAPTURE_REQUEST.json']
        if label=='R1':
            old['pending_refs'].update(preconditions=ref(dest/'PRECONDITIONS.json'),resource_capture_request=ref(dest/'RESOURCE_CAPTURE_REQUEST.json'))
            old['exact_capture_dispatch']=ref(dest/'RESOURCE_CAPTURE_DISPATCH.json');old['entry_argv']['capture_after_independent_admission']=read(dest/'RESOURCE_CAPTURE_DISPATCH.json')['argv'];old['source_ready_scope']='R1 actual bare config and finite request bound; execution still requires independent RESOURCE_CAPTURE_ADMISSION. R1 execution code now frozen under07ebe request closure.'
        else:old['source_ready_scope']='M2 independent namespace/policy/source-ready only. Actual own business bare profile, config, full gate closure and finite capture request remain pending; no R1 receipt substitution.'
        request=dest/'REQUEST_DRAFT_v2.json';write(request,old);requests[label]=ref(request)
    ready=dict(status='source_ready_not_execution_admission',revision=2,requests=requests,tool_manifests=manifests,previous_unexecuted_drafts=dict(source_ready=ref(D/'SOURCE_READY.json'),candidates=prior),previous_immutable='Old SOURCE_READY/REQUEST_DRAFT/TOOL_MANIFEST bytes remain unchanged and cannot admit current tools. v2 derives10 metrics directly from each frozen design and binds current files.',exact_R1_resource_capture_request=ref(D/'R1/RESOURCE_CAPTURE_REQUEST.json'),exact_R1_resource_capture_dispatch=ref(D/'R1/RESOURCE_CAPTURE_DISPATCH.json'),input_policy_parent_acceptance=ref(ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/SCREEN_INPUT_POLICY_ACCEPTANCE.json'),control_metadata_parent_acceptance=ref(ROOT/'docs/coordination/c14-iteration-20261020-v2/reviews/CONTROL_METADATA_EQUIVALENCE.json'),source_only_checks=dict(AST=True,independent_random_schedule72pairs144children_AB36_BA36=True,zero_all10_undefined_no_promotion=True,negative_noncanonical_duplicate_role_rejected=True,borrowed_execute_native_truth_bytes_unchanged=True,host_helper_bytes_unchanged_and_module_only_new_STATE=True),actions=dict(Cplusplus_compile=0,native=0,queue=0,host_plan=0,host_suspend=0),R1_execution_code_frozen=True,M2_config_profile_gates_pending=True,screen_not_admitted=True)
    write(D/'SOURCE_READY_v2.json',ready)
    print(json.dumps(dict(source_ready=ref(D/'SOURCE_READY_v2.json'),requests=requests,tool_manifests=manifests)))
if __name__=='__main__':main()
