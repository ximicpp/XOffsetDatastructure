"""Maintained source tar audit plus exact-byte ZIP/clean full gates; no runner."""
from support import *
import zipfile
from deps_audit import audit_default
from header_probe import header_objects

def source_members(root,metadata):
    expected=dict(metadata['files']);manifest='SOURCE_MANIFEST.json'
    actual={}
    for path in root.rglob('*'):
        state=path.lstat();name=path.relative_to(root).as_posix()
        need(stat.S_ISREG(state.st_mode) or stat.S_ISDIR(state.st_mode),'archive link/nonregular')
        if stat.S_ISREG(state.st_mode):
            need(stat.S_IMODE(state.st_mode)==expected.get(name,{}).get('mode',0o644),'source normalized mode')
            actual[name]=fid(path)
    need(set(actual)==set(expected)|{manifest},'clean source membership')
    for name,value in expected.items():
        need(actual[name]=={'bytes':value['bytes'],'sha256':value['sha256']},'clean source byte identity '+name)
    canonical=(json.dumps(metadata,indent=2,sort_keys=True,ensure_ascii=True)+'\n').encode()
    need(actual[manifest]=={'bytes':len(canonical),'sha256':hashlib.sha256(canonical).hexdigest()},'source manifest exact canonical bytes')
    need(not (root/'.git').exists(),'source package includes Git metadata')
    return actual

