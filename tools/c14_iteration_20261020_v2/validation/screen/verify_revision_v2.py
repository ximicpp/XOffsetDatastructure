"""Final bounded read-only AST/ref check of published revision; no native/queue."""
from pathlib import Path
import ast,hashlib,json
ROOT=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发');H=ROOT/'tools/c14_iteration_20261020_v2/validation/screen';D=ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/screen'
def fid(p):
    b=Path(p).read_bytes();return dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def read(p):return json.loads(Path(p).read_text())
def check(ref):
    actual=fid(ref['path']);expected=ref['identity'];assert actual['sha256']==expected['sha256'] and actual['bytes']==int(expected['bytes']),(ref['path'],actual,expected)
def main():
    for p in H.rglob('*.py'):ast.parse(p.read_text(),filename=str(p))
    ready=read(D/'SOURCE_READY_v2.json')
    for item in ready['previous_unexecuted_drafts']['candidates'].values():
        for record in item.values():check(record)
    check(ready['previous_unexecuted_drafts']['source_ready'])
    for label in ('R1','M2'):
        check(ready['requests'][label]);check(ready['tool_manifests'][label]);manifest=read(ready['tool_manifests'][label]['path'])
        for record in manifest['files'].values():check(record)
        request=read(ready['requests'][label]['path']);design=read(ROOT/f'docs/coordination/c14-iteration-20261020-v2/core/SCREEN_DESIGN_{label}.json')
        assert request['statistics']['metrics']==design['statistics']['metrics'] and request['statistics']['primary']==design['statistics']['primary'] and request['statistics']['auxiliary']==design['statistics']['auxiliary']
    check(ready['exact_R1_resource_capture_request']);check(ready['exact_R1_resource_capture_dispatch'])
    request=read(ready['exact_R1_resource_capture_request']['path'])
    for p,v in request['runtime']['tools'].items():check(dict(path=p,identity=v))
    check(request['config']);config=read(request['config']['path']);check(config['preconditions']);pre=read(config['preconditions']['path'])
    for name in ('affinity_observation','cost_policy','functional_summary','full_request','input_policy'):check(pre[name])
    check(pre['control_manifest_exception']['provenance']);check(config['input_policy'])
    report=dict(status='passed_read_only_revision_binding',source_ready=dict(path=str(D/'SOURCE_READY_v2.json'),identity=fid(D/'SOURCE_READY_v2.json')),request=dict(path=str(D/'R1/RESOURCE_CAPTURE_REQUEST.json'),identity=fid(D/'R1/RESOURCE_CAPTURE_REQUEST.json')),published_current_tools_match=True,metrics_from_design_exact=True,old_draft_bytes_preserved=True,capture_runtime_tools_and_config_inputs_unchanged=True,new_compiles=0,new_native=0,new_queues=0,host_actions=0)
    output=D/'FINAL_SOURCE_CHECK_v2.json'
    with output.open('x') as stream:json.dump(report,stream,indent=2);stream.write('\n')
    print(json.dumps(report))
if __name__=='__main__':main()
