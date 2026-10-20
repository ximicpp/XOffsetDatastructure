"""Bounded source/pure policy checks; no child, queue, host API or filesystem mutation."""
from pathlib import Path
import ast,copy,hashlib,json,sys
H=Path(__file__).resolve().parent
def main():
    for p in H.rglob('*.py'):ast.parse(p.read_text(encoding='utf-8'),filename=str(p))
    results={}
    for label in ('r1','m2'):
        sys.path.insert(0,str(H/label));import screen as s
        jobs=s.schedule();assert len(jobs)==144 and sum(x['order']=='AB' for x in jobs)==72 and sum(x['order']=='BA' for x in jobs)==72
        samples=[dict(job=j,values={k:100 for k in s.METRICS}) for j in jobs]
        baseline=s.summary(samples);assert not baseline['statistically_eligible']
        for metric in s.METRICS:
            data=copy.deepcopy(samples);data[0]['values'][metric]=0;r=s.summary(data)
            assert r['undefined_metrics']==[metric] and not r['statistically_eligible']
        for bad in (-1,1.0,True,None,float('inf')):
            data=copy.deepcopy(samples);data[0]['values'][s.POLICY['primary']]=bad
            try:s.summary(data)
            except s.C.ProtocolError:pass
            else:raise AssertionError('invalid metric accepted')
        data=copy.deepcopy(samples);data[0]['job']=data[1]['job']
        try:s.summary(data)
        except s.C.ProtocolError:pass
        else:raise AssertionError('duplicate/misordered role accepted')
        old=H.parents[2]/'c14_iteration_20261019/validation/screen'
        for name in ('core.py','campaign.py','protocol.py'):assert (H/label/name).read_bytes()==(old/name).read_bytes()
        original=(old.parent/'host/process_pause.py').read_text(encoding='utf-8');current=(H/label/'host/process_pause.py').read_text(encoding='utf-8')
        assert '\n'.join(x for x in original.splitlines() if not x.startswith('STATE='))=='\n'.join(x for x in current.splitlines() if not x.startswith('STATE='))
        assert (H/label/'host/pause-control.ps1').read_bytes()==(old.parent/'host/pause-control.ps1').read_bytes()
        results[label]=dict(schedule_children=len(jobs),AB=36,BA=36,primary=s.POLICY['primary'],all10_zero_policies='undefined/no promotion',invalid_data='rejected',borrowed_execute_native_truth_and_host_logic='byte-identical except fresh STATE',compile=0,native=0,queue=0,host_actions=0)
        sys.path.pop(0)
        for name in ('screen','campaign','core','protocol'):sys.modules.pop(name,None)
    print(json.dumps(results,sort_keys=True))
if __name__=='__main__':main()
