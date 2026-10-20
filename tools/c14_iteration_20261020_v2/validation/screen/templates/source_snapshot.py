def source_snapshot(cfg):
    material=C.read(checked_file(cfg['materialization']))['production_files'];identity=C.read(checked_file(cfg['candidate_identity']))
    need(len(material)==14 and identity['candidate']=='%CANDIDATE%' and identity['member_count']==len(identity['members'])==154,'independent exact154/14 candidate required')
    result={}
    for role in ('control','candidate'):
        root=path(cfg['sources'][role]);actual_root=path(cfg['actual_sources'][role]);members={};actual_members={}
        for n,record in identity['members'].items():
            C.relative(n);v=fid(root/n);v['bytes']=int(v['bytes']);expected=record['candidate_sha256' if role=='candidate' else 'baseline_sha256']
            need(v['sha256']==expected,'materialized154 differs: '+role+'/'+n);members[n]=v;actual_members[n]=fid(actual_root/n);actual_members[n]['bytes']=int(actual_members[n]['bytes'])
        production={n:members[n] for n in material};need({n:actual_members[n] for n in material}==production,'actual compiled production14 differs')
        differences=[n for n in members if members[n]!=actual_members[n]]
        if differences:
            # The accepted historical C14 binary has identical153 code/input
            # members but its packaging SOURCE_MANIFEST has historical bytes.
            exception=cfg['control_manifest_exception']
            need(role=='control' and differences==['SOURCE_MANIFEST.json'] and exception is not None,'unregistered compiled154 source mismatch')
            need(exception['status']=='passed' and exception['kind']=='control-source-manifest-metadata-equivalence' and exception['baseline153_and_production14_identical'] is True,'explicit root manifest-equivalence review required')
            need(str(checked_file(exception['actual_manifest']))==str(actual_root/'SOURCE_MANIFEST.json'),'actual historical manifest differs')
            checked_file(exception['provenance'])
        if role=='control':need(all(production[n]['sha256']==material[n]['sha256'] for n in material),'C14 control14 differs')
        digest=hashlib.sha256(__import__('json').dumps(production,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        need(digest==cfg['binaries'][role]['source_content_sha256'],'actual adapter14 content hash differs')
        result[role]=dict(materialized_root=str(root),actual_compiled_source=str(actual_root),members=members,actual_members154=actual_members,production=production,actual_source_manifest=fid(actual_root/'SOURCE_MANIFEST.json'),metadata_only_differences=differences)
    return result
