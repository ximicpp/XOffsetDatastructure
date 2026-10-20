def projection(cfg,capture_dir):
    if path(capture_dir).is_dir():return project_capture(cfg,path(capture_dir))
    # Registration references already completed own functional M runs. It never
    # executes a capture, and cannot consume the closed old M1 timing prefix.
    registration=path(capture_dir);s=C.read(registration)
    need(s['kind']=='v2-screen-existing-functional-projection' and s['status']=='passed' and s['candidate']=='%CANDIDATE%','existing full bare functional projection review required')
    need(s['candidate_identity']==cfg['candidate_identity'] and s['screen_config_identity']==fid(checked_file(s['screen_config'])),'projection identity/config binding differs')
    need(C.read(checked_file(s['screen_config']))==cfg,'projection config differs')
    need(s['not_timing_inference'] is True and s['no_old_screen_samples'] is True and s['diagnostic_observer_runs_used'] is False,'projection must exclude timing prefixes/diagnostic runs')
    need(set(s['roles'])=={'control','candidate'},'two existing bare-run roles required')
    observed=[]
    for role,item in s['roles'].items():
        prefix=path(item['output_prefix']);receipt_prefix=path(item['receipt_prefix']);receipt=C.read(checked_file(item['receipt']));fixture=path(item['fixture']);wire=checked_file(item['input_wire'])
        need('screen' not in str(prefix).lower() and 'diag' not in str(prefix).lower(),'screen/diagnostic prefix cannot be a functional projection')
        need(item['profile']==cfg['binaries'][role]['metadata'] and item['binary']==cfg['binaries'][role]['binary'],'actual role profile/binary differs')
        G.fixture_check(fixture,16384)
        need(all(fid(fixture/n)==fid(path(cfg['fixture'])/n) for n in (*G.FILES,'fixture.json')) and fid(wire)==cfg['input_wire']['identity'],'existing M fixture/input bytes differ')
        need(os.stat(wire).st_dev==os.stat(cfg['input_wire']['path']).st_dev and os.stat(fixture).st_dev==os.stat(cfg['fixture']).st_dev,'root functional process cannot project a different mounted storage domain')
        checked_file(item['same_storage_domain_review'])
        expected=G.command(dict(path=cfg['binaries'][role]['binary']['path'],prefix=item['actual_prefix']),'run','--wire',str(wire),'--trace',fixture/'trace.bin','--wire-out',prefix.with_suffix('.wire'),'--canonical-out',prefix.with_suffix('.canonical'),'--queries-out',prefix.with_suffix('.queries'),'--result',prefix.with_suffix('.native.json'))
        need(receipt['argv']==expected and receipt['execution_environment']==cfg['environment'] and receipt['returncode']==0 and receipt['timed_out'] is False and receipt['launch_error'] is None,'existing complete process receipt/argv differs')
        need(item['actual_prefix'] in ([],cfg['binaries'][role]['prefix']) and receipt_prefix.with_suffix('.stderr').stat().st_size==0,'unexpected functional wrapper/stderr')
        need(checked_file(item['receipt'])==receipt_prefix.with_suffix('.receipt.json'),'raw receipt path differs')
        need(set(item['body_files'])=={'wire','canonical','queries','native.json','stdout','stderr'},'all actual output/receipt bodies required')
        for ext,reference in item['body_files'].items():
            actual=receipt_prefix.with_suffix('.'+ext) if ext in ('stdout','stderr') else prefix.with_suffix('.'+ext)
            need(checked_file(reference)==actual,'actual functional output body reference differs')
        q=checked_file(item['queue_request']);r=checked_file(item['queue_result']);qr=C.read(q);rr=C.read(r)
        need(qr['runner_sha256']==C.sha(RUNNER) and qr['lock_dir']==str(LOCK) and qr['kind']=='functional' and rr['run_id']==qr['run_id'] and rr['status']=='complete' and rr['command_returncode']==0 and rr['cleanup']=={'live':[],'process_groups':[],'zombies':[]},'original successful functional lease/cleanup required')
        checked_file(item['full_audit']);checked_file(item['runtime_before']);checked_file(item['runtime_after']);checked_file(item['functional_manifest'])
        need(C.read(item['runtime_before']['path'])==C.read(item['runtime_after']['path']),'functional before/after runtime changed')
        row=G.native_result(prefix.with_suffix('.native.json'),cfg['binaries'][role],fixture,wire,prefix.with_suffix('.wire'),prefix.with_suffix('.canonical'),prefix.with_suffix('.queries'),receipt)
        need(C.decimal(row['peak_rss_after_audit_kib'])*1024<=16*GiB,'functional full-run RSS exceeds16GiB')
        seconds=(int(receipt['finished_monotonic_ns'])-int(receipt['started_monotonic_ns']))/1e9
        need(0<seconds<=20,'actual full process cannot fit child20s')
        margin=item['process_plus_online_audit_bound_seconds'];need(type(margin) in (int,float) and math.isfinite(margin) and margin>=seconds+item['registered_online_audit_margin_seconds'] and item['registered_online_audit_margin_seconds']>0,'explicit positive full-body audit/dedup margin required')
        checked_file(item['margin_review'])
        observed.append(dict(role=role,full_process_seconds=seconds,online_child_audit_seconds=margin,wire_bytes=prefix.with_suffix('.wire').stat().st_size,canonical_bytes=prefix.with_suffix('.canonical').stat().st_size,queries_bytes=prefix.with_suffix('.queries').stat().st_size))
    wire=max(r['wire_bytes'] for r in observed);canonical=max(r['canonical_bytes'] for r in observed);queries=max(r['queries_bytes'] for r in observed);limit=max(wire,canonical,queries,1024**2)
    metadata=64*1024**2;retained=allocated(registration)['unique_allocated_bytes']
    staging=input_policy(cfg)
    peak=144*((wire+4095)//4096*4096)+canonical+queries+7*limit+metadata+retained+staging['actual_allocated_bytes']
    online=144*max(r['online_child_audit_seconds'] for r in observed)+POLICY['nonchild_projection_seconds']+staging['elapsed_seconds']
    need(peak<=4*GiB and online<=600-POLICY['seal_seconds'],'complete-process/full-body-audit or physical projection cannot fit registered cap')
    return dict(wire_max_bytes=wire,wire_bytes_by_role={r['role']:r['wire_bytes'] for r in observed},canonical_bytes=canonical,queries_bytes=queries,file_limit_bytes=limit,all_wire_unique=True,metadata_reserve_bytes=metadata,retained_capture_allocated_bytes=retained,temporary_slots=7,physical_peak_bytes=peak,logical_output_bytes=144*(wire+canonical+queries),projected_online_seconds=online,observed_complete_functional=observed,staging_engineering_cost=staging,capture_manifest=fid(registration),actual_free=resources(cfg,peak))
