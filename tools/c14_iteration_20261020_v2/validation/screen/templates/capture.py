def capture(config,out,queue,request,request_sha256,review):
    cfg=C.read(config);before=validate_cfg(cfg);out=path(out)
    need(C.hexsha(request_sha256) and C.sha(request)==request_sha256,'exact finite resource-capture request required')
    req=C.read(request);admit=C.read(review)
    need(req['kind']=='v2-screen-finite-resource-capture' and req['candidate']=='%CANDIDATE%' and req['config_identity']==fid(config) and req['runtime']==before,'finite capture source/input/tool/runtime closure differs')
    need(req['output']==str(out) and req['queue']==str(queue) and req['roles']==['control','candidate'] and req['children']==2 and req['child_seconds']==20 and req['outer_seconds']==120 and req['compile']==0 and req['retry']==0 and req['timing_inference_samples']==0,'exact one-control/one-candidate finite capture budget required')
    need(admit['kind']=='independent-resource-capture-admission' and admit['status']=='passed' and admit['candidate']=='%CANDIDATE%' and admit['request_sha256']==request_sha256,'independent exact capture admission required')
    need(out==Path('/root/xoffset-c14-iteration/20261020/v2/validation/%LOWER%-screen-resource-capture-v1') and queue==out/'queue' and set(p.name for p in out.iterdir())=={'queue'},'one new resource-capture prefix required')
    lease=active(queue,'functional',120);write(out/'lease.json',lease);C.copy_file(config,out/'config.json');C.copy_file(request,out/'capture-request.json');C.copy_file(review,out/'capture-review.json');write(out/'runtime-before.json',before)
    resources(cfg,14*64*1024**2);(out/'commands').mkdir();completed=[];error=None;started=time.monotonic()
    try:
        for role in ('control','candidate'):
            need(time.monotonic()-started<=60,'capture must leave complete second child and sealing room')
            prefix=out/'commands'/role;t=time.monotonic();receipt=G.execute(argv(cfg,role,prefix),prefix,20,cfg['environment'],64*1024**2)
            _,values=native(cfg,role,prefix,receipt)
            need(receipt['returncode']==0 and receipt['timed_out'] is False and receipt['launch_error'] is None and prefix.with_suffix('.stderr').stat().st_size==0,'full bare resource child failed')
            need(byte_equal(prefix.with_suffix('.canonical'),path(cfg['fixture'])/'final.canonical.bin') and byte_equal(prefix.with_suffix('.queries'),path(cfg['fixture'])/'queries.bin'),'capture full byte truth mismatch')
            completed.append(dict(role=role,receipt=fid(prefix.with_suffix('.receipt.json')),wire_bytes=prefix.with_suffix('.wire').stat().st_size,canonical_bytes=prefix.with_suffix('.canonical').stat().st_size,queries_bytes=prefix.with_suffix('.queries').stat().st_size,full_process_seconds=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9,online_child_audit_seconds=time.monotonic()-t,values=values))
    except (C.ProtocolError,OSError,ValueError) as exception:error=type(exception).__name__+': '+str(exception)
    try:after=bindings(cfg)
    except (C.ProtocolError,OSError,ValueError) as exception:after=dict(identity_error=str(exception))
    write(out/'runtime-after.json',after)
    if after!=before:error='resource capture before/after closure differs'
    if time.monotonic()-started>120:error='capture outer120s budget exceeded'
    C.seal(out,dict(schema='%LOWER%-functional-resource-capture-v2',status='passed' if error is None else 'failed',error=error,completed=completed,elapsed_seconds=time.monotonic()-started,request_sha256=request_sha256,timing_inference_samples=0,warmup=False,retries=0))
    need(error is None,'finite resource capture closed: '+str(error));return dict(status='resource_capture_complete_not_timing_inference',roles=2,new_compiles=0,retry=0)

