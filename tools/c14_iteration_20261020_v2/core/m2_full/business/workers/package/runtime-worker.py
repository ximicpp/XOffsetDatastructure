"""Same six shared-runtime consumers, plus actual C consumer; root lease only."""
from support import *

EXPECTED=('scratch_typed','index_array','index_budget','scratch_contracts','scratch_two_tu','scratch_modules')

def main():
    spec,source,out,own=get_spec('runtime')
    snapshot=out/'producer-source';shutil.copytree(source,snapshot,symlinks=True)
    consumer=out/'consumer';shutil.copytree(Path(__file__).parent/'runtime-fixtures',consumer)
    write(out/'fixture-inputs.json',{p.name:fid(p) for p in sorted(consumer.iterdir())})
    producer=out/'producer-build';prefix=out/'original-prefix';relocated=out/'relocated-prefix'
    cmake=P1/'tools/cmake/bin/cmake'
    command(out,'producer-configure',[cmake,'-S',snapshot,'-B',producer,*flags(),
      '-DXOFFSET_BUILD_TESTS=OFF','-DXOFFSET_BUILD_TOOLS=OFF','-DXOFFSET_BUILD_EXAMPLES=OFF','-DXOFFSET_INSTALL=ON',
      '-DXOFFSET_BUILD_VALIDATION_SCRATCH_RUNTIME=ON','-DXOFFSET_BUILD_SNAPSHOT_INDEX_RUNTIME=ON',
      '-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_INSTALL_LIBDIR=lib','-DCMAKE_INSTALL_INCLUDEDIR=include'],180)
    command(out,'producer-build',[cmake,'--build',producer,'--parallel','2'])
    command(out,'producer-install',[cmake,'--install',producer],180)
    shared={p.relative_to(prefix).as_posix():fid(p) for p in prefix.rglob('*') if p.is_file() and p.suffix=='.so'}
    need(set(shared)=={'lib/libxoffset_validation_scratch.so','lib/libxoffset_snapshot_index_runtime.so'},'exact optional runtime binaries')
    for n in own:need(fid(prefix/'include'/n)==fid(source/n),'runtime prefix own header differs')
    for p in (prefix/'lib/cmake/XOffsetDatastructure').glob('*.cmake'):
        text=p.read_text()
        for forbidden in (source,snapshot,producer,prefix):need(str(forbidden) not in text,'runtime export original path')
    prefix.rename(relocated);hidden=out/'unavailable-producer-source';snapshot.rename(hidden)
    results={};c_consumer=out/'receive-consumer';copy_consumer(c_consumer)
    try:
        for mode in ('installed','subproject'):
            if mode=='subproject':hidden.rename(snapshot)
            build=out/(mode+'-build');args=[cmake,'-S',consumer,'-B',build,*flags()]
            args+=['-DXC_SOURCE='+str(snapshot)] if mode=='subproject' else [
              '-DCMAKE_PREFIX_PATH='+str(relocated),'-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF','-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF']
            command(out,mode+'-configure',args,180);command(out,mode+'-build',[cmake,'--build',build,'--parallel','2'])
            results[mode]=ctest(out,mode,build,EXPECTED)
            include=snapshot if mode=='subproject' else relocated/'include'
            results[mode]['actual_dependencies']=actual_deps(out,mode,build,include,
                (source,hidden,prefix) if mode=='subproject' else (source,snapshot,hidden,prefix),
                [n for n in own if n not in ('xoffset_core_lifetime.hpp','receive_raw_fwd.hpp','receive_raw.hpp','receive_future.hpp','xoffset_verified_receive.hpp')])
            if mode=='installed':
                need(not snapshot.exists() and not prefix.exists(),'runtime original source/prefix visible')
                results['receive_installed']=build_c_consumer(out,'receive-installed',c_consumer,prefix=relocated,own=own,
                    forbidden=(source,snapshot,hidden,prefix))
            else:
                results['receive_subproject']=build_c_consumer(out,'receive-subproject',c_consumer,source=snapshot,own=own,
                    forbidden=(source,hidden,prefix))
    finally:
        if hidden.exists() and not snapshot.exists():hidden.rename(snapshot)
    runtime={}
    for n,v in shared.items():
        path=relocated/n;need(fid(path)==v,'runtime binary changed')
        tag=Path(n).stem
        command(out,tag+'-dynamic',['/usr/bin/readelf','-d',path],60)
        command(out,tag+'-linkage',['/usr/bin/ldd',path],60)
        need('not found' not in (out/(tag+'-linkage.stdout')).read_text(),'runtime dependency unresolved')
        runtime[n]={'binary':v,'dynamic':fid(out/(tag+'-dynamic.stdout')),'linkage':fid(out/(tag+'-linkage.stdout'))}
    check_production(source,spec['production_files'])
    write(out/'result.json',{'schema_version':1,'status':'passed','consumers':results,'runtimes':runtime,
      'source_identity':spec['source_identity'],'old_six_consumers_unchanged':True,'installed_original_paths_hidden':True,
      'compiler_specific_shared_runtime':True,'header_only_archive_schema_unchanged':True,'performance_samples':0})

if __name__=='__main__':main()
