"""Small metadata-only R6 spec construction; root executes, no queue here."""
from pathlib import Path
import argparse,ast,json,re,stat,sys
HERE=Path(__file__).resolve().parent
sys.path.insert(1,str(HERE.parent))
from common import B,C,H,P,OLD_H,fid,read,write
from patch import headers

SOURCE_TOOLS=('tools/validate_vm_package.py','tools/prepare_alpha_release.py',
 'cmake/XOffsetFourPathHeaders.cmake','cmake/XOffsetInstall.cmake',
 'cmake/XOffsetDatastructureConfig.cmake.in','cmake/XOffsetDatastructureConfigVersion.cmake.in',
 'VERSION','CHANGELOG.md','docs/ALPHA_COMPATIBILITY.md')
LABELS={'package':'package-standard-v2','runtime':'package-runtime-v2','archive':'package-clean-archive-v2'}

def need(ok,message):
    if not ok:raise ValueError(message)
def regular(path,expected=None):
    path=Path(path);need(stat.S_ISREG(path.lstat().st_mode),'regular file required: '+str(path))
    value=fid(path)
    if expected is not None:need(value==expected,'identity changed: '+str(path))
    return value
def members():
    receipt=read(HERE/'receipt.json');result={item['path']:{'bytes':item['bytes'],'sha256':item['sha256']} for item in receipt['files']}
    need(len(result)==len(receipt['files']),'author member uniqueness')
    for name,value in result.items():regular(HERE/name,value)
    fixtures={n:v for n,v in result.items() if n.startswith('runtime-fixtures/')}
    need(len(fixtures)==len(list((HERE/'runtime-fixtures').iterdir()))==14,'original fourteen runtime fixtures')
    return result,fixtures
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--source',type=Path,required=True);ap.add_argument('--materialization',type=Path,required=True)
    ap.add_argument('--candidate',type=Path,required=True)
    ap.add_argument('--normal-gate',type=Path,required=True);ap.add_argument('--sanitized-gate',type=Path,required=True)
    ap.add_argument('--out',type=Path,default=C/'package-archive-repair-spec-v2.json');args=ap.parse_args()
    source=args.source;material=args.materialization;candidate=args.candidate
    need(source.is_absolute() and source.parent.parent==B and source.name=='source' and
         re.fullmatch(r'final-v[1-9][0-9]*',source.parent.name) is not None,'explicit selected final-vN source path')
    need(material.is_absolute() and material.parent==B and candidate.is_absolute() and C in candidate.parents,
         'explicit source/candidate metadata boundaries')
    tools,fixtures=members();record=read(material)
    need(record['source']==str(source),'selected materialization source path')
    production=record['production_files'];need(production==record['identity']['production_files'] and len(production)==14,'actual fourteen production files')
    own=headers(record['own_headers']);need(set(own)=={n for n in production if n.endswith('.hpp') and n!='xoffset_virtual_memory.hpp'},'actual eleven own header closure')
    for name,value in production.items():regular(source/name,value)
    need(read(candidate)['production_files']==production,'candidate production identity')
    gates={};names=None
    for profile in ('normal','sanitized'):
        path=getattr(args,profile+'_gate');need(path.is_absolute() and B in path.parents,'full gate scope')
        gate=read(path)
        need(gate['status']=='passed' and gate['source']==str(source) and gate['production_files']==production,'actual accepted selected full gate '+profile)
        actual=gate['junit_names'];need(type(gate['ctest_count']) is int and gate['ctest_count']>=71 and
            gate['ctest_count']==len(actual)==len(set(actual)) and gate['configure_check_count']==3,
            'explicit complete full inventory plus configure3')
        if names is None:names=sorted(actual)
        else:need(names==sorted(actual),'normal/SAN inventories differ')
        gates[profile]={'path':str(path),'identity':regular(path)}
    for relative in ('tools/validate_vm_package.py','tools/prepare_alpha_release.py'):
        nodes=[n for n in ast.parse((source/relative).read_text()).body if isinstance(n,ast.Assign) and
            any(isinstance(x,ast.Name) and x.id=='FULL_OWN_HEADERS' for x in n.targets)]
        need(len(nodes)==1 and ast.literal_eval(nodes[0].value)==own,'actual package audit header tuple '+relative)
    cmake=(source/'cmake/XOffsetFourPathHeaders.cmake').read_text()
    match=re.fullmatch(r'\s*set\(XOFFSET_FULL_OWN_HEADERS\s+(.+?)\)\s*',cmake,re.S)
    need(match is not None and tuple(match.group(1).split())==own,'actual CMake install header list')
    need((source/'cmake/XOffsetInstall.cmake').read_text().count('foreach(header IN LISTS XOFFSET_FULL_OWN_HEADERS)')==1,'shared exact install list')
    shared={str(p):regular(p) for p in (H/'common.py',OLD_H/'common.py',H.parents[0]/'performance-v8-20261018/common.py',Path(P.__file__),
       H.parents[1]/'experiments/vm_coordination/run_job.py')}
    need(set(('test_verified_receive','test_receive_raw','test_core_lifetime','test_custom_future_lifetime')).issubset(names),
         'required original completion targets missing')
    spec={'schema_version':1,'kind':'receive-completion-package-input','source':str(source),
      'source_identity':{'path':str(material),'identity':regular(material)},
      'candidate_identity':{'path':str(candidate),'identity':regular(candidate)},'full_gates':gates,
      'production_files':production,'own_headers':list(own),'expected_full_tests':names,'expected_full_test_count':len(names),
      'required_tests':['test_verified_receive','test_receive_raw','test_core_lifetime','test_custom_future_lifetime'],
      'source_tools':{n:regular(source/n) for n in SOURCE_TOOLS},
      'execution_author_receipt':regular(HERE/'receipt.json'),'execution_tools':tools,
      'runtime_fixture_files':fixtures,'shared_tools':shared,
      'outputs':{stage:str(B/label) for stage,label in LABELS.items()},
      'queues':{stage:str(B/'queues'/label) for stage,label in LABELS.items()},
      'queue_timeouts':{'package':2400,'runtime':2400,'archive':3600},
      'full_dependency_scope':{'required_default_count':9,'separate_first_include_object_count_per_profile':11,
          'optional_default_headers':['validation_scratch.hpp','xoffset_validation_scratch.hpp'],
          'first_include_native_executions':0},
      'performance_samples':0,'source_or_old_evidence_modified':False}
    write(args.out,spec);print(args.out,flush=True)

if __name__=='__main__':main()
