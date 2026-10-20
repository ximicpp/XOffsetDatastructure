def bindings(cfg):
    need(set(cfg['binaries'])=={'control','candidate'},'exact two binary roles')
    binaries={r:fid(checked_file(e['binary'])) for r,e in cfg['binaries'].items()}
    for role,e in cfg['binaries'].items():
        need(e['prefix']==['/usr/bin/taskset','-c',str(cfg['cpu'])],'fresh registered taskset prefix differs')
        opts=e['effective_argv'];need('-O3' in opts and '-DNDEBUG' in opts and '-UNDEBUG' not in opts and '-freflection' in opts and '-fexpansion-statements' in opts,'bare original optimized P2996 profile required')
        need(not any('sanitize' in x or x in ('-include','-include-pch') or 'common_diag' in x or 'C14_DIAG' in x for x in opts),'diagnostic/SAN/forced instrumentation rejected')
        metadata=C.read(checked_file(e['metadata']));entry=metadata['binaries']['xoffset']
        need(all(e[k]==entry[k] for k in ('library','commit','trust_mode','load_contract','source_content_sha256')) and e['binary']['path']==entry['path'] and e['load_contract']=='detached','actual profile metadata differs')
        rows=C.read(checked_file(e['compile_commands']));rows=[r for r in rows if 'CMakeFiles/sc_xoffset.dir/' in r.get('command','')+' '+r.get('output','')]
        need(len(rows)==1 and (rows[0].get('arguments') or shlex.split(rows[0]['command']))==opts,'actual compile row differs')
        adapter=checked_file(e['adapter']).read_text()
        for key in ('library','commit','trust_mode','source_content_sha256'):
            found=re.search(r'\b'+('library_id' if key=='library' else key)+r'\s*=\s*"([^"\n]+)"',adapter)
            need(found is not None and found.group(1)==e[key],'actual adapter literal differs')
    required={'compiler','wrapper','profile_wrapper','libcxx','runtime','driver','adapter','dependencies','helper','request','full_functional','full_request','cost_policy','preconditions'}
    need(required<=set(cfg['closure']),'complete runtime/driver/receipt/cost closure missing')
    checked_file(DESIGN['compiler_launcher'])
    sources=source_snapshot(cfg)
    for role,key in (('control','control-dependencies'),('candidate','dependencies')):
        dependency=C.read(path(cfg['closure'][key]))
        need(dependency['status']=='passed' and dependency['candidate_actual_header_binding_verified'] is True and dependency['production_files']==sources[role]['production'],'actual compiled-dependency production closure differs: '+role)
        need(dependency['new_binary']['sha256']==binaries[role]['sha256'] and int(dependency['new_binary']['bytes'])==int(binaries[role]['bytes']) and dependency['profile']['sha256']==cfg['binaries'][role]['metadata']['identity']['sha256'],'actual compiled binary/profile binding differs: '+role)
    closure={n:C.tree_id(path(p)) for n,p in cfg['closure'].items()}
    fixture=path(cfg['fixture']);G.fixture_check(fixture,16384)
    return dict(sources=sources,binaries=binaries,closure=closure,tools=tools(),design=fid(DESIGN_PATH),input_policy=input_policy(cfg),
                metadata={r:fid(checked_file(e['metadata'])) for r,e in cfg['binaries'].items()},fixture=C.inventory(fixture,exclude=()),input_wire=fid(checked_file(cfg['input_wire'])),environment=cfg['environment'])