def zip_copy(out,source,metadata,root_name):
    actual=source_members(source,metadata);archive=out/(root_name+'.zip')
    with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as stream:
        for name in sorted(actual):
            mode=metadata['files'].get(name,{}).get('mode',0o644)
            item=zipfile.ZipInfo(root_name+'/'+name,(1980,1,1,0,0,0))
            item.create_system=3;item.external_attr=(stat.S_IFREG|mode)<<16
            item.compress_type=zipfile.ZIP_DEFLATED
            stream.writestr(item,(source/name).read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
    # Audit every member before extraction; ZIP is an alternative encoding of
    # the maintained, already-audited tar payload, not a relaxed new manifest.
    extracted=out/'zip-extracted';extracted.mkdir(exist_ok=False)
    with zipfile.ZipFile(archive) as stream:
        items=stream.infolist();names=[i.filename for i in items]
        need(names==[root_name+'/'+n for n in sorted(actual)],'ZIP exact sorted inventory/duplicates')
        need(len(items)<=10001 and sum(i.file_size for i in items)<=128*1024*1024,'ZIP bounded closure')
        for item,name in zip(items,sorted(actual)):
            need(item.file_size==actual[name]['bytes'] and item.file_size<=16*1024*1024,'ZIP member size')
            need(item.flag_bits&~0x800==0 and item.compress_type==zipfile.ZIP_DEFLATED,'ZIP unsupported encoding')
            mode=(item.external_attr>>16)&0xffff
            expected_mode=metadata['files'].get(name,{}).get('mode',0o644)
            need(stat.S_ISREG(mode) and stat.S_IMODE(mode)==expected_mode,'ZIP member type/mode')
            need(not Path(name).is_absolute() and '..' not in Path(name).parts and '\\' not in name,'ZIP path')
            data=stream.read(item);need(len(data)==actual[name]['bytes'] and hashlib.sha256(data).hexdigest()==actual[name]['sha256'],'ZIP byte hash')
            target=extracted/root_name/name;target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as output:output.write(data)
            target.chmod(expected_mode)
    root=extracted/root_name
    need(source_members(root,metadata)==actual,'ZIP extraction closure differs')
    write(out/'zip-audit.json',{'status':'passed','archive':fid(archive),'file_count':len(actual),
      'source_manifest':fid(root/'SOURCE_MANIFEST.json'),'same_tar_payload_bytes':True,'nonregular_rejected':True})
    return root

def full(out,source,spec,kind,own,forbidden):
    build=out/('clean-full-'+kind+'-build');sanitized=kind=='sanitized';cmake=P1/'tools/cmake/bin/cmake'
    e=env(sanitized)
    command(out,kind+'-configure',[cmake,'-S',source,'-B',build,*flags(sanitized),
      '-DXOFFSET_BUILD_TESTS=ON','-DXOFFSET_BUILD_TOOLS=OFF','-DXOFFSET_BUILD_EXAMPLES=OFF','-DXOFFSET_INSTALL=OFF'],180,e)
    inventory=read(build/'test-inventory.json');cats=('runtime_test_programs','subprocess_scenarios','tooling_checks')
    expected=[n for c in cats for n in inventory[c]]
    need(inventory['mode']=='full' and sorted(expected)==sorted(spec['expected_full_tests']) and
      len(expected)==len(set(expected))==inventory['ctest_count'],'clean full selected inventory')
    need(set(spec['required_tests']).issubset(expected),'required new/old tests missing')
    need(inventory['tooling_check_count']==0 and inventory['tooling_checks']==[] and
      inventory['configure_check_count']==len(inventory['configure_checks'])==len(set(inventory['configure_checks']))==3,'three separate configure checks')
    for count,names in (('runtime_test_program_count','runtime_test_programs'),('subprocess_scenario_count','subprocess_scenarios'),('tooling_check_count','tooling_checks')):
        need(inventory[count]==len(inventory[names]),'inventory category count')
    command(out,kind+'-build',[cmake,'--build',build,'--parallel','2'],1200,e)
    xml=out/(kind+'.xml');command(out,kind+'-ctest',[P1/'tools/cmake/bin/ctest','--test-dir',build,
      '--output-on-failure','--timeout','120','--output-junit',xml],1200,e)
    result=junit(xml,expected);result['inventory']=fid(build/'test-inventory.json')
    command(out,kind+'-deps',['/usr/bin/ninja','-C',build,'-t','deps'],60,e)
    dep_path=out/(kind+'-deps.stdout')
    result['actual_dependencies']=fid(dep_path)
    result['dependency_scope']=audit_default(dep_path.read_text(),source,own,forbidden)
    return result

def main():
    spec,source,out,own=get_spec('archive');tool=source/'tools/prepare_alpha_release.py'
    artifacts=out/'source-artifacts'
    command(out,'source-create',['/usr/bin/python3','-B',tool,'create','--source-dir',source,'--out',artifacts],180)
    result=json.loads((out/'source-create.stdout').read_text());archive=Path(result['archive'])
    sidecar=archive.with_name(archive.name.removesuffix('.tar.gz')+'.manifest.json');checksums=artifacts/'SHA256SUMS'
    command(out,'source-audit',['/usr/bin/python3','-B',tool,'audit','--archive',archive,'--manifest',sidecar,'--checksums',checksums],180)
    command(out,'source-extract',['/usr/bin/python3','-B',tool,'extract','--archive',archive,'--manifest',sidecar,'--checksums',checksums,'--out',out/'tar-extracted'],180)
    record=json.loads((out/'source-extract.stdout').read_text());tar_source=Path(record['source_dir'])
    metadata=read(sidecar);clean=zip_copy(out,tar_source,metadata,record['archive_root'])
    check_production(clean,spec['production_files'])
    results={k:full(out,clean,spec,k,own,(source,tar_source)) for k in ('normal','sanitized')}
    probes={k:header_objects(out,clean,own,k,(source,tar_source)) for k in ('normal','sanitized')}
    # New C consumers execute from the clean decoded source as well; source
    # include dependencies cannot silently use the original final source.
    consumer=out/'receive-consumer';copy_consumer(consumer)
    results['clean_receive']=build_c_consumer(out,'clean-receive',consumer,source=clean,own=own,forbidden=(source,tar_source))
    source_members(clean,metadata);check_production(source,spec['production_files'])
    write(out/'result.json',{'schema_version':1,'status':'passed','source_identity':spec['source_identity'],
      'tar':fid(archive),'manifest':fid(sidecar),'checksums':fid(checksums),'zip_audit':fid(out/'zip-audit.json'),
      'full':results,'public_first_include_objects':probes,'public_header_probe_native_executions':0,
      'dependency_lock_unchanged':True,'clean_source_without_git':True,
      'three_configure_checks_separate':True,'native_performance_samples':0,'published':False})

if __name__=='__main__':main()
