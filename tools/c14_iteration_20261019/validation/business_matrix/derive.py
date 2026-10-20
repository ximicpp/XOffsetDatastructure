"""Pure one-time derivation; preserves original matrix model/check logic."""
from pathlib import Path
import ast,hashlib,json
H=Path(__file__).resolve().parent
OLD=Path('C:/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/build/receive-completion-20261019/matrix-execution-v2')
def fid(p):
    b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def main():
    text=(OLD/'worker.py').read_text(encoding='utf-8')
    text=text.replace('sys.path.insert(1,str(HERE.parent))','sys.path.insert(0,str(HERE))')
    text=text.replace("SOURCE=B/'final-v4/source';MATERIALIZATION=B/'final-source-identity-v4.json'", "SOURCE=B/'source';MATERIALIZATION=B/'source-identity.json'")
    start=text.index('def prerequisite(spec):');end=text.index('\ndef build_binding',start)
    text=text[:start]+'''def prerequisite(spec):
    need(spec['kind']=='c14-m1-production-matrix-input-v1','current M1 input required')
    source=Path(spec['source']);need(source==SOURCE,'exact current matrix source')
    exact(MATERIALIZATION,spec['materialization']['identity'])
    record=K.read(MATERIALIZATION);production=record['production_files']
    exact(CANDIDATE_IDENTITY,spec['candidate_identity']['identity'])
    need(K.read(CANDIDATE_IDENTITY)['production_files']==production and len(production)==14,'current production14 identity')
    for mode,ref in spec['full_receipts'].items():
        exact(ref['path'],ref['identity']);r=K.read(ref['path'])
        need(r['status']=='passed' and r['production_files']==production and len(r['junit_names'])==len(set(r['junit_names']))==73,'current actual full73 '+mode)
    for path,value in spec['bindings'].items():exact(path,value)
    for name,value in spec['source_members'].items():exact(source/name,value)
    need(Path(spec['reference']['path'])==REFERENCE and spec['reference']['identity']==REFERENCE_ID,'fixed original reference')
    exact(REFERENCE,REFERENCE_ID)
    return source,production
''' +text[end:]
    text=text.replace("verify_source(source,production,business);exact(REFERENCE,spec['reference']['identity'])", "verify_source(source,production,business);exact(REFERENCE,spec['reference']['identity'])\n        for path,value in spec['bindings'].items():exact(path,value)\n        for name,value in spec['source_members'].items():exact(source/name,value)")
    ast.parse(text);(H/'worker.py').write_text(text,encoding='utf-8',newline='\n')
    (H/'checks.py').write_bytes((OLD/'checks.py').read_bytes())
    (H/'DERIVATION.json').write_text(json.dumps({'original_worker':fid(OLD/'worker.py'),'current_worker':fid(H/'worker.py'),
      'checks_original_equal_current':fid(OLD/'checks.py'),'changes':['local import path','current SOURCE/MATERIALIZATION paths','M1 actual154/14/full73 prerequisites','before-after complete input member checks'],
      'model_native_schedule_parser_changed':False},indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
