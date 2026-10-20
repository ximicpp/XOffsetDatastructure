"""Materialize only the root-selected exact R1+M2 patch union; no execution."""
import ast
import difflib
import hashlib
import json
from pathlib import Path

R = Path(__file__).resolve().parents[3]
D = R/'docs/coordination/c14-iteration-20261020-v2'
OUT = R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1'

def fid(p):
    b=p.read_bytes(); return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}

def read(p): return json.loads(p.read_text(encoding='utf-8'))

def new(p,v):
    with p.open('x',encoding='utf-8') as f: json.dump(v,f,indent=2,ensure_ascii=False); f.write('\n')

def main():
    selection=D/'reviews/FINAL_CANDIDATE_SELECTION.json'
    assert fid(selection)['sha256']=='bcbb7c0eaf6f59fe60ee824b285f576f98fb01f4415a8bdb0231915067c18345'
    s=read(selection)
    assert s['source_creation_authorized'] and s['selected_candidate']==OUT.name and not OUT.exists()
    for x in s['components']:
        p=R/x['path'].split('/xoffsetdatastructure开发/',1)[1]; assert fid(p)==x['identity']
    material=R/'handoff/github-20261019/C14_MATERIALIZATION.json'; names=read(material)['non_external_files']
    originals={n:(R/n).read_bytes() for n in names}
    for n,b in originals.items(): assert {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}==names[n]
    components={}
    for label,rel,pname in [('R1','build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1','R1.patch'),('M2','build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1','M2.patch')]:
        p=R/rel; identity=read(p/'IDENTITY.json'); assert len(identity['members'])==154
        for n,v in identity['members'].items(): assert fid(p/'source'/n)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']}
        assert fid(p/pname)['sha256']==identity['patch_sha256']
        components[label]={'identity':{'path':rel+'/IDENTITY.json','identity':fid(p/'IDENTITY.json')},'patch':{'path':rel+'/'+pname,'identity':fid(p/pname)}}
    h='xoffsetdatastructure.hpp'; baseline=originals[h].decode('utf-8')
    r1=(R/'build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/source'/h).read_text(encoding='utf-8')
    m2=(R/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/source'/h).read_text(encoding='utf-8')
    tree=ast.parse((Path(__file__).parent/'prepare_m2.py').read_text(encoding='utf-8'))
    literals={n.targets[0].id:ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and isinstance(n.targets[0],ast.Name) and n.targets[0].id in ('old','new')}
    old,updated=literals['old'],literals['new']
    assert baseline.count(old)==r1.count(old)==1 and baseline.replace(old,updated,1)==m2
    combined=r1.replace(old,updated,1)
    # Independent reverse order: R1 is precisely two insertions before the M2 hunk.
    groups=[]
    for line in (R/components['R1']['patch']['path']).read_text(encoding='utf-8').splitlines(True):
        if line.startswith('@@'): groups.append([])
        elif line.startswith('+') and not line.startswith('+++'): groups[-1].append(line[1:])
        elif line.startswith('-') and not line.startswith('---'): raise ValueError('R1 must be insertion-only')
    assert len(groups)==2
    positions=[baseline.index('            const auto address = reinterpret_cast<std::uintptr_t>(anchor);'),
               baseline.index('        XGrowPolicy growth_policy() const noexcept')]
    assert all(x<baseline.index(old) for x in positions)
    edits=list(zip(positions,map(''.join,groups)))
    check=baseline
    for pos,insertion in reversed(edits): check=check[:pos]+insertion+check[pos:]
    assert check==r1
    reverse=m2
    for pos,insertion in reversed(edits): reverse=reverse[:pos]+insertion+reverse[pos:]
    assert reverse==combined
    members={}; source=OUT/'source'; source.mkdir(parents=True)
    for n,b in originals.items():
        raw=combined.encode('utf-8') if n==h else b; p=source/n; p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
        members[n]={'baseline_sha256':hashlib.sha256(b).hexdigest(),'candidate_sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw),'changed':raw!=b}
    patch=''.join(difflib.unified_diff(baseline.splitlines(True),combined.splitlines(True),fromfile='a/'+h,tofile='b/'+h)).encode('utf-8')
    (OUT/'combined.patch').write_bytes(patch)
    identity={'schema_version':1,'candidate':OUT.name,'baseline_commit':s['baseline_commit'],'materialization_sha256':fid(material)['sha256'],
              'source':str(source),'member_count':154,'changed':[h],'patch_sha256':hashlib.sha256(patch).hexdigest(),'members':members,
              'components':components,'root_selection':{'path':'docs/coordination/c14-iteration-20261020-v2/reviews/FINAL_CANDIDATE_SELECTION.json','identity':fid(selection)},
              'exact_two_patch_union_both_orders_equal':True,'M1_L1_included':False,'native_executed':False,'integrated':False}
    new(OUT/'IDENTITY.json',identity);new(D/'core/COMBINED_IDENTITY.json',identity)
    new(D/'core/COMBINED_SOURCE_PROOF.json',{'status':'source_materialized_not_compiled','candidate_identity':fid(OUT/'IDENTITY.json'),
        'candidate_main':fid(source/h),'combined_patch':fid(OUT/'combined.patch'),'component_pins':components,'two_application_orders_equal':True,
        'root_baseline154_before_after_equal':all(fid(R/n)==v for n,v in names.items()),'source_members':154,'production_files':14,'new_compile':0,'new_native':0})
    print(json.dumps({'candidate':OUT.name,'main':fid(source/h),'patch':fid(OUT/'combined.patch'),'identity':fid(OUT/'IDENTITY.json'),'native':0}))

if __name__=='__main__': main()
