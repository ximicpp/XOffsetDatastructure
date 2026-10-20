"""Exact source-only identity derivation of retained M2 functional workers."""
import ast,difflib,hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[3];H=Path(__file__).resolve().parent
old=H/'m2_full';out=H/'combined_full'
if out.exists():raise ValueError('fresh combined tooling required')
out.mkdir()
def fid(p):
 b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
swaps=[('m2-exact-owns-v1','r1-m2-combined-v1'),('m2_full','combined_full'),('m2-full','combined-full'),('m2-business','combined-business'),('c14-m2-','c14-combined-'),('M2','COMBINED')]
records={};patch=[]
for rel in json.loads((old/'DERIVATION.json').read_text(encoding='utf-8'))['files']:
 p=old/rel;t=out/rel;t.parent.mkdir(parents=True,exist_ok=True);raw=p.read_bytes()
 if p.suffix=='.py':
  before=raw.decode('utf-8');s=before
  for a,b in swaps:s=s.replace(a,b)
  if rel=='full_entry.py':s=s.replace("('M1','L1','COMBINED','COMBINED')","('COMBINED',)")
  if rel=='business/prepare.py':
   marker="fr['request_identity']==result['request_identity']"
   assert s.count(marker)==1;s=s.replace(marker,"fr['request_identity']==result['initial_request_identity']")
  ast.parse(s);t.write_text(s,encoding='utf-8',newline='\n')
  if s!=before:patch.extend(difflib.unified_diff(before.splitlines(True),s.splitlines(True),fromfile='M2/'+rel,tofile='COMBINED/'+rel))
 else:t.write_bytes(raw)
 records[rel]={'original':str(p),'before':fid(p),'after':fid(t)}
for name in ('business','business_matrix'):
 base=out/name;(base/'TOOL_MANIFEST.json').write_text(json.dumps({'schema_version':1,'files':{p.relative_to(base).as_posix():fid(p) for p in sorted(base.rglob('*')) if p.is_file()}},indent=2)+'\n',encoding='utf-8')
(out/'DERIVATION.json').write_text(json.dumps({'files':records,'source_only':True,'swaps':swaps,'changes':'Only candidate/source/output/doc/request literals; combined initial-vs-full request bridge explicit. All C++ fixtures, oracles, workload, flags, caps and shared supervisor preserved.'},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
(out/'TOOLS.patch').write_text(''.join(patch),encoding='utf-8')
print(json.dumps({'files':len(records),'unchanged_cpp_fixtures':sum(Path(n).suffix in ('.cpp','.hpp','.h') and v['before']==v['after'] for n,v in records.items()),'compiles':0,'native':0}))
