"""Source-only exact derivation of the existing business/package workers.

This authoring entry does not import a project module or launch a child.
"""
from pathlib import Path
import ast, hashlib, json, shutil

HERE=Path(__file__).resolve().parent
OLD=Path('C:/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/build/receive-completion-20261019')

def fid(p):
    b=Path(p).read_bytes(); return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}

def replace(s,a,b):
    assert a in s, a
    return s.replace(a,b)

def main():
    output=HERE/'workers'; output.mkdir(exist_ok=False)
    records={}
    names=('functional-coordinator.py','compatibility-worker.py','business_common.py',
      'bench-profiles-worker.py','bench-gates.py','profiles-reuse-v1.py',
      'prepare-functional-tools.py','dispatch.py','v1-compatibility.py','v8-direct-compatibility.py',
      'alpha-S-compatibility.py','resize-compatibility.py','string-compatibility.py',
      'new-API-compatibility.py','AB-compatibility.py','C-compatibility.py')
    for n in names:
        src=OLD/'business-v2'/n; s=src.read_text(encoding='utf-8')
        s=s.replace("sys.path.insert(0, str(Path(__file__).resolve().parents[1]))", "sys.path.insert(0, str(Path(__file__).resolve().parent))")
        if n=='business_common.py':
            s=replace(s,"SOURCE = B / 'final-v4/source'", "SOURCE = Path('/root/xoffset-c14-iteration/20261019/v1/validation/m1-full-v2-normal/source')\nBENCH_META = B / 'metadata'")
            s=replace(s,"IDENTITY_FILE = C / 'candidate-source-identity-v2.json'", "IDENTITY_FILE = C / 'candidate-source-identity-v2.json'")
            s=replace(s,"MATERIALIZATION_FILE = B / 'final-source-identity-v4.json'", "MATERIALIZATION_FILE = B / 'source-identity.json'")
            s=replace(s,"CANDIDATE_ID = 'xoffset-receive-completion-v1'", "CANDIDATE_ID = 'xoffset-c14-m1-default-scalar-v2'")
            s=replace(s,"BASELINE_SOURCE = B / 'baseline/source'", "BASELINE_SOURCE = Path('/root/xoffset-receive-completion/20261019/v1/baseline/source')")
            s=replace(s,"record=read(C/'baseline.json')", "record=read(R/'docs/coordination/receive-completion-20261019/baseline.json')")
            start=s.index('def full_record(kind):'); end=s.index('\ndef full_build',start)
            s=s[:start]+'''def full_record(kind):
    require(kind in ('normal','sanitized'), 'Invalid full profile kind')
    identity=require_final_identity(); record=INPUTS['full_profiles'][kind]
    path=match_ref(record['acceptance']); value=read(path)
    receipt_path=Path(value['receipt']['path']); receipt=read(receipt_path)
    require(fid(receipt_path)==value['receipt']['identity'] and receipt['status']=='passed', 'Full receipt differs')
    require(value['source']==str(SOURCE) and value['production_files']==identity['production_files'], 'Full source identity differs')
    require(value['ctest_count']==73 and len(receipt['junit_names'])==73 and len(set(receipt['junit_names']))==73, 'Full73 inventory required')
    require(receipt['production_files']==identity['production_files'], 'Full production closure differs')
    require(Path(record['build'])==receipt_path.parent/'build', 'Full build path differs')
    for n,v in identity['production_files'].items(): require(fid(receipt_path.parent/'source'/n)==v, 'Full snapshot differs: '+n)
    require(fid(Path(record['build'])/'compile_commands.json')==receipt['compile_commands_identity'], 'Full compile database differs')
    return record,value
''' +s[end:]
        if n=='bench-gates.py':
            s=replace(s,"str(source / 'experiments/serialization_comparison')", "str(BENCH_META / 'experiments/serialization_comparison')")
        if n=='profiles-reuse-v1.py':
            s=replace(s,"SOURCE / 'tools/serialization_comparison/protocol.py'", "BENCH_META / 'tools/serialization_comparison/protocol.py'")
            s=replace(s,"SOURCE / 'experiments/serialization_comparison/driver.cpp'", "BENCH_META / 'experiments/serialization_comparison/driver.cpp'")
            s=replace(s,"SOURCE / 'experiments/serialization_comparison/adapters/xoffset/adapter.hpp'", "BENCH_META / 'experiments/serialization_comparison/adapters/xoffset/adapter.hpp'")
            # Explicit current adapter/protocol identity belongs in the capture closure.
            s=replace(s,"closure.update({'candidate-source-tree': str(SOURCE),", "closure.update({'candidate-adapter': str(BENCH_META / 'experiments/serialization_comparison/adapters/xoffset/adapter.hpp'), 'candidate-protocol': str(BENCH_META / 'tools/serialization_comparison/protocol.py'), 'candidate-source-tree': str(SOURCE),")
        if n=='prepare-functional-tools.py': s=replace(s,"SOURCE / 'tools/serialization_comparison'", "BENCH_META / 'tools/serialization_comparison'")
        if n=='functional-coordinator.py':
            s=replace(s,"SOURCE / 'tools/serialization_comparison'", "BENCH_META / 'tools/serialization_comparison'")
            s=replace(s,"str(R / 'experiments/vm_coordination/run_job.py')", "str(RUNNER)")
        dest=output/n; dest.write_text(s,encoding='utf-8',newline='\n');ast.parse(s)
        records[n]={'original':str(src),'before':fid(src),'after':fid(dest)}
    package=output/'package';shutil.copytree(OLD/'package-archive-repair-v2a',package)
    # Keep immutable original data/fixtures; only the input binder is local.
    source=package/'support.py'; text=source.read_text(encoding='utf-8')
    start=text.index('def get_spec(stage):'); end=text.index('\ndef command(',start)
    text=text[:start]+'''def get_spec(stage):
    global _INPUT_SPEC
    ap=argparse.ArgumentParser();ap.add_argument('--spec',type=Path,required=True);args=ap.parse_args()
    spec=read(args.spec);_INPUT_SPEC=spec
    need(spec['kind']=='c14-m1-package-input-v1','exact M1 package input')
    source=Path(spec['source']);need(source==Path('/root/xoffset-c14-iteration/20261019/v1/validation/m1-full-v2-normal/source'),'explicit M1 full source')
    own=headers(spec['own_headers']);check_production(source,spec['production_files'])
    for mode,ref in spec['full_receipts'].items():
        need(fid(ref['path'])==ref['identity'],'full receipt identity changed')
        r=read(ref['path']);need(r['status']=='passed' and r['production_files']==spec['production_files'] and len(r['junit_names'])==73,'candidate full73 required')
    for p,v in spec['bindings'].items():need(fid(p)==v,'tool/input binding changed: '+p)
    out=B/('package-'+stage+'-v2');out.mkdir(parents=True,exist_ok=False)
    write(out/'authority.json',P.queue_authority(B/'queues'/('package-'+stage+'-v2')))
    write(out/'input.json',{'spec':fid(args.spec),'production_files':spec['production_files']})
    return spec,source,out,own

def check_production(source,production):
    for name,value in production.items():need(fid(source/name)==value,'production changed: '+name)
    if _INPUT_SPEC:
        for p,v in _INPUT_SPEC['bindings'].items():need(fid(p)==v,'bound input changed: '+p)

''' +text[end:]
    text=replace(text,'from common import B,H,P,P1,env,fid,read,write','from common import B,H,P,P1,env,fid,read,write')
    source.write_text(text,encoding='utf-8',newline='\n');ast.parse(text)
    (HERE/'derivation.json').write_text(json.dumps({'schema_version':1,'business':records,
      'package_original':str(OLD/'package-archive-repair-v2a'),
      'scope':'Source-only tool derivation; zero compilation/native/queue/performance.'},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')

if __name__=='__main__':main()
