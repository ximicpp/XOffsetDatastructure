def input_policy(cfg):
    policy=C.read(checked_file(cfg['input_policy']))
    need(policy['kind']=='v2-screen-staged-input-policy' and policy['status']=='passed' and policy['candidate']=='%CANDIDATE%' and policy['all_roles_same_paths'] is True and policy['API_model_trace_unchanged'] is True,'independent candidate staged-input policy approval required')
    staged=C.read(checked_file(policy['staging_receipt']));checked_file(policy['independent_review'])
    need(staged['status']=='staged_pending_screen_policy_admission' and staged['new_compiles']==staged['new_native']==staged['new_timing_samples']==0 and staged['file_count']==len(staged['files'])==9,'actual source-only staging receipt required')
    need(cfg['fixture']==staged['fixture'] and cfg['input_wire']['path']==staged['wire'],'selected common staged paths differ')
    base=Path('/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1');original=ROOT/'handoff/continuation-20261019-v1/portable-inputs'
    expected={str(original/'fixed-M'/n):str(base/'fixed-M'/n) for n in ('final.canonical.bin','fixture.json','frozen-reference.py','input.canonical.bin','queries.bin','query-after.bin','query-before.bin','trace.bin')};expected[str(original/'old-V8-initial-wire.bin')]=str(base/'initial-wire.bin')
    need({r['source']:r['target'] for r in staged['files']}==expected,'exact original9 to native WSL staged9 mapping required')
    for record in staged['files']:
        for name in ('source','target'):checked_file(dict(path=record[name],identity=record['identity']))
    need(sum(r['identity']['bytes'] for r in staged['files'])==staged['bytes'] and staged['actual_allocated_bytes']>=staged['bytes'] and 0<staged['elapsed_seconds']<=60,'actual staging cost/bytes required')
    checked_file(staged['tool']);checked_file(staged['freeze']);cresult=C.read(checked_file(policy['C_result']))
    need(cresult['status']=='passed' and cresult['actual_diagnostic_native']==2 and policy['C_result']['identity']['sha256']=='c13e761ba41a67a7abcac600fbcea2a1edfe517032c782ccc98f1d41cbeab2c4','exact actual C diagnostic basis required')
    for role in ('c14','r1'):checked_file(cresult['roles'][role]['observer'])
    need(policy['staging_engineering_cost_accounted'] is True and policy['diagnostic_times_not_AB_speed'] is True,'charge staging engineering work; diagnostics cannot establish performance')
    return dict(policy=dict(path=str(checked_file(cfg['input_policy'])),identity=fid(cfg['input_policy']['path'])),staging=policy['staging_receipt'],bytes=staged['bytes'],elapsed_seconds=staged['elapsed_seconds'],actual_allocated_bytes=allocated(base)['unique_allocated_bytes'],files=staged['files'])
