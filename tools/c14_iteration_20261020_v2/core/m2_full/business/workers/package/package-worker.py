"""Root-leased old package 3+3/alpha audit plus new standalone receive consumer."""
from support import *

def main():
    spec,source,out,own=get_spec('package');results={}
    e=env();e['PATH']=str(P1/'tools/cmake/bin')+':'+e.get('PATH','/usr/bin:/bin');e['CXXFLAGS']='-stdlib=libc++ -pthread'
    for suite in ('storage','full'):
        directory=out/(suite+'-package')
        command(out,suite+'-package',['/usr/bin/python3','-B',source/'tools/validate_vm_package.py',
          '--source-dir',source,'--suite',suite,'--out',directory,'--generator','Ninja',
          '--compiler',P1/'tools/p2996-profile/bin/clang++-libcxx','--jobs','2'],900,e)
        result=read(directory/'result.json');need(result['status']=='passed','original package validator')
        need(result['installed_consumer_tests']==3 and result['subproject_consumer_tests']==3,'original consumers retained')
        numeric=result['version']['numeric'];need(result['version_request_rejections']==[numeric,'999.0.0',numeric+';EXACT',numeric+'...<999.0.0'],'numeric alpha rejections unchanged')
        if suite=='full':need(result['package_audit']['dependency_header_count']==851,'locked dependency closure')
        results[suite]={'result':fid(directory/'result.json'),'installed':3,'subproject':3,'numeric_requests_rejected':4}
        archive_out=out/(suite+'-install-artifacts');tool=source/'tools/prepare_alpha_release.py'
        command(out,suite+'-prefix-pack',['/usr/bin/python3','-B',tool,'pack-prefix','--prefix',directory/'relocated-prefix','--component',suite,'--out',archive_out],180)
        packed=json.loads((out/(suite+'-prefix-pack.stdout')).read_text());archive=Path(packed['archive'])
        sidecar=archive.with_name(archive.name.removesuffix('.tar.gz')+'.manifest.json');sums=archive_out/'SHA256SUMS'
        command(out,suite+'-prefix-audit',['/usr/bin/python3','-B',tool,'audit','--archive',archive,'--manifest',sidecar,'--checksums',sums],180)
        command(out,suite+'-prefix-extract',['/usr/bin/python3','-B',tool,'extract','--archive',archive,'--manifest',sidecar,'--checksums',sums,'--out',out/(suite+'-install-extracted')],180)
        decoded=json.loads((out/(suite+'-prefix-extract.stdout')).read_text());unpacked=Path(decoded['source_dir'])
        unpacked.rename(out/(suite+'-install-moved-prefix'))
        results[suite]['install_archive']={'archive':fid(archive),'manifest':fid(sidecar),'checksums':fid(sums)}
    full=out/'full-package';snapshot=full/'producer-source';hidden=full/'unavailable-producer-source';prefix=full/'relocated-prefix'
    consumer=out/'receive-consumer';copy_consumer(consumer)
    snapshot.rename(hidden)
    try:
        need(not snapshot.exists() and not (full/'original-prefix').exists(),'original paths not hidden')
        results['receive_installed']=build_c_consumer(out,'receive-installed',consumer,prefix=prefix,own=own,
            forbidden=(source,snapshot,hidden,full/'original-prefix'))
    finally:hidden.rename(snapshot)
    results['receive_subproject']=build_c_consumer(out,'receive-subproject',consumer,source=snapshot,own=own,
        forbidden=(source,hidden,full/'original-prefix'))
    hidden_prefix=out/'unavailable-old-relocated-prefix'
    snapshot.rename(hidden);prefix.rename(hidden_prefix)
    try:
        results['receive_archive_installed']=build_c_consumer(out,'receive-archive-installed',consumer,
          prefix=out/'full-install-moved-prefix',own=own,
          forbidden=(source,snapshot,hidden,prefix,hidden_prefix,full/'original-prefix'))
    finally:
        hidden_prefix.rename(prefix);hidden.rename(snapshot)
    for n in own:need(fid(prefix/'include'/n)==fid(source/n),'installed own header bytes')
    check_production(source,spec['production_files'])
    write(out/'result.json',{'schema_version':1,'status':'passed','source_identity':spec['source_identity'],
      'results':results,'own_headers':list(own),'dependency_header_count':851,'native_performance_samples':0,
      'publication':False,'installed_old_source_paths_hidden':True})

if __name__=='__main__':main()
