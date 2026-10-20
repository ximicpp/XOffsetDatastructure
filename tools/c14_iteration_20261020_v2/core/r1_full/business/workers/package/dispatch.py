"""Root's existing queue dispatcher; never call this from an active lease."""
from pathlib import Path
import argparse,re,sys
HERE=Path(__file__).resolve().parent
sys.path.insert(1,str(HERE.parent))
from common import B,C,fid,read,write,queue

def need(ok,message):
    if not ok:raise ValueError(message)
def validate(spec):
    source=Path(spec['source'])
    need(source.is_absolute() and source.parent.parent==B and source.name=='source' and
         re.fullmatch(r'final-v[1-9][0-9]*',source.parent.name) is not None,'explicit final-vN source')
    need(fid(HERE/'receipt.json')==spec['execution_author_receipt'],'execution receipt pin')
    author=read(HERE/'receipt.json')
    expected={item['path']:{'bytes':item['bytes'],'sha256':item['sha256']} for item in author['files']}
    need(len(expected)==len(author['files']) and spec['execution_tools']==expected,'complete execution member set')
    for name,value in spec['execution_tools'].items():need(fid(HERE/name)==value,'execution source changed '+name)
    for path,value in spec['shared_tools'].items():need(fid(Path(path))==value,'shared source changed '+path)
    for name,value in {**spec['production_files'],**spec['source_tools']}.items():need(fid(Path(spec['source'])/name)==value,'selected source changed '+name)
    for key in ('source_identity','candidate_identity'):
        value=spec[key];need(fid(Path(value['path']))==value['identity'],'source metadata changed '+key)
    material=read(Path(spec['source_identity']['path']))
    need(material['source']==str(source) and material['production_files']==spec['production_files'] and
         material['identity']['production_files']==spec['production_files'] and len(spec['production_files'])==14,
         'materialization production closure')
    need(read(Path(spec['candidate_identity']['path']))['production_files']==spec['production_files'],'candidate production closure')
    for profile,value in spec['full_gates'].items():
        gate=read(Path(value['path']))
        need(fid(Path(value['path']))==value['identity'] and gate['status']=='passed' and
             gate['source']==str(source) and gate['production_files']==spec['production_files'] and
             type(gate['ctest_count']) is int and gate['ctest_count']==spec['expected_full_test_count'] and
             sorted(gate['junit_names'])==spec['expected_full_tests'] and gate['configure_check_count']==3,
             'accepted actual selected full gate changed '+profile)
    need(type(spec['expected_full_test_count']) is int and spec['expected_full_test_count']>=71 and
         len(spec['expected_full_tests'])==len(set(spec['expected_full_tests']))==spec['expected_full_test_count'],
         'actual full inventory count')
    labels={'package':'package-standard-v2','runtime':'package-runtime-v2','archive':'package-clean-archive-v2'}
    need(spec['outputs']=={s:str(B/label) for s,label in labels.items()} and
         spec['queues']=={s:str(B/'queues'/label) for s,label in labels.items()} and
         spec['queue_timeouts']=={'package':2400,'runtime':2400,'archive':3600},'fixed fresh v2 stage boundaries/budgets')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--spec',type=Path,default=C/'package-archive-repair-spec-v2.json')
    ap.add_argument('--stage',choices=('package','runtime','archive','all'),required=True);args=ap.parse_args()
    spec=read(args.spec);validate(spec)
    stages=('package','runtime','archive') if args.stage=='all' else (args.stage,)
    results={}
    for stage in stages:
        validate(spec);out=Path(spec['outputs'][stage]);label=Path(spec['queues'][stage]).name
        need(not out.exists() and not Path(spec['queues'][stage]).exists(),'fresh stage only; no retries '+stage)
        queue(label,HERE/(stage+'-worker.py'),['--spec',str(args.spec)],timeout=spec['queue_timeouts'][stage])
        value=read(out/'result.json');need(value['status']=='passed' and value['source_identity']==spec['source_identity'],'actual worker passed/source '+stage)
        results[stage]={'path':str(out/'result.json'),'identity':fid(out/'result.json')}
    write(C/('package-repair-'+args.stage+'-acceptance-v2.json'),{
      'schema_version':1,'status':'passed','spec':fid(args.spec),'stages':results,
      'no_retry':True,'performance_samples':0,'no_publication':True})

if __name__=='__main__':main()
