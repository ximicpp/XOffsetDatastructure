"""Source-only M2 instantiation of the retained R1 functional workers.

R1 is running: never modify it. Original C++/oracle/fixture bytes remain exact.
"""
import ast
import difflib
import hashlib
import json
from pathlib import Path

r=Path(__file__).resolve().parents[3]
h=Path(__file__).resolve().parent
old=h/'r1_full';out=h/'m2_full'
if out.exists():raise ValueError('fresh M2 tool directory required')
out.mkdir()
def fid(p):
    b=p.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
swaps=[('r1-canonical-hotcold-v1','m2-exact-owns-v1'),
       ('build/c14-iteration-20261019-v1/candidates/m2-exact-owns-v1','build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1'),
       ('validation/r1_full','validation/m2_full'),('core/REQUESTS_R1_FULL.json','core/REQUESTS_M2_FULL.json'),
       ('r1-full-v1-','m2-full-v1-'),('r1-business','m2-business'),
       ('xoffset-c14-r1-canonical-hotcold-v1','xoffset-c14-m2-exact-owns-v1'),
       ('R1','M2'),('c14-r1-','c14-m2-')]
records={};patch=[]
for p in sorted(old.rglob('*')):
    if not p.is_file() or p.name in ('TOOL_MANIFEST.json','DERIVATION.json','TOOLS.patch') or '__pycache__' in p.parts:continue
    rel=p.relative_to(old).as_posix();target=out/rel;target.parent.mkdir(parents=True,exist_ok=True)
    if p.suffix=='.py':
        original=p.read_text(encoding='utf-8');text=original
        for a,b in swaps:text=text.replace(a,b)
        if rel=='business/prepare.py':
            marker="    binding[str(ident)]=stable(ident)"
            if text.count(marker)!=1:raise ValueError('exact launcher binding insertion')
            text=text.replace(marker,marker+'''
    launcher=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
    launcher_id={'bytes':61,'sha256':'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}
    need(stable(launcher)==launcher_id,'explicit current 61-byte outer compiler launcher differs')
    binding[str(launcher)]=launcher_id
''',1)
        ast.parse(text);target.write_text(text,encoding='utf-8',newline='\n')
        if text!=original:patch.extend(difflib.unified_diff(original.splitlines(True),text.splitlines(True),fromfile='R1/'+rel,tofile='M2/'+rel))
    else:target.write_bytes(p.read_bytes())
    records[rel]={'original':str(p),'before':fid(p),'after':fid(target)}
for name in ('business','business_matrix'):
    base=out/name
    (base/'TOOL_MANIFEST.json').write_text(json.dumps({'schema_version':1,'files':{p.relative_to(base).as_posix():fid(p) for p in sorted(base.rglob('*')) if p.is_file()}},indent=2)+'\n',encoding='utf-8')
(out/'DERIVATION.json').write_text(json.dumps({'files':records,'source_only':True,'changes':'Exact M2 identity/source/doc/output labels; explicit 61-byte compiler launcher added to business PREPARED bindings inherited by all workers/matrix. C++ fixtures/oracles/schedules/limits unchanged. R1 tools not edited.','swaps':swaps},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
(out/'TOOLS.patch').write_text(''.join(patch),encoding='utf-8')
print(json.dumps({'derived_files':len(records),'cpp_execution':0,'R1_modified':False}))
