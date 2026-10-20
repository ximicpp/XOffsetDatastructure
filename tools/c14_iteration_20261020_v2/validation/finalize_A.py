"""Actual locked-dependency check and narrow A receipt, without platform tests."""
import importlib.util
import json
from pathlib import Path
H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('current',H/'verify_current.py')
V=importlib.util.module_from_spec(s);s.loader.exec_module(V)
s=importlib.util.spec_from_file_location('prior_inventory',V.ROOT/'tools/c14_iteration_20261019/validation/s0_inventory.py')
P=importlib.util.module_from_spec(s);s.loader.exec_module(P)
def main():
    current=V.read(V.DOC/'S0_RESULT.json');assert current['status']=='passed'
    dep=P.dependencies(P.SOURCE,V.read(V.ROOT/'SOURCE_MANIFEST.json')['dependency_lock'])
    assert dep['passed'],'actual dependency content changed'
    result={'stage':'A','status':'passed','finite_continuation':{'path':str(V.DOC/'S0_RESULT.json'),'identity':V.fid(V.DOC/'S0_RESULT.json')},
        'dependencies':dep,'new_cpp_compiles':0,'new_cpp_executions':0,'new_timing_samples':0,
        'scope':'Current154/14/tool ABI/SAN/actual11include trees+3licenses/lease/resource admission only; no platform test result.',
        'reuse':'Identical C14 control evidence retained; R1 must pass its own new changed-code gates. No R1 codegen recompile.',
        'M1_v1_closed':True,'next':'Review exact R1 native request and source/test safety; execute exactly requested5normal+5SAN under original shared lease. Diagnostic still awaits independent freeze admission.'}
    with (V.DOC/'A_RESULT.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
    print(json.dumps({'status':'passed','include_trees':len(dep['trees']),'licenses':len(dep['licenses'])}))
if __name__=='__main__':main()
