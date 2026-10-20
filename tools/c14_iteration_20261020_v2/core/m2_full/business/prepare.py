"""Prepare M2 inputs/metadata only, under the existing global validation lease.

No compiler, native child, campaign or queue is started by this entry.
"""
from pathlib import Path
import ast, hashlib, importlib.util, json, shutil, sys
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent
sys.path.insert(0,str(H/'workers'))
from common import *

def need(ok,message):
    if not ok:raise ValueError(message)

def stable(path):
    value=fid(path);return {'bytes':int(value['bytes']),'sha256':value['sha256']}

def patch_literal(path,name,value):
    text=path.read_text(); nodes=[]
    for n in ast.parse(text).body:
        if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets):nodes.append(n)
    need(len(nodes)==1,'exact literal '+name)
    n=nodes[0]; lines=text.splitlines(keepends=True)
    lines[n.lineno-1:n.end_lineno]=[name+' = '+repr(value)+'\n']
    path.write_text(''.join(lines));ast.parse(path.read_text())

def main():
    holder=read(R/'build/vm-coordination/queue.lock')
    need(holder['status']=='running' and Path(holder['out'])==B.parent/'leases/m2-business-prepare-v1','original active prepare lease required')
    need(not B.exists(),'fresh business output required')
    doc=C.parent
    result=read(doc/'M2_FULL_SOURCE_BINDING.json'); requests=ROOT/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_M2_FULL.json'
    need(result['status']=='passed' and stable(requests)==result['request_identity'],'current full request identity required')
    prod=result['production_files']['candidate'];need(len(prod)==14,'14 production files')
    source=Path('/root/xoffset-c14-iteration/20261020/v2/validation/m2-full-v1-normal/source')
    full_result=doc/'M2_FULL_RESULT.json'; fr=read(full_result)
    need(fr['status']=='passed' and fr['production_files']==prod and fr['request_identity']==result['request_identity'],'M2 complete normal/SAN required')
    refs={};profiles={};binding={str(requests):stable(requests),str(doc/'M2_FULL_SOURCE_BINDING.json'):stable(doc/'M2_FULL_SOURCE_BINDING.json'),str(full_result):stable(full_result)}
    ident=ROOT/'build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json'
    need(stable(ident)==result['candidate_identity_file'],'core154 identity differs')
    binding[str(ident)]=stable(ident)
    launcher=Path('/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++-libcxx')
    launcher_id={'bytes':61,'sha256':'c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87'}
    need(stable(launcher)==launcher_id,'explicit current 61-byte outer compiler launcher differs')
    binding[str(launcher)]=launcher_id

    for mode in ('normal','sanitized'):
        base=B.parent/('m2-full-v1-'+mode); receipt=base/'receipt.json'; r=read(receipt)
        need(r['status']=='passed' and r['production_files']==prod and len(r['junit_names'])==len(set(r['junit_names']))==73,'actual full73 '+mode)
        inv=read(base/'build/test-inventory.json')
        expected=inv['runtime_test_programs']+inv['subprocess_scenarios']+inv['tooling_checks']
        need(inv['mode']=='full' and sorted(expected)==sorted(r['junit_names']) and inv['configure_check_count']==3,'actual full inventory differs')
        for n,v in result['candidate_members'].items():
            need(stable(base/'source'/n)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'full154 differs '+mode+'/'+n)
            binding[str(base/'source'/n)]=stable(base/'source'/n)
        need(stable(base/'build/compile_commands.json')==r['compile_commands_identity'],'compile database differs')
        binding[str(receipt)]=stable(receipt);refs[mode]={'path':str(receipt),'identity':stable(receipt)}
    B.mkdir();C.mkdir(exist_ok=True)
    old=Path('/root/xoffset-receive-completion/20261019/v1/final-v4/source')
    meta=B/'metadata';meta.mkdir()
    changes={}
    for rel in ('experiments/serialization_comparison','tools/serialization_comparison'):
        shutil.copytree(old/rel,meta/rel)
    original_metadata={}
    for p in meta.rglob('*'):
        if p.is_file():
            rel=p.relative_to(meta)
            need(stable(p)==stable(old/rel),'metadata copy differs: '+str(rel))
            original_metadata[rel.as_posix()]=stable(old/rel)
    identity={'candidate_id':'xoffset-c14-m2-exact-owns-v1','baseline_candidate_id':'xoffset-four-paths-v1',
      'production_files':prod,'source_content_sha256':hashlib.sha256(json.dumps(prod,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
      'core_request':stable(requests),'core_identity':stable(ident),'core_member_count':154}
    identity_path=C/'candidate-source-identity-v2.json';write(identity_path,identity)
    write(B/'source-identity.json',{'source':str(source),'identity':identity,'full_expected_ctest':73})
    adapter=meta/'experiments/serialization_comparison/adapters/xoffset/adapter.hpp'
    before=stable(adapter);text=adapter.read_text()
    import re
    for name,value in (('library_id',identity['candidate_id']),('source_content_sha256',identity['source_content_sha256'])):
        text,count=re.subn(r'(\b'+name+r'\s*=\s*)"[^"\n]+"',lambda m:m[1]+json.dumps(value),text);need(count==1,'adapter literal '+name)
    adapter.write_text(text);changes[str(adapter)]={'before':before,'after':stable(adapter),'allowed':'candidate library_id/source_content_sha256 literals only'}
    protocol=meta/'tools/serialization_comparison/protocol.py';before=stable(protocol)
    module=ast.parse(protocol.read_text());values={}
    for n in module.body:
        if isinstance(n,ast.Assign):
            for t in n.targets:
                if isinstance(t,ast.Name) and t.id=='IDENTITIES':values=ast.literal_eval(n.value)
    values['xoffset']=(identity['candidate_id'],values['xoffset'][1]);patch_literal(protocol,'IDENTITIES',values)
    patch_literal(protocol,'CANDIDATE_CONTENT_SHA256',identity['source_content_sha256'])
    closure={'candidate-source-identity':{**stable(identity_path),'bytes':str(stable(identity_path)['bytes'])}}
    for n,v in prod.items():
        key='candidate-main-header' if n=='xoffsetdatastructure.hpp' else 'candidate-os-header' if n=='xoffset_virtual_memory.hpp' else 'candidate-'+n.replace('.','-').replace('_','-')
        closure[key]={**v,'bytes':str(v['bytes'])}
    patch_literal(protocol,'CANDIDATE_CLOSURE_IDS',closure)
    changes[str(protocol)]={'before':before,'after':stable(protocol),'allowed':'IDENTITIES.xoffset label, content hash, complete 15-file closure literals only'}
    for n,v in original_metadata.items():
        if n not in ('experiments/serialization_comparison/adapters/xoffset/adapter.hpp','tools/serialization_comparison/protocol.py'):
            need(stable(meta/n)==v,'unexpected metadata edit: '+n)
    write(C/'METADATA_DERIVATION.json',{'original_source':str(old),'before':original_metadata,
      'after':{n:stable(meta/n) for n in original_metadata},'changes':changes,
      'note':'source_archive_sha256 is inherited historical metadata and is not asserted to be a new M2 archive; current identity is the exact154/14 source and request binding.'})
    for mode in ('normal','sanitized'):
        base=B.parent/('m2-full-v1-'+mode);r=read(base/'receipt.json');inv=read(base/'build/test-inventory.json')
        accept=C/('full-'+mode+'-acceptance.json')
        write(accept,{'status':'passed','source':str(source),'production_files':prod,'ctest_count':73,'receipt':refs[mode],
          'runtime_test_programs':inv['runtime_test_programs'],'configure_checks':inv['configure_checks'],'configure_checks_are_not_ctest_cases':True})
        profiles[mode]={'build':str(base/'build'),'logs':str(base),'acceptance':{'path':str(accept),'identity':stable(accept)}}
    write(C/'inputs.json',{'schema_version':1,'kind':'receive-completion-business-inputs','source':str(source),
      'source_identity':{'path':str(identity_path),'identity':stable(identity_path)},'full_profiles':profiles})
    for p in H.rglob('*'):
        if p.is_file():binding[str(p)]=stable(p)
    for p in meta.rglob('*'):
        if p.is_file():binding[str(p)]=stable(p)
    # Existing 11 installed own headers, excluding separately installed OS header.
    own=['xoffsetdatastructure.hpp','xoffset_verified_paths.hpp','xoffset_validation_scratch.hpp','validation_scratch.hpp',
       'xoffset_snapshot_index.hpp','xoffset_snapshot_index_budget.hpp','xoffset_core_lifetime.hpp','receive_raw_fwd.hpp','receive_raw.hpp','receive_future.hpp','xoffset_verified_receive.hpp']
    write(C/'package-input.json',{'schema_version':1,'kind':'c14-m2-package-input-v1','source':str(source),
      'production_files':prod,'own_headers':own,'source_identity':{'path':str(identity_path),'identity':stable(identity_path)},
      'full_receipts':refs,'bindings':binding})
    write(C/'PREPARED.json',{'status':'prepared','production_files':prod,'members':154,'requests':stable(requests),
      'full_receipts':refs,'metadata_changes':changes,'bindings':binding,'performance_samples':0,'native_runs':0,'compiles':0,
      'benchmark_commit_meaning':'7539433ab96d69c4a07b1db5c0a13786da85cdcd is retained legacy benchmark base metadata; it is neither the M2 current Git base 1904b9c71230026ac6f2d58f919a9ab76447d264 nor a claim M2 has been committed. Actual identity is core REQUESTS plus IDENTITY154 plus production14 and source/compiler inputs.',
      'full_binding':'Actual passed full normal/SAN receipts from r2 requests bind the new snapshots; the earlier rejected staged-path preflight is not a compiled-source or platform result.'})
    print('M2 business metadata prepared; no compiler/native/queue started',flush=True)

if __name__=='__main__':main()
