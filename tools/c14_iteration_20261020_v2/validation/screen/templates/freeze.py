def freeze(config,capture_dir,gates,out):
    cfg=C.read(config);before=validate_cfg(cfg);gate=C.read(gates)
    need(gate['kind']=='v2-screen-complete-gates-admission' and gate['status']=='passed' and gate['candidate_identity']==cfg['candidate_identity'],'same actual candidate complete gate admission required')
    need(set(gate['gates'])==set(GATES),'all eleven candidate gate dependencies required')
    records={}
    expected={'codegen':{'candidate_compiles':1},'mechanism':{'actual_nonempty_path_observed':True,'static_actual_codegen_passed':True,'unhooked_screen_binary':True},'targeted':{'normal':int('%TARGET_N%'),'sanitized':int('%TARGET_N%')},'full_normal':{'ctest_passed':73},'full_san':{'ctest_passed':73},'business_SML':{'normal_children':80,'sanitized_children':80},'v1_bidirectional':{'main_cases':84,'extra_cases':8,'AB_actions':4,'C_actions':4},'consumers':{'install_subproject_passed':True,'shared_runtime_passed':True},'lifecycle':{'actual_native_children':52,'normal_model_cases':25,'sanitized_model_cases':25,'pool_cases':2},'safety':{'representation_and_lifetime_preserved':True},'engineering':{'predeclared_cost_policy_satisfied':True}}
    for name,reference in gate['gates'].items():
        p=checked_file(reference);e=C.read(p)
        need(e['kind']=='v2-screen-gate-receipt' and e['gate']==name and e['status']=='passed' and e['candidate_identity']==cfg['candidate_identity'],'typed actual candidate gate receipt required: '+name)
        need(e['source154']==before['sources']['candidate']['members'] and e['production14']==before['sources']['candidate']['production'],'gate source154/14 differs: '+name)
        need(all(e['assertions'].get(k)==v for k,v in expected[name].items()),'actual gate counts/assertions differ: '+name)
        need(e['evidence'] and all(checked_file(r).is_file() for r in e['evidence']),'real frozen evidence refs required: '+name)
        need(e['independent_review']['status']=='passed' and checked_file(e['independent_review']['record']).is_file(),'independent raw evidence review required: '+name)
        records[name]=dict(path=str(p),identity=fid(p))
    need(not path(cfg['output']).exists(),'attempt output already exists; replacement/retry forbidden')
    projection_record=projection(cfg,path(capture_dir));need(before==bindings(cfg),'identity changed while assembling freeze')
    out=C.fresh(out)
    record=dict(schema='%LOWER%-screen-freeze-v2',frozen_path=str(out/'freeze.json'),config=cfg,config_identity=fid(config),design=dict(path=str(DESIGN_PATH),identity=fid(DESIGN_PATH)),policy=POLICY,schedule=schedule(),randomization=DESIGN['randomization'],runtime=before,gates=records,gate_admission=dict(path=str(path(gates)),identity=fid(gates)),capture=str(path(capture_dir)),projection=projection_record,performance_accepted=False,independent_review_required=True)
    write(out/'freeze.json',record)
    return dict(status='frozen_pending_independent_review',freeze_sha256=C.sha(out/'freeze.json'),output=cfg['output'])
