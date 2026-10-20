def project_capture(cfg,directory):
    C.check_inventory(directory);s=C.read(directory/'status.json')
    need(s['schema']=='%LOWER%-functional-resource-capture-v2' and s['status']=='passed' and [r['role'] for r in s['completed']]==['control','candidate'],'one exact complete two-role resource capture required')
    need(C.read(directory/'config.json')==cfg and C.read(directory/'runtime-before.json')==C.read(directory/'runtime-after.json')==bindings(cfg),'capture exact full closure differs')
    request=C.read(directory/'capture-request.json');admission=C.read(directory/'capture-review.json')
    need(s['request_sha256']==C.sha(directory/'capture-request.json')==admission['request_sha256'] and request['candidate']=='%CANDIDATE%' and admission['status']=='passed' and admission['kind']=='independent-resource-capture-admission','actual finite capture admission differs')
    q,r=queue_audit(directory/'queue','functional',120,C.read(directory/'lease.json')['request']['command'])
    need(r['status']=='complete' and r['command_returncode']==0 and r['command_elapsed_seconds']<=120,'finite capture outer failed')
    pre=C.read(checked_file(cfg['preconditions']));tail=pre['projection_tail_margin_seconds'];need(type(tail) in (int,float) and math.isfinite(tail) and tail>0,'predeclared positive dedup/ledger/full audit tail margin required')
    for item in s['completed']:
        role=item['role'];prefix=directory/'commands'/role;receipt=C.read(prefix.with_suffix('.receipt.json'))
        need(receipt['argv']==argv(cfg,role,prefix) and receipt['execution_environment']==cfg['environment'] and receipt['returncode']==0 and receipt['timed_out'] is False and receipt['timeout_seconds']==20 and fid(prefix.with_suffix('.receipt.json'))==item['receipt'] and prefix.with_suffix('.stderr').stat().st_size==0,'finite capture raw process receipt differs')
        _,values=native(cfg,role,prefix,receipt,item['wire_bytes']);elapsed=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9
        need(values==item['values'] and elapsed==item['full_process_seconds'] and 0<elapsed<=20 and item['online_child_audit_seconds']>=elapsed,'complete process and full-body audit time differs')
        need(item['canonical_bytes']==prefix.with_suffix('.canonical').stat().st_size and item['queries_bytes']==prefix.with_suffix('.queries').stat().st_size,'capture body sizes differ')
    wire=max(x['wire_bytes'] for x in s['completed']);canonical=max(x['canonical_bytes'] for x in s['completed']);queries=max(x['queries_bytes'] for x in s['completed']);limit=max(wire,canonical,queries,1024**2);retained=allocated(directory)['unique_allocated_bytes'];staging=input_policy(cfg)
    peak=144*((wire+4095)//4096*4096)+canonical+queries+7*limit+64*1024**2+retained+staging['actual_allocated_bytes']
    online=144*(max(x['online_child_audit_seconds'] for x in s['completed'])+tail)+POLICY['nonchild_projection_seconds']+staging['elapsed_seconds']
    need(peak<=4*GiB and online<=600-POLICY['seal_seconds'],'actual complete process/audit+tail/staging or physical projection cannot fit; do not launch screen')
    return dict(wire_max_bytes=wire,wire_bytes_by_role={x['role']:x['wire_bytes'] for x in s['completed']},canonical_bytes=canonical,queries_bytes=queries,file_limit_bytes=limit,all_wire_unique=True,metadata_reserve_bytes=64*1024**2,retained_capture_allocated_bytes=retained,temporary_slots=7,physical_peak_bytes=peak,logical_output_bytes=144*(wire+canonical+queries),projected_online_seconds=online,process_and_audit_by_role=s['completed'],predeclared_tail_margin_seconds=tail,staging_engineering_cost=staging,capture_manifest=fid(directory/'manifest.json'),actual_free=resources(cfg,peak))
