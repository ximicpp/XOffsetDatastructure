def config(root,out,output,control_profile,candidate_profile,preconditions):
    root=path(root);need(root==ROOT,'sole C14 root required');pre=C.read(preconditions)
    need(pre['status']=='passed' and pre['kind']=='v2-screen-preconditions' and pre['candidate']=='%CANDIDATE%','fresh independently reviewed candidate preconditions required')
    cpu=pre['cpu'];need(type(cpu)is int and cpu>=0 and cpu in os.sched_getaffinity(0),'actual registered CPU unavailable')
    checked_file(pre['affinity_observation']);checked_file(pre['cost_policy']);checked_file(pre['functional_summary']);checked_file(pre['full_request'])
    policy=C.read(checked_file(pre['input_policy']));staged=C.read(checked_file(policy['staging_receipt']))
    need(pre['host_policy']['power_guid'] and pre['host_policy']['change_power'] is False,'register actual unchanged power condition')
    profiles={r:C.read(p) for r,p in (('control',control_profile),('candidate',candidate_profile))};need(profiles['control']['environment']==profiles['candidate']['environment'],'two profile environments differ')
    closure={};entries={};actual_sources={}
    for role,profile in profiles.items():
        need(profile['lock_dir']==str(LOCK) and profile['platform_class']=='wsl2-vm-x86_64','original lease/platform required')
        source=path(profile['closure']['candidate-source-tree']);actual_sources[role]=str(source)
        comp=path(profile['closure']['compile-commands']);rows=[r for r in C.read(comp) if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')];need(len(rows)==1,'single original driver compile row')
        adapter=path(profile['closure'].get('candidate-adapter',str(source/'experiments/serialization_comparison/adapters/xoffset/adapter.hpp')))
        entry=profile['binaries']['xoffset'];entries[role]={k:entry[k] for k in ('library','commit','trust_mode','load_contract','source_content_sha256')}
        entries[role].update(binary=dict(path=entry['path'],identity=fid(entry['path'])),prefix=['/usr/bin/taskset','-c',str(cpu)],metadata=dict(path=str(control_profile if role=='control' else candidate_profile),identity=fid(control_profile if role=='control' else candidate_profile)),compile_commands=dict(path=str(comp),identity=fid(comp)),adapter=dict(path=str(adapter),identity=fid(adapter)),effective_argv=rows[0].get('arguments') or shlex.split(rows[0]['command']))
        for key,p in profile['closure'].items():
            if Path(p).is_file():closure[role+'-'+key]=str(path(p))
        closure[role+'-profile']=str(control_profile if role=='control' else candidate_profile)
    need(entries['control']['library']=='xoffset-receive-completion-v1' and entries['candidate']['library']=='xoffset-c14-%CANDIDATE%','actual C14/candidate labels differ')
    cpp=path(entries['candidate']['adapter']['path']).parents[2]/'driver.cpp'
    closure['control-dependencies']=str(path(control_profile).parent/'compiled-source-binding-normal-v2.json')
    closure.update(compiler='/opt/p2996-toolchain/bin/clang++',wrapper=DESIGN['compiler_launcher']['path'],profile_wrapper='/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/bin/clang++',libcxx='/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++.so.1',runtime='/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++abi.so.1',driver=str(cpp),adapter=entries['candidate']['adapter']['path'],dependencies=str(path(candidate_profile).parent/'compiled-source-binding-normal-v2.json'),helper=str(path(DESIGN['source']['candidate'])/'xoffset_core_lifetime.hpp'),request=str(DESIGN_PATH),full_functional=pre['functional_summary']['path'],full_request=pre['full_request']['path'],cost_policy=pre['cost_policy']['path'],preconditions=str(path(preconditions)),taskset='/usr/bin/taskset',host_loader='/lib64/ld-linux-x86-64.so.2',host_libc='/lib/x86_64-linux-gnu/libc.so.6')
    cfg=dict(schema='%LOWER%-screen-config-v2',candidate='%CANDIDATE%',attempt_id='%LABEL%-screen-20261020-v2-v1',output=str(path(output)),backing_mount='/mnt/c',cpu=cpu,environment=profiles['candidate']['environment'],sources={r:DESIGN['source'][r] for r in ('control','candidate')},actual_sources=actual_sources,binaries=entries,closure=closure,materialization=DESIGN['baseline_materialization'],candidate_identity=DESIGN['candidate_identity'],fixture=str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/fixed-M'),input_wire=dict(path=str(ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin'),identity=fid(ROOT/'handoff/continuation-20261019-v1/portable-inputs/old-V8-initial-wire.bin')),preconditions=dict(path=str(path(preconditions)),identity=fid(preconditions)),control_manifest_exception=pre['control_manifest_exception'],host_policy=pre['host_policy'],actual_commit_policy='Legacy adapter commit is inherited benchmark base metadata only; exact source154/14, actual deps and content hash identify this candidate.')
    cfg.update(input_policy=pre['input_policy'],fixture=staged['fixture'],input_wire=dict(path=staged['wire'],identity=fid(staged['wire'])))
    cfg['closure'].update(input_policy=pre['input_policy']['path'],input_staging=policy['staging_receipt']['path'])
    validate_cfg(cfg);write(out,cfg);return dict(status='config_bound_not_screen_admitted',config_identity=fid(out),timing_samples=0)
