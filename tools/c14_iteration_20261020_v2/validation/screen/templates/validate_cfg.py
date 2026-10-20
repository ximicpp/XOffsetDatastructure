def validate_cfg(cfg):
    need(cfg['schema']=='%LOWER%-screen-config-v2' and cfg['candidate']=='%CANDIDATE%' and cfg['attempt_id']=='%LABEL%-screen-20261020-v2-v1','exact independent candidate/attempt only')
    need(path(cfg['output'])==Path('/root/xoffset-c14-iteration/20261020/v2/validation/%LABEL%-screen-20261020-v2-v1'),'single new candidate output domain')
    need(cfg['environment'] and all(type(k)is str and type(v)is str for k,v in cfg['environment'].items()) and cfg['backing_mount']=='/mnt/c','explicit unchanged environment/C backing')
    input_policy(cfg)
    checked_file(cfg['preconditions']);return bindings(cfg)
