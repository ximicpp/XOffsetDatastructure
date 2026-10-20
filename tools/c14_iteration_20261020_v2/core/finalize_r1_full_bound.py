"""Final read-only request/source/receipt recheck around the retained finalizer.

The v1 worker/request bytes stay frozen; this v2 envelope strengthens only the
last binding check. No native execution, retry or extra gate budget is created.
"""
import importlib.util
import json
from pathlib import Path

H=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('retained_r1_finalizer',H/'finalize_r1_full.py')
F=importlib.util.module_from_spec(spec);spec.loader.exec_module(F)
R,D,V=F.R,F.D,F.V
OUTER=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_R1_FULL_v2.json'

def exact_ref(value):
    path=Path(value['path']);F.need(F.fid(path)==value['identity'],'reference changed: '+str(path));return path

def main():
    q=F.read(OUTER)
    inner=exact_ref(q['inner_request']);inner_value=F.read(inner)
    F.need(inner_value['candidate_identity']==q['candidate_identity'] and inner_value['source_members']==q['source_members'],'outer/inner candidate differs')
    for path,value in q['tool_files'].items():F.need(F.fid(path)==value,'final tool binding changed: '+path)
    identity=R/'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json'
    F.need(F.fid(identity)==q['candidate_identity'],'frozen R1 identity changed')
    full=F.passed(D/'R1_FULL_RESULT.json')
    F.need(full['request_identity']==F.fid(inner),'full receipts not bound to approved inner request')
    F.need(full['production_files']==q['production14'],'full production14 differs')
    checked={}
    for mode,key in (('normal','normal_receipt'),('sanitized','sanitized_receipt')):
        receipt_path=exact_ref(full[key]);receipt=F.passed(receipt_path)
        request_path=D/('R1_FULL_'+mode.upper()+'_REQUEST_v1.json')
        F.need(receipt['request']['path']==str(request_path) and
               {k:receipt['request'][k] for k in ('bytes','sha256')}==F.fid(request_path),'actual full child request changed')
        invpath=receipt_path.parent/'build/test-inventory.json';inv=F.read(invpath)
        expected=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
        F.need(inv['mode']=='full' and len(expected)==73 and sorted(receipt['junit_names'])==sorted(expected),'actual inventory/JUnit differs')
        F.need(receipt['inventory_identity']==F.fid(invpath),'full inventory file changed')
        junit=receipt_path.parent/'ctest.xml'
        F.need(receipt['junit_identity']==F.fid(junit),'full actual JUnit file changed')
        F.need(receipt['compile_commands_identity']==F.fid(receipt_path.parent/'build/compile_commands.json'),'full compile database changed')
        F.need(receipt['production_files']==q['production14'],'full child production differs')
        for name,value in q['source_members'].items():
            F.need(F.fid(receipt_path.parent/'source'/name)=={'bytes':value['bytes'],'sha256':value['candidate_sha256']},'full source154 changed: '+mode+'/'+name)
        checked[mode]={'receipt':F.ref(receipt_path),'request':F.ref(request_path),'inventory':F.ref(invpath),'junit':F.ref(junit)}
    bridge=F.read(D/'R1_FULL_SOURCE_BINDING.json')
    F.need(bridge['request_identity']==F.fid(inner) and bridge['candidate_members']==q['source_members'] and
           bridge['production_files']['candidate']==q['production14'],'source binding bridge differs')
    for name in ('codegen','targeted','current_A'):exact_ref(bridge[name])
    prepared_path=D/'business/PREPARED.json';prepared=F.read(prepared_path)
    F.need(prepared['status']=='prepared' and prepared['production_files']==q['production14'] and
           prepared['requests']==F.fid(inner),'business preparation is not current approved source/request')
    for path,value in prepared['bindings'].items():F.need(F.fid(path)==value,'prepared input changed: '+path)
    for mode,value in prepared['full_receipts'].items():
        F.need(exact_ref(value)==Path(full['normal_receipt' if mode=='normal' else 'sanitized_receipt']['path']),'business full receipt differs')
    F.main()
    gate=D/'R1_FUNCTIONAL_GATES.json'
    record={'status':'passed','scope':'Final binding of actual R1 functional gates; no performance admission',
        'outer_request':F.ref(OUTER),'inner_request':F.ref(inner),'functional_gates':F.ref(gate),
        'candidate_identity':F.ref(identity),'actual_full':checked,'prepared':F.ref(prepared_path),
        'source_bridge':F.ref(D/'R1_FULL_SOURCE_BINDING.json'),'tool_files':q['tool_files'],
        'source154_each_full_rechecked':True,'production14_rechecked':True,
        'historical_text_residue':'Retained prepare worker PREPARED.full_binding mentions r2 requests/earlier preflight from its M1 origin. That inherited explanatory string is not evidence of any R1 preflight or repair. R1 actual request/receipt identities are the new checked references above.',
        'performance_accepted':False,'screen_admitted':False,'QF_admitted':False}
    with (D/'R1_FUNCTIONAL_GATES_BINDING.json').open('x',encoding='utf-8') as f:json.dump(record,f,indent=2);f.write('\n')

if __name__=='__main__':main()
