"""Four-path conditioned Q32/F64 executor; actual root identity binding pending.

Derived source-only from the sealed V5 conditioned executor. No generator runs
inside a measurement window. The root coordinator must bind the actual V8 and
final candidate identities in STAGE_BINDINGS before this source can configure.
The original condition/primary schedule, lifecycle, resource and power mechanics
remain intact. Only the primary-cohort phase projection enters the new math.
"""
from pathlib import Path
import argparse, datetime, fcntl, hashlib, json, math, os, shutil, stat, sys, time, types
sys.dont_write_bytecode=True

R=Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
B=Path('/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1')
PREVIOUS=Path('/root/xoffset-receive-completion/20261019/v1')
COORD=R/'docs/coordination/c14-iteration-20261020-v2/validation/final_formal_v1'
BASELINE_ORIGIN_PROFILE=PREVIOUS/'profile-normal-v2.json'
CANDIDATE_ORIGIN_PROFILE=Path('/root/xoffset-c14-iteration/20261020/v2/validation/combined-business-v1/profile-normal-v2.json')
BASELINE_FIXTURE=PREVIOUS/'functional-normal-v2/prepared/M'
LIVE_RUNNER=Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发/experiments/vm_coordination/run_job.py')
RESOURCE_TEST_RECEIPT=Path('/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/registration/resources-pure-tests-v3.json')
HERE=Path(__file__).resolve().parent
# Root binds this one dictionary from accepted actual final profile/header/binary
# records before source freeze. None is deliberately non-executable admission.
# No V4/V5 old constant is accepted as a placeholder for the V8/final identities.
STAGE_BINDINGS={'baseline': {'binary': {'bytes': '225856',
                         'sha256': '7ff88829110ceea397bd9baded724bc1438e6645e3b39a6b67934873de1a11d3'},
              'closure_ids': {'candidate-main-header': {'bytes': '278262',
                                                        'sha256': '66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3'},
                              'candidate-os-header': {'bytes': '20900',
                                                      'sha256': '90105e4c172da26a5b23cbb10021a18613b458e92b795beee8ec9d7faed72991'},
                              'candidate-receive-future-hpp': {'bytes': '3052',
                                                               'sha256': '42b2f9aac5286a3c56d62d706bf8d4528bb94eb5c3f27687464a0bcb2e306651'},
                              'candidate-receive-raw-fwd-hpp': {'bytes': '4997',
                                                                'sha256': '1933345bb64eb363a435aff86745318dcb97d4c72d50754674c64db8bbdcd9ca'},
                              'candidate-receive-raw-hpp': {'bytes': '50646',
                                                            'sha256': 'a6a5f4176b6c94acaa54999c01a64ad8f4570855005eef4b506d17647a1ba3a0'},
                              'candidate-snapshot-index-runtime-cpp': {'bytes': '525',
                                                                       'sha256': '1208ff468efddeacc86d4e7f5880f106307a4dd348ff4e7ff2883806d509a79b'},
                              'candidate-source-identity': {'bytes': '2969',
                                                            'sha256': '48f313f3481f516e49d6fd46a5a60362d1680e4736a9b493a94cf72548483de6'},
                              'candidate-validation-scratch-cpp': {'bytes': '408',
                                                                   'sha256': 'b16223714ed972aa17c7f8243216930969bfbaab58398b3de5e85ec08f5b6fcd'},
                              'candidate-validation-scratch-hpp': {'bytes': '15756',
                                                                   'sha256': 'edaf40d8cb472acfaddca5938bbb3dbf4c31a54d4c8fa03cea6b25dc0084592c'},
                              'candidate-xoffset-core-lifetime-hpp': {'bytes': '7269',
                                                                      'sha256': '1fed92b34c842b14905502db4f35ff93e66b4d102e59b7e37df3969db8e8f268'},
                              'candidate-xoffset-snapshot-index-budget-hpp': {'bytes': '3042',
                                                                              'sha256': '724dba8fee57faa6f19d596e2726cc5198a3d5f1054867f5014d9db8f7f02b4d'},
                              'candidate-xoffset-snapshot-index-hpp': {'bytes': '11831',
                                                                       'sha256': '6286b20288bd43cff22006c5169eab3585960276004a526e32974bf3dcbf63f9'},
                              'candidate-xoffset-validation-scratch-hpp': {'bytes': '1094',
                                                                           'sha256': '63e673b17d9ba38bda4ade0effe9ec15cf3ac367fa8c5f18c189277211ebddb3'},
                              'candidate-xoffset-verified-paths-hpp': {'bytes': '10209',
                                                                       'sha256': 'a3e69dc13d595bbe94955e0a940b1def5f07b76300e2d5d84242d4d1383e3512'},
                              'candidate-xoffset-verified-receive-hpp': {'bytes': '15907',
                                                                         'sha256': 'f92b2bff7b812ef4bb53687c093ed0e73d13bf1b0defaf282447a5ad02abab4a'}},
              'content_sha256': '26299f796e2ba289ab15b38b000a785ad1877fbc65ce29cf3071d2118804c3b4',
              'identity': ['xoffset-receive-completion-v1', '7539433ab96d69c4a07b1db5c0a13786da85cdcd']},
 'candidate': {'binary': {'bytes': '229352',
                          'sha256': '8b52729c014885a4f4df24dac1a8fc7d1e989e4c3903ea1792a4fde5795f57b8'},
               'closure_ids': {'candidate-main-header': {'bytes': '279066',
                                                         'sha256': '155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501'},
                               'candidate-os-header': {'bytes': '20900',
                                                       'sha256': '90105e4c172da26a5b23cbb10021a18613b458e92b795beee8ec9d7faed72991'},
                               'candidate-receive-future-hpp': {'bytes': '3052',
                                                                'sha256': '42b2f9aac5286a3c56d62d706bf8d4528bb94eb5c3f27687464a0bcb2e306651'},
                               'candidate-receive-raw-fwd-hpp': {'bytes': '4997',
                                                                 'sha256': '1933345bb64eb363a435aff86745318dcb97d4c72d50754674c64db8bbdcd9ca'},
                               'candidate-receive-raw-hpp': {'bytes': '50646',
                                                             'sha256': 'a6a5f4176b6c94acaa54999c01a64ad8f4570855005eef4b506d17647a1ba3a0'},
                               'candidate-snapshot-index-runtime-cpp': {'bytes': '525',
                                                                        'sha256': '1208ff468efddeacc86d4e7f5880f106307a4dd348ff4e7ff2883806d509a79b'},
                               'candidate-source-identity': {'bytes': '2550',
                                                             'sha256': 'a160f395702a70c5837f4f7d3433b9820e106de3082ca6160bf3c29483d61b1e'},
                               'candidate-validation-scratch-cpp': {'bytes': '408',
                                                                    'sha256': 'b16223714ed972aa17c7f8243216930969bfbaab58398b3de5e85ec08f5b6fcd'},
                               'candidate-validation-scratch-hpp': {'bytes': '15756',
                                                                    'sha256': 'edaf40d8cb472acfaddca5938bbb3dbf4c31a54d4c8fa03cea6b25dc0084592c'},
                               'candidate-xoffset-core-lifetime-hpp': {'bytes': '7269',
                                                                       'sha256': '1fed92b34c842b14905502db4f35ff93e66b4d102e59b7e37df3969db8e8f268'},
                               'candidate-xoffset-snapshot-index-budget-hpp': {'bytes': '3042',
                                                                               'sha256': '724dba8fee57faa6f19d596e2726cc5198a3d5f1054867f5014d9db8f7f02b4d'},
                               'candidate-xoffset-snapshot-index-hpp': {'bytes': '11831',
                                                                        'sha256': '6286b20288bd43cff22006c5169eab3585960276004a526e32974bf3dcbf63f9'},
                               'candidate-xoffset-validation-scratch-hpp': {'bytes': '1094',
                                                                            'sha256': '63e673b17d9ba38bda4ade0effe9ec15cf3ac367fa8c5f18c189277211ebddb3'},
                               'candidate-xoffset-verified-paths-hpp': {'bytes': '10209',
                                                                        'sha256': 'a3e69dc13d595bbe94955e0a940b1def5f07b76300e2d5d84242d4d1383e3512'},
                               'candidate-xoffset-verified-receive-hpp': {'bytes': '15907',
                                                                          'sha256': 'f92b2bff7b812ef4bb53687c093ed0e73d13bf1b0defaf282447a5ad02abab4a'}},
               'content_sha256': '8cd90345f109308d150caa141e49fa65e2e7b1fac957957e8e45bc8ee8eb5518',
               'identity': ['xoffset-c14-r1-m2-combined-v1',
                            '7539433ab96d69c4a07b1db5c0a13786da85cdcd']},
 'competitor_binaries': {'capnproto': {'bytes': '556064',
                                       'sha256': '8808a8c1abc52538c2bce7d01a7ad1074421d1c7224d5d0f19853df7512aff6b'},
                         'cereal': {'bytes': '152784',
                                    'sha256': 'e169afb796486b46880ad0ccf7d66aa650e1aeb49056a907ea73bd63b20b5fcf'},
                         'cista': {'bytes': '180456',
                                   'sha256': 'd5fc2c2c73e5a52d4ed37a2ab33ce1a7cb5e0026e7c3575a4800a63894ded46d'},
                         'flatbuffers': {'bytes': '150088',
                                         'sha256': '105909a60974ad6aaf53ec0373d38a8f24da0f9a84b2bebd7bb1947b0f148ead'}},
 'fixed_input_captures': None,
 'sealed_initial_wires': {'capnproto': {'identity': {'bytes': '9055872',
                                                     'sha256': 'cc27816e0af6bc816624fa50840117bd5cf79071ab44e2e2ad84d2158420167b'},
                                        'path': '/root/xoffset-performance-v5/20261018/v1/functional-normal-v1/commands/capnproto-M.input.wire'},
                          'cereal': {'identity': {'bytes': '8063804',
                                                  'sha256': 'aba30c823a57f80470a928cfd15c94d6103ec1f7226ebb6f5868bfd5281cedfb'},
                                     'path': '/root/xoffset-performance-v5/20261018/v1/functional-normal-v1/commands/cereal-M.input.wire'},
                          'cista': {'identity': {'bytes': '10315104',
                                                 'sha256': '1d54344941e86b63bd3525b694971a0593a009d022052f440c89db20c2d9b84a'},
                                    'path': '/root/xoffset-performance-v5/20261018/v1/functional-normal-v1/commands/cista-M.input.wire'},
                          'flatbuffers': {'identity': {'bytes': '8990248',
                                                       'sha256': 'd53fb8e89a71eef77efd47f680c1c37bd0e187a5bb93fc2243eb7fa59a05987f'},
                                          'path': '/root/xoffset-performance-v5/20261018/v1/functional-normal-v1/commands/flatbuffers-M.input.wire'},
                          'xoffset': {'identity': {'bytes': '13702104',
                                                   'sha256': '9cd8ab46cea310b19f7617bca29a7f1afcd7dc9c45888704ce058df0c60efe84'},
                                      'path': '/root/xoffset-performance-v8/20261018/v1/functional-normal-v1/commands/xoffset-M.input.wire'}}}
BASELINE_BINARY_ID=None
BASELINE_CONTENT=None
BASELINE_CLOSURE_IDS=None
FIXED_BINARY_IDS=None
TOOL_MEMBERS={
    'six-role-campaign.py':'six-role-campaign.py',
    'phase_protocol.py':'four-path-plan-support-20261018/statistics/phase_protocol.py',
    'comparison_protocol.py':'performance-v5/measurement/comparison_protocol.py',
    'conditioning_envelope.py':'conditioning_envelope.py',
    'runtime_resources.py':'runtime_resources.py',
    'power_environment.py':'power_environment.py',
    'power-control.ps1':'power-control.ps1',
    'campaign.py':'campaign.py','core.py':'core.py','protocol.py':'protocol.py',
    'run_job.py':'run_job.py'}
MATH_PROVENANCE_PATH='tools/performance-v5/measurement/comparison_protocol.py'
CAPTURE_BODIES=('.input.wire','.wire','.native.json','.canonical','.queries',
    '.initial.canonical','.reloaded.canonical','.validation.json')
CAPTURE_ACTIONS=('inspect-input','validate','run','inspect-final')
CAPTURE_METADATA=('.native.json','.validation.json',
    *(f'-{action}{suffix}' for action in CAPTURE_ACTIONS for suffix in ('.receipt.json','.stdout','.stderr')))
CAPTURE_SUFFIXES=(*CAPTURE_BODIES,*(f'-{action}{suffix}' for action in CAPTURE_ACTIONS
                                  for suffix in ('.receipt.json','.stdout','.stderr')))
NATIVE_METADATA=65536
CAMPAIGN=None
REGISTRATION_FILES=('measurement-decision-v1.json','measurement-decision-v2.json','backing-volume-windows-v1.json',
    'backing-volume-verification-v1.json','power-capability-v2.json','power-capability-v3.json',
    'resources-pure-tests-v3.json')


def need(condition,message): K.require(condition,message)

def copy(source,target): return K.copy_file(source,target)

def put(path,value): K.write(path,value)

def tool_paths():
    return [Path(__file__),STATS,T_SOURCE,ENVELOPE,RESOURCES,POWER_MODULE,POWER_HELPER,
            Path(G.__file__),Path(K.__file__),Path(G.P.__file__),RUNNER]

def tool_inventory(directory):
    """Exactly eleven source files; nested math paths preserve its unchanged import."""
    members=K.inventory(directory,exclude=())
    need(set(members)==set(TOOL_MEMBERS.values()),'archived tool membership differs')
    return {name:members[relative] for name,relative in TOOL_MEMBERS.items()}

def statistical_policy(kind):
    """Normalize only a provenance spelling that changes when the bundle moves."""
    value=P.policy(kind)
    frozen=value['frozen_source']
    need(frozen['bytes']==P.FROZEN_BYTES and frozen['sha256']==P.FROZEN_SHA256
         and Path(frozen['path'])==T_SOURCE,'math source provenance differs')
    return {**value,'frozen_source':{**frozen,'path':MATH_PROVENANCE_PATH}}

def analyze(samples,kind):
    value=P.analyze(samples,kind)
    need(K.same(value['policy'],P.policy(kind)),'math result/policy differs')
    return {**value,'policy':statistical_policy(kind)}

def primary_sample(job,native):
    # Native truth/parser checks the complete row. The math independently checks
    # exact phase names, canonical uint64 decimal domains, sum and prefix order.
    return {'job':dict(job),'phase_ns':dict(native['phase_ns']),
            'total_ns':native['total_ns']}

def bind_stage(kind):
    global BASELINE_BINARY_ID,BASELINE_CONTENT,BASELINE_CLOSURE_IDS,FIXED_BINARY_IDS
    K.exact(STAGE_BINDINGS,('baseline','candidate','competitor_binaries','sealed_initial_wires',
                           'fixed_input_captures'),'root stage bindings')
    def role_binding(value,label):
        K.exact(value,('binary','content_sha256','closure_ids','identity'),label)
        K.validate_id(value['binary'])
        need(K.hexsha(value['content_sha256']) and type(value['identity']) is list
             and len(value['identity'])==2 and all(type(x) is str and x for x in value['identity']),
             'invalid '+label+' source/native identity')
        closure=value['closure_ids']
        need(type(closure) is dict and {'candidate-source-identity','candidate-main-header',
             'candidate-os-header'}<=set(closure) and all(type(k) is str and k for k in closure),
             'missing complete '+label+' production header bindings')
        for identity in closure.values():K.validate_id(identity)
        return value
    baseline=role_binding(STAGE_BINDINGS['baseline'],'baseline')
    candidate=role_binding(STAGE_BINDINGS['candidate'],'candidate')
    competitors=STAGE_BINDINGS['competitor_binaries']
    need(type(competitors) is dict and set(competitors)==set(P.COMPETITORS),
         'sealed four-competitor set differs')
    for identity in competitors.values():K.validate_id(identity)
    inputs=STAGE_BINDINGS['sealed_initial_wires']
    need(type(inputs) is dict and set(inputs)=={'xoffset',*P.COMPETITORS},
         'sealed five-library initial input set differs')
    for source in inputs.values():
        K.exact(source,('path','identity'),'sealed initial input source')
        need(K.absolute_string(source['path']),'sealed initial source path must be absolute')
        K.validate_id(source['identity'])
    captures=K.exact(STAGE_BINDINGS['fixed_input_captures'],('normal','sanitized'),'fixed input captures')
    for mode,group in captures.items():
        K.exact(group,('environment','roles'),'capture mode')
        need(type(group['environment']) is dict and group['environment']
             and all(type(k) is str and type(v) is str and '\0' not in k+v
                     for k,v in group['environment'].items()),'invalid capture environment')
        need(type(group['roles']) is dict and set(group['roles'])==set(P.ROLES),
             'six normal and six sanitized fixed-input captures required')
        for role,record in group['roles'].items():
            K.exact(record,('prefix','binary','files'),'fixed input capture record')
            need(K.absolute_string(record['prefix']) and not Path(record['prefix']).suffix,
                 'capture prefix must be absolute and have no suffix')
            K.exact(record['binary'],('path','identity'),'capture binary')
            need(K.absolute_string(record['binary']['path']),'relative capture binary')
            K.validate_id(record['binary']['identity'])
            need(type(record['files']) is dict and set(record['files'])==set(CAPTURE_SUFFIXES),
                 'fixed capture twenty-file set differs')
            for file_identity in record['files'].values():K.validate_id(file_identity)
    # Qualification exercises all six actual future roles, including the final
    # candidate. An A/A pass for two V8 labels cannot qualify candidate behavior.
    active=candidate
    need(G.P.CANDIDATE_CONTENT_SHA256==active['content_sha256']
         and K.same(G.P.CANDIDATE_CLOSURE_IDS,active['closure_ids'])
         and tuple(G.P.IDENTITIES['xoffset'])==tuple(active['identity']),
         'maintained parser is not bound to complete active production source')
    BASELINE_BINARY_ID=baseline['binary'];BASELINE_CONTENT=baseline['content_sha256']
    BASELINE_CLOSURE_IDS=baseline['closure_ids']
    FIXED_BINARY_IDS={'old_xoffset':BASELINE_BINARY_ID,
        'new_xoffset':candidate['binary'],**competitors}

def fixed_binary_domains(basis):
    need(K.same(basis['binaries'],FIXED_BINARY_IDS),'fixed actual six-role binary identities differ')

def schedules():
    primary=P.schedule(CAMPAIGN);actual=E.schedule(primary,CAMPAIGN);e=E.policy(CAMPAIGN)
    need(len(primary)==e['required_primary_children'] and len(actual)==e['maximum_actual_children']
         and len({j['id'] for j in actual})==len(actual),'fixed schedule counts/IDs differ')
    return primary,actual

def budgets():
    p=P.policy(CAMPAIGN);e=E.policy(CAMPAIGN)
    need(p['maximum_child_seconds']==20 and e['maximum_child_seconds']==20
         and p['maximum_window_seconds']==e['maximum_window_seconds']==MAX_SECONDS
         and p['cpu_affinity_virtual_cpu']==e['cpu_affinity_virtual_cpu']==2
         and p['maximum_children']==e['required_primary_children']
         and e['maximum_actual_children']==2*p['maximum_children'],'statistical/envelope budgets differ')
    need(set(E.resource_policy(CAMPAIGN))==set(L.KEYS),'resource helper policy differs')

def projection(frozen,jobs):
    """Fixed body sizes plus conservative per-child metadata; no measured times."""
    sizes=K.read(frozen/'status.json')['final_wire_bytes']
    side=(frozen/'fixture/final.canonical.bin').stat().st_size+(frozen/'fixture/queries.bin').stat().st_size
    return sum(sizes[j['role']]+side+NATIVE_METADATA for j in jobs)

def regular_bytes(directory):
    total=0
    for path in Path(directory).rglob('*'):
        mode=path.lstat().st_mode
        need(stat.S_ISDIR(mode) or stat.S_ISREG(mode),'output link/special member')
        if stat.S_ISREG(mode): total+=path.stat().st_size
    return total

def wire_usage(out,jobs):
    by_id={j['id']:j for j in jobs}; total=primary=0
    for path in (out/'commands').glob('*.wire'):
        need(path.stem in by_id and stat.S_ISREG(path.lstat().st_mode),'unknown/nonregular wire output')
        size=path.stat().st_size; total+=size
        if by_id[path.stem]['execution_kind']=='primary': primary+=size
    return total,primary

def accounting(jobs,completed,attempted,primary_jobs):
    """Only exact successful actual prefixes may project into primary samples."""
    need(type(completed) is list and type(attempted) is list,'invalid actual ledgers')
    need(len(completed)<=len(attempted)<=min(len(jobs),len(completed)+1),'invalid success/attempt lengths')
    need(K.same(completed,jobs[:len(completed)])
         and K.same(attempted,jobs[:len(attempted)]),'actual ledger is not exact schedule prefix')
    selected=[primary_jobs[j['primary_index']] for j in completed if j['execution_kind']=='primary']
    need(K.same(selected,primary_jobs[:len(selected)]),'primary projection is not original schedule prefix')
    return selected

def queue_domains(out,queue):
    manifest=K.exact(K.read(out/'queue/manifest.json'),('schema_version','files'),'queue manifest')
    need(type(manifest['schema_version']) is int and manifest['schema_version']==1
         and type(manifest['files']) is list,'invalid queue manifest domain')
    for item in manifest['files']:
        K.exact(item,('path','bytes','sha256'),'queue file');K.relative(item['path'])
        need(K.integer(item['bytes']) and K.hexsha(item['sha256']),'invalid queue member domain')
    request=K.read(out/'queue/request.json');queue_timeout(request['timeout_seconds'])
    need(type(queue['schema_version']) is int and queue['schema_version']==1 and queue['kind']=='timing'
         and (queue['command_returncode'] is None or type(queue['command_returncode']) is int),'invalid queue result domain')
    context=K.read(out/'execution-context.json')
    need(set(context['inputs'])=={'--frozen'},'queue frozen binding missing/extra')

def profile_domains(profile):
    K.exact(profile,('schema_version','platform_class','lock_dir','environment','binaries','closure'),'six role profile')
    need(type(profile['schema_version']) is int and profile['schema_version']==1
         and profile['platform_class']=='wsl2-vm-x86_64' and K.absolute_string(profile['lock_dir']),'invalid profile domain')
    env=profile['environment']
    need(type(env) is dict and bool(env) and all(type(k) is str and type(v) is str and '\0' not in k+v
         for k,v in env.items()),'invalid execution environment')
    need(type(profile['binaries']) is dict and set(profile['binaries'])==set(P.ROLES),'invalid role set')
    for role,entry in profile['binaries'].items():
        is_x=role in ('old_xoffset','new_xoffset');lib='xoffset' if is_x else role
        fields=('path','prefix','library','commit','trust_mode','load_contract')
        K.exact(entry,(*fields,'source_content_sha256') if is_x else fields,'role profile')
        identity=tuple(STAGE_BINDINGS['baseline']['identity']) if role=='old_xoffset' else G.P.IDENTITIES[lib]
        need(K.absolute_string(entry['path']) and K.same(entry['prefix'],['/usr/bin/taskset','-c','2'])
             and (entry['library'],entry['commit'])==identity and type(entry['trust_mode']) is str
             and bool(entry['trust_mode']) and entry['load_contract']=='detached','role identity/affinity/ownership differs')
        if is_x:
            expected=BASELINE_CONTENT if role=='old_xoffset' else G.P.CANDIDATE_CONTENT_SHA256
            need(entry['source_content_sha256']==expected,'fixed source content differs')
    # Q and F both use the actual baseline/final pair; each role's two within-
    # block replicas supply its own A/A contrast. The roles need not be equal.
    need(type(profile['closure']) is dict and bool(profile['closure']) and all(type(k) is str and bool(k)
         and K.absolute_string(v) for k,v in profile['closure'].items()),'invalid closure')

def profile_from_origins(new,old,proposal_path=None):
    for origin,critical in ((new,G.P.CANDIDATE_CLOSURE_IDS),(old,BASELINE_CLOSURE_IDS)):
        K.exact(origin,('schema_version','platform_class','lock_dir','environment','binaries','closure'),'origin profile')
        need(type(origin['schema_version']) is int and origin['schema_version']==1
             and type(origin['binaries']) is dict and set(origin['binaries'])==set(G.P.LIBRARIES)
             and type(origin['closure']) is dict,'invalid origin profile domain')
        need(set(critical)<=set(origin['closure']),
             'origin critical identity/header labels missing')
        need(all(type(k) is str and K.absolute_string(v) for k,v in origin['closure'].items()),
             'invalid origin closure path domain')
    need(K.same(new['environment'],old['environment']) and new['lock_dir']==old['lock_dir']
         and new['platform_class']==old['platform_class']=='wsl2-vm-x86_64','origin runtime/platform/lock differs')
    entries={role:(old if role=='old_xoffset' else new)['binaries'][
        'xoffset' if role in ('old_xoffset','new_xoffset') else role] for role in P.ROLES}
    proposal_path=str(PROPOSAL.resolve()) if proposal_path is None else proposal_path
    need(K.absolute_string(proposal_path),'proposal original path must be absolute')
    paths=sorted(set(new['closure'].values())|set(old['closure'].values())|{proposal_path})
    closure={'closure-%04d'%i:path for i,path in enumerate(paths)}
    # Preserve fixed singleton labels in addition to the full union. These are
    # validated from captured metadata offline, never by reopening old paths.
    closure.update({label:new['closure'][label] for label in G.P.CANDIDATE_CLOSURE_IDS})
    closure.update({'baseline-'+label:old['closure'][label] for label in BASELINE_CLOSURE_IDS})
    result={'schema_version':1,'platform_class':new['platform_class'],'lock_dir':new['lock_dir'],
        'environment':new['environment'],'binaries':entries,'closure':closure}
    profile_domains(result)
    return result

def captured_candidate_domains(profile,basis):
    G.candidate_runtime_identity(basis)
    need(set(basis['closure'])==set(profile['closure']),'captured/original closure labels differ')
    for label,identity in BASELINE_CLOSURE_IDS.items():
        need(K.same(basis['closure'].get('baseline-'+label),{'.':identity}),
             'captured V8 baseline identity/header differs: '+label)
    by_path={}
    for label,path in profile['closure'].items():
        if path in by_path:
            need(K.same(basis['closure'][label],by_path[path]),'same original path captured inconsistently')
        else: by_path[path]=basis['closure'][label]

def capture_path(record,suffix):
    need(suffix in CAPTURE_SUFFIXES,'unknown fixed capture suffix')
    return Path(record['prefix']+suffix)

def capture_archive_path(directory,mode,role,suffix):
    need(mode in ('normal','sanitized') and role in P.ROLES and suffix in CAPTURE_METADATA,
         'invalid capture archive member')
    return directory/'functional-captures'/mode/role/('capture'+suffix)

def capture_argv(record,entry,action):
    """Original provenance strings only; offline verification never opens them."""
    prefix=Path(record['prefix']);fixture=prefix.parent.parent/'prepared/M'
    native_entry={**entry,'path':record['binary']['path']}
    if action=='inspect-input':
        return G.command(native_entry,'inspect','--wire',capture_path(record,'.input.wire'),
                         '--canonical',capture_path(record,'.initial.canonical'))
    if action=='validate':
        return G.command(native_entry,'validate','--wire',capture_path(record,'.input.wire'),
                         '--trace',fixture/'trace.bin','--result',capture_path(record,'.validation.json'))
    if action=='inspect-final':
        return G.command(native_entry,'inspect','--wire',capture_path(record,'.wire'),
                         '--canonical',capture_path(record,'.reloaded.canonical'))
    need(action=='run','unknown fixed capture action')
    return G.command(native_entry,'run','--wire',capture_path(record,'.input.wire'),
        '--trace',fixture/'trace.bin','--wire-out',capture_path(record,'.wire'),
        '--canonical-out',capture_path(record,'.canonical'),'--queries-out',capture_path(record,'.queries'),
        '--result',capture_path(record,'.native.json'))

def verify_input_captures(directory,profile):
    """Verify small archived records plus copied old inputs; no old path reopen."""
    captured=K.exact(K.read(directory/'input-captures.json'),('schema_version','captures'),'input captures')
    need(type(captured['schema_version']) is int and captured['schema_version']==3
         and K.same(captured['captures'],STAGE_BINDINGS['fixed_input_captures']),
         'frozen dedicated capture binding differs')
    captures=captured['captures'];fixture=directory/'fixture'
    initial=K.file_id(fixture/'input.canonical.bin');canonical=K.file_id(fixture/'final.canonical.bin')
    queries=K.file_id(fixture/'queries.bin')
    digests={key:K.sha(fixture/name) for key,name in
        [('trace_sha256','trace.bin'),('query_before_sha256','query-before.bin'),('query_after_sha256','query-after.bin')]}
    need(K.same(captures['normal']['environment'],profile['environment']),
         'normal fixed capture environment differs from measured profile')
    for mode,group in captures.items():
      for role,record in group['roles'].items():
        entry=profile['binaries'][role];files=record['files']
        lib='xoffset' if role in ('old_xoffset','new_xoffset') else role
        source=STAGE_BINDINGS['sealed_initial_wires'][lib]
        need(files['.input.wire']==source['identity']
             and K.file_id(directory/'input-wires'/(role+'.wire'))==source['identity'],
             'dedicated or frozen input differs from sealed old library input: '+role)
        if mode=='normal':
            need(record['binary']=={'path':entry['path'],'identity':FIXED_BINARY_IDS[role]},
                 'normal capture does not use actual measured role binary')
        need(files['.initial.canonical']==initial and files['.canonical']==canonical
             and files['.reloaded.canonical']==canonical and files['.queries']==queries,
             'dedicated inspect/run output does not match complete truth')
        for suffix in CAPTURE_METADATA:
            need(K.file_id(capture_archive_path(directory,mode,role,suffix))==files[suffix],
                 'archived dedicated capture member changed')
        previous=0;receipts={}
        for action in CAPTURE_ACTIONS:
            receipt=K.read(capture_archive_path(directory,mode,role,'-'+action+'.receipt.json'))
            started=K.decimal(receipt['started_monotonic_ns']);finished=K.decimal(receipt['finished_monotonic_ns'])
            need(type(receipt['returncode']) is int and receipt['returncode']==0
                 and receipt['timed_out'] is False and receipt['launch_error'] is None
                 and type(receipt['pid']) is int and receipt['pid']>0
                 and type(receipt['timeout_seconds']) in (int,float)
                 and math.isfinite(receipt['timeout_seconds']) and 0<receipt['timeout_seconds']<=60
                 and previous<=started<finished and finished-started<=receipt['timeout_seconds']*10**9
                 and K.same(receipt['argv'],capture_argv(record,entry,action))
                 and K.same(receipt['execution_environment'],group['environment']),
                 'dedicated capture argv/environment/receipt differs')
            previous=finished;receipts[action]=receipt
            for suffix in ('.stdout','.stderr'):
                need(files['-'+action+suffix]['bytes']=='0','unexpected dedicated capture stdout/stderr')
        G.validate_stages(capture_archive_path(directory,mode,role,'.validation.json'),entry,fixture)
        native=K.exact(K.read(capture_archive_path(directory,mode,role,'.native.json')),G.NATIVE_KEYS,
                       'captured native result')
        need(type(native['schema_version']) is int and native['schema_version']==1
             and native['kind']=='serialization-comparison-native-result' and native['status']=='passed'
             and native['library']==entry['library']
             and native['input_sha256']==source['identity']['sha256']
             and native['final_wire_sha256']==files['.wire']['sha256']
             and native['final_canonical_sha256']==canonical['sha256']
             and native['queries_sha256']==queries['sha256']
             and all(native[key]==value for key,value in digests.items()),
             'dedicated old-input native capture/truth differs')
        K.exact(native['phase_ns'],P.PHASE_KEYS,'captured phase vector')
        total=K.decimal(native['total_ns'],positive=True)
        receipt=receipts['run']
        interval=K.decimal(receipt['finished_monotonic_ns'])-K.decimal(receipt['started_monotonic_ns'])
        need(sum(K.decimal(value) for value in native['phase_ns'].values())==total<=interval,
             'dedicated capture timer bounds differ')
        need(files['.wire']['bytes']==captures['normal']['roles'][role]['files']['.wire']['bytes'],
             'sanitized/normal fixed model final sizes differ')
    return captures

def capture_inputs(directory,profile):
    """Live freeze only: audit pinned dedicated outputs, then archive metadata."""
    captures=STAGE_BINDINGS['fixed_input_captures'];fixture=directory/'fixture'
    for mode,group in captures.items():
      for role,record in group['roles'].items():
        need(K.file_id(record['binary']['path'])==record['binary']['identity'],'capture binary changed')
        for suffix in CAPTURE_SUFFIXES:
            need(K.file_id(capture_path(record,suffix))==record['files'][suffix],
                 'dedicated capture source identity changed: '+role+suffix)
        entry={**profile['binaries'][role],'path':record['binary']['path']}
        receipt=K.read(capture_path(record,'-run.receipt.json'))
        G.native_result(capture_path(record,'.native.json'),entry,fixture,capture_path(record,'.input.wire'),
            capture_path(record,'.wire'),capture_path(record,'.canonical'),capture_path(record,'.queries'),receipt,
            expected_wire_bytes=int(record['files']['.wire']['bytes']))
        G.validate_stages(capture_path(record,'.validation.json'),entry,fixture)
        for suffix in CAPTURE_METADATA:
            copy(capture_path(record,suffix),capture_archive_path(directory,mode,role,suffix))
    for role in P.ROLES:
        lib='xoffset' if role in ('old_xoffset','new_xoffset') else role
        source=STAGE_BINDINGS['sealed_initial_wires'][lib]
        need(K.file_id(source['path'])==source['identity'],'sealed initial source changed')
        copy(source['path'],directory/'input-wires'/(role+'.wire'))
    put(directory/'input-captures.json',{'schema_version':3,'captures':captures})
    verify_input_captures(directory,profile)
    return {role:int(record['files']['.wire']['bytes'])
            for role,record in captures['normal']['roles'].items()}

def tools():
    paths=tool_paths()
    need(len(paths)==len(TOOL_MEMBERS) and {p.name for p in paths}==set(TOOL_MEMBERS),
         'eleven-tool source set differs')
    return {p.name:K.file_id(p) for p in paths}

def runtime(profile):
    return {'binaries':{role:K.file_id(e['path']) for role,e in profile['binaries'].items()},
        'closure':{name:K.tree_id(path) for name,path in profile['closure'].items()},'tools':tools(),
        'interpreter':{'path':str(Path(sys.executable).resolve()),'identity':K.file_id(Path(sys.executable).resolve())},
        'environment':profile['environment'],'platform_class':profile['platform_class'],
        'power_executables':{name:K.file_id(path) for name,path in power_paths().items()}}

def payload_modes(directory):
    result={}
    for path in sorted(Path(directory).rglob('*')):
        mode=path.lstat().st_mode
        need(stat.S_ISDIR(mode) or stat.S_ISREG(mode),'special/link frozen member')
        if stat.S_ISDIR(mode):
            result[path.relative_to(directory).as_posix()+'/']=stat.S_IMODE(mode)
        else:
            rel=path.relative_to(directory).as_posix()
            need(not rel.endswith('.pyc') and '__pycache__' not in path.parts,'frozen bytecode')
            if rel not in ('modes.json','manifest.json','status.json'):
                result[rel]=stat.S_IMODE(mode)
    return result

def copy_frozen(source,target,pin):
    verify_frozen(source,pin)
    need(not target.exists(),'copied frozen output exists')
    executables={'bin/'+role for role in P.ROLES}
    members=K.inventory(source,exclude=(),exclude_roots=('queue',))
    need({n for n in members if n.startswith('bin/')}==executables,'six frozen binary members differ')
    for name in members:
        destination=target/name; copy(source/name,destination)
        if name in executables:
            fd=os.open(destination,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
            try:
                need(stat.S_ISREG(os.fstat(fd).st_mode),'copied binary not regular')
                os.fchmod(fd,0o755)
                a,b=os.fstat(fd),destination.lstat()
                need((a.st_dev,a.st_ino)==(b.st_dev,b.st_ino) and stat.S_ISREG(b.st_mode)
                     and stat.S_IMODE(b.st_mode)==0o755,'copied binary mode/inode differs')
            finally: os.close(fd)
    verify_frozen(target,pin)

def verify_frozen(directory,pin):
    directory=Path(directory);K.check_inventory(directory,pin);budgets();primary,actual=schedules()
    need(K.same(K.read(directory/'modes.json'),payload_modes(directory)),'frozen modes changed')
    for name,value in [('schedule.json',primary),('native-schedule.json',actual),
        ('policy.json',statistical_policy(CAMPAIGN)),('envelope-policy.json',E.policy(CAMPAIGN))]:
        need(K.same(K.read(directory/name),value),'frozen policy/schedule changed: '+name)
    proposal=K.read(directory/'proposal.json')
    need(proposal['kind']==E.VERSION and proposal['statistics_version']==P.VERSION
         and proposal['campaigns'][CAMPAIGN]['statistics']==statistical_policy(CAMPAIGN)
         and proposal['campaigns'][CAMPAIGN]['envelope']==E.policy(CAMPAIGN),'wrong preregistered policy')
    need(K.file_id(directory/'proposal.json')==K.read(directory/'proposal-pin.json'),'proposal identity differs')
    basis=K.read(directory/'runtime.json');profile=K.read(directory/'profile.json');profile_domains(profile)
    origins=K.exact(K.read(directory/'profile-origins.json'),('new','old','proposal_original_path'),'profile origins')
    need(K.same(profile,profile_from_origins(origins['new'],origins['old'],origins['proposal_original_path'])),'origin profile binding differs')
    captured_candidate_domains(profile,basis)
    verify_input_captures(directory,profile)
    need(K.same(basis['environment'],profile['environment']) and basis['platform_class']==profile['platform_class'],
         'runtime/profile environment differs')
    need(K.same(basis['tools'],tools()) and len(basis['tools'])==11,'eleven-tool closure changed')
    need(K.same(tool_inventory(directory/'tools'),basis['tools']),'frozen tool bytes differ')
    power_capability(directory)
    registration_domains(directory,profile,basis)
    labels=[label for label,path in profile['closure'].items() if path==origins['proposal_original_path']]
    need(labels and all(K.same(basis['closure'][label],{'.':K.file_id(directory/'proposal.json')}) for label in labels),
         'captured proposal differs')
    need(set(basis['binaries'])==set(P.ROLES),'runtime roles differ')
    for role in P.ROLES:
        need((directory/'bin'/role).stat().st_mode&0o777==0o755
             and K.file_id(directory/'bin'/role)==basis['binaries'][role],'frozen binary identity/mode changed')
    need(basis['binaries']['old_xoffset']==BASELINE_BINARY_ID,'old role is not V8 baseline')
    fixed_binary_domains(basis)
    s=K.exact(K.read(directory/'status.json'),('schema_version','kind','campaign','status','window_out',
        'final_wire_bytes','predicted_wire_bytes','predicted_primary_wire_bytes','predicted_retained_bytes',
        'frozen_projection_base_bytes','predicted_pool_anchor_bytes','max_seconds','resource_policy','actual_children','primary_children',
        'conditioning_children','performance_accepted'),'frozen status')
    need(type(s['schema_version']) is int and s['schema_version']==2 and s['kind']=='xoffset-four-path-conditioned-frozen'
         and s['campaign']==CAMPAIGN and s['status']=='complete' and K.absolute_string(s['window_out'])
         and s['performance_accepted'] is False,'frozen status domain differs')
    numbers={'max_seconds':MAX_SECONDS,'actual_children':len(actual),'primary_children':len(primary),'conditioning_children':len(primary)}
    need(all(K.same(s[k],v) for k,v in numbers.items()) and K.same(s['resource_policy'],E.resource_policy(CAMPAIGN)),
         'frozen budget/count differs')
    sizes=s['final_wire_bytes'];need(type(sizes) is dict and set(sizes)==set(P.ROLES)
         and all(K.integer(v,1) for v in sizes.values()),'invalid wire sizes')
    captures=K.read(directory/'input-captures.json')['captures']['normal']['roles']
    need(sizes=={role:int(value['files']['.wire']['bytes']) for role,value in captures.items()},
         'frozen modified sizes differ from sealed-input captures')
    wire=sum(sizes[j['role']] for j in actual);pwire=sum(sizes[j['role']] for j in primary)
    need(K.same(s['predicted_wire_bytes'],wire) and K.same(s['predicted_primary_wire_bytes'],pwire)
         and wire<=MAX_WIRE and pwire<=MAX_PRIMARY_WIRE,'frozen wire prediction differs')
    base=s['frozen_projection_base_bytes'];need(K.integer(base) and K.integer(s['predicted_retained_bytes']),
         'invalid projection types')
    anchors=sum(sizes.values())+(directory/'fixture/final.canonical.bin').stat().st_size+(directory/'fixture/queries.bin').stat().st_size
    need(K.same(s['predicted_pool_anchor_bytes'],anchors),'pool-anchor forecast differs')
    expected=projection(directory,actual)+base+sum(int(v['bytes']) for v in basis['tools'].values())+METADATA_RESERVE+anchors
    need(s['predicted_retained_bytes']==expected and expected<=MAX_RETAINED,'retained projection differs')
    need(regular_bytes(directory)<=base+METADATA_RESERVE,'frozen payload exceeds reserve')

def require_qualified_q():
    """F admission follows actual audited Q and its complete queue, not status alone."""
    acceptance=K.read(COORD/'qualification-acceptance.json')
    need(acceptance['status']=='passed' and acceptance['measurement_qualified'] is True
         and acceptance['complete'] is True,'once-only Q not qualified/complete')
    records={}
    for name in ('audit_report','queue_request','queue_result'):
        reference=K.exact(acceptance[name],('path','identity'),'qualification '+name)
        need(K.absolute_string(reference['path']),'relative Q evidence reference')
        K.validate_id(reference['identity'])
        path=Path(reference['path'])
        need(K.file_id(path)==reference['identity'],'Q evidence identity changed: '+name)
        records[name]=(path,K.read(path))
    report=records['audit_report'][1];request=records['queue_request'][1];result=records['queue_result'][1]
    need(report['status']=='passed' and report['campaign']=='qualification' and report['complete'] is True
         and report['measurement_qualified'] is True
         and report['successful_actual_children']==4656 and report['successful_primary_children']==2328
         and report['budget_violations']==[]
         and report['queue_result']==acceptance['queue_result']['identity'],'Q audit did not accept exact complete queue')
    analysis=report['analysis']
    need(K.same(analysis['policy'],statistical_policy('qualification'))
         and analysis['qualification_passed'] is True
         and set(analysis['AA'])==set(P.ROLES)
         and all(set(metrics)==set(P.AA_METRICS)
                 and all(test['equivalence_established'] is True for test in metrics.values())
                 for metrics in analysis['AA'].values()),'not all thirty Q AA contrasts passed')
    queue=records['queue_result'][0].parent
    need(records['queue_request'][0]==queue/'request.json'
         and K.file_id(queue/'manifest.json')==report['queue_manifest'],'Q queue manifest/reference changed')
    manifest=K.exact(K.read(queue/'manifest.json'),('schema_version','files'),'Q queue manifest')
    members={item['path']:{'bytes':str(item['bytes']),'sha256':item['sha256']}
             for item in manifest['files']}
    need(manifest['schema_version']==1 and len(members)==len(manifest['files'])
         and members==K.inventory(queue,exclude=('manifest.json',))
         and set(members)=={'request.json','result.json','events.json','supervisor.log','stdout.log','stderr.log'},
         'Q queue captured members changed')
    need(result['status']=='complete' and type(result['command_returncode']) is int and result['command_returncode']==0
         and result['kind']==request['kind']=='timing' and result['owner']==request['owner']=='W3'
         and result['cleanup']=={'live':[],'process_groups':[],'zombies':[]}
         and type(result['command_elapsed_seconds']) in (int,float)
         and math.isfinite(result['command_elapsed_seconds']) and 0<result['command_elapsed_seconds']<=2700
         and request['timeout_seconds']==2700,'Q queue failed full-window/cleanup guard')
    created=datetime.datetime.fromisoformat(request['created_at'].replace('Z','+00:00'))
    finished=datetime.datetime.fromisoformat(result['finished_at'].replace('Z','+00:00'))
    whole=(finished-created).total_seconds()
    need(created.utcoffset() is not None and finished.utcoffset() is not None
         and 0<whole<=2700 and result['command_elapsed_seconds']<=whole+.001,'Q whole wall guard failed')
    qrun=queue.parent
    need(K.file_id(qrun/'manifest.json')==report['run_manifest']
         and K.file_id(qrun/'frozen/manifest.json')==report['frozen_manifest'],'Q external manifest chain differs')
    postpin=hashlib.sha256(b'SC-POST-RUN-PIN-v1\0'+bytes.fromhex(report['run_manifest']['sha256'])+
                          bytes.fromhex(report['queue_manifest']['sha256'])).hexdigest()
    need(postpin==acceptance['postrun_pin'],'Q external composite pin changed')
    basis=K.read(qrun/'frozen/runtime.json');fixed_binary_domains(basis)
    profile=K.read(qrun/'frozen/profile.json');profile_domains(profile);captured_candidate_domains(profile,basis)
    need(K.same(basis['tools'],tools()),'Q did not qualify the current complete source/tool closure')

def freeze(out):
    gate_name='qualification-prerequisites.json' if CAMPAIGN=='qualification' else 'combined-gates.json'
    gate=COORD/gate_name;need(K.read(gate)['status']=='passed','prerequisite gates absent')
    if CAMPAIGN=='formal':
        require_qualified_q()
    budgets();primary,actual=schedules()
    new=K.read(CANDIDATE_ORIGIN_PROFILE);old=K.read(BASELINE_ORIGIN_PROFILE)
    profile=profile_from_origins(new,old);entries=profile['binaries']
    need(K.file_id(entries['old_xoffset']['path'])==BASELINE_BINARY_ID,'V8 binary changed')
    proposal=K.read(PROPOSAL)
    need(proposal['kind']==E.VERSION and proposal['statistics_version']==P.VERSION,'proposal version differs')
    basis=runtime(profile);captured_candidate_domains(profile,basis)
    fixed_binary_domains(basis)
    out=K.fresh(out)
    for name,value in [('profile.json',profile),('runtime.json',basis),('schedule.json',primary),
        ('native-schedule.json',actual),('policy.json',statistical_policy(CAMPAIGN)),('envelope-policy.json',E.policy(CAMPAIGN)),
        ('profile-origins.json',{'new':new,'old':old,'proposal_original_path':str(PROPOSAL.resolve())})]:put(out/name,value)
    copy(PROPOSAL,out/'proposal.json');put(out/'proposal-pin.json',K.file_id(PROPOSAL))
    copy(gate,out/'production-gates.json')
    for name in REGISTRATION_FILES:copy(registration_source(name),out/'registration'/name)
    if CAMPAIGN=='formal':copy(COORD/'qualification-acceptance.json',out/'qualification-acceptance.json')
    for source in tool_paths():copy(source,out/'tools'/TOOL_MEMBERS[source.name])
    fixture=Path(STAGE_BINDINGS['fixed_input_captures']['normal']['roles']['new_xoffset']['prefix']).parent.parent/'prepared/M'
    previous_fixture=BASELINE_FIXTURE
    for name in (*G.FILES,'fixture.json','queries.bin'):
        need(K.file_id(fixture/name)==K.file_id(previous_fixture/name),'model fixture changed')
        copy(fixture/name,out/'fixture'/name)
    sizes=capture_inputs(out,profile)
    for role,entry in entries.items():
        copy(entry['path'],out/'bin'/role);(out/'bin'/role).chmod(0o755)
        need(K.file_id(entry['path'])==basis['binaries'][role],'binary changed during freeze')
    base=regular_bytes(out);wire=sum(sizes[j['role']] for j in actual);pwire=sum(sizes[j['role']] for j in primary)
    side=(fixture/'final.canonical.bin').stat().st_size+(fixture/'queries.bin').stat().st_size
    anchors=sum(sizes.values())+side
    retained=wire+len(actual)*(side+NATIVE_METADATA)+base+sum(int(v['bytes']) for v in basis['tools'].values())+METADATA_RESERVE+anchors
    need(wire<=MAX_WIRE and pwire<=MAX_PRIMARY_WIRE and retained<=MAX_RETAINED,'logical projection exceeds caps')
    # Deduplication is not assumed in logical caps. Physical admission uses both
    # backing and guest actual free space and permits at most a worst child peak.
    peak=7*(max(*sizes.values(),side,1024**2)+4096)+METADATA_RESERVE
    need(base+peak<=MAX_ALLOCATED,'initial physical peak exceeds declared allocated cap')
    for path in (out,Path('/mnt/c')):need(shutil.disk_usage(path).free>=MAX_ALLOCATED+DISK_MARGIN,'initial guest/backing space unavailable')
    need(K.same(runtime(profile),basis),'runtime changed during freeze')
    put(out/'modes.json',payload_modes(out))
    K.seal(out,{'schema_version':2,'kind':'xoffset-four-path-conditioned-frozen','campaign':CAMPAIGN,'status':'complete',
        'window_out':str(B/('qualification-v1' if CAMPAIGN=='qualification' else 'formal-v1')),
        'final_wire_bytes':sizes,'predicted_wire_bytes':wire,'predicted_primary_wire_bytes':pwire,
        'predicted_retained_bytes':retained,'frozen_projection_base_bytes':base,'predicted_pool_anchor_bytes':anchors,'max_seconds':MAX_SECONDS,
        'resource_policy':E.resource_policy(CAMPAIGN),'actual_children':len(actual),'primary_children':len(primary),
        'conditioning_children':len(primary),'performance_accepted':False})
    verify_frozen(out,K.sha(out/'manifest.json'))
    print(json.dumps({'status':'frozen','campaign':CAMPAIGN,'manifest':K.file_id(out/'manifest.json'),
        'actual_children':len(actual),'predicted_wire_bytes':wire,'predicted_retained_bytes':retained}),flush=True)

def leased(out,queue,profile,frozen,pin):
    need(out.is_dir() and {p.name for p in out.iterdir()}=={'queue'} and queue==out/'queue','invalid leased output')
    request=K.read(queue/'request.json')
    expected=[str(Path(sys.executable).absolute()),'-B',str(Path(__file__).resolve()),*sys.argv[1:]]
    need(request['command']==expected,'queue must invoke exact absolute wrapper argv')
    need(request['owner']=='W3' and request['kind']=='timing' and request['lock_dir']==profile['lock_dir']
         and request['runner_sha256']==K.sha(RUNNER) and request['out']==str(queue)
         and request['cwd']==str(Path.cwd()) and request['timeout_seconds']==MAX_SECONDS,'queue request differs')
    fd=os.open(Path(profile['lock_dir'])/'queue.lock',os.O_RDONLY)
    try:
        try: fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: pass
        else:
            fcntl.flock(fd,fcntl.LOCK_UN)
            raise K.ProtocolError('global lease is not held')
        record=K.parse(os.read(fd,65536))
    finally: os.close(fd)
    need(record['status']=='running' and record['supervisor_pid']==os.getppid()
         and record['run_id']==request['run_id'] and record['owner']=='W3','not direct leased child')
    basis=K.read(frozen/'runtime.json')
    context={'schema_version':1,'action':'run','original_output':str(out),'request':request,
        'queue_request_sha256':K.sha(queue/'request.json'),'interpreter':basis['interpreter'],
        'requested_interpreter':request['command'][0],'adapter_path':str(Path(__file__).resolve()),
        'tools':tools(),'inputs':{'--frozen':{'original_path':str(frozen/'manifest.json'),
                                           'identity':K.file_id(frozen/'manifest.json')}}}
    put(out/'execution-context.json',context)

def argv_for(out,job,profile):
    role=job['role']; p=out/'commands'/job['id']; entry=profile['binaries'][role]
    argv=G.command(entry,'run','--wire',out/'frozen/input-wires'/(role+'.wire'),
        '--trace',out/'frozen/fixture/trace.bin','--wire-out',p.with_suffix('.wire'),
        '--canonical-out',p.with_suffix('.canonical'),'--queries-out',p.with_suffix('.queries'),
        '--result',p.with_suffix('.native.json'),binary=out/'frozen/bin'/role)
    return argv,p

def limit_for(frozen,role):
    status=K.read(frozen/'status.json')
    return max(status['final_wire_bytes'][role],(frozen/'fixture/final.canonical.bin').stat().st_size,
               (frozen/'fixture/queries.bin').stat().st_size,1024**2)

def run(frozen,pin,out,queue):
    start=time.monotonic();verify_frozen(frozen,pin)
    need(str(out)==K.read(frozen/'status.json')['window_out'],'output differs from frozen window')
    profile=K.read(frozen/'profile.json');before=runtime(profile)
    need(K.same(before,K.read(frozen/'runtime.json')),'runtime changed before measurement')
    leased(out,queue,profile,frozen,pin);copy_frozen(frozen,out/'frozen',pin)
    for relative in TOOL_MEMBERS.values():copy(frozen/'tools'/relative,out/'protocol'/relative)
    (out/'commands').mkdir();put(out/'frozen-pin.json',{'sha256':pin});put(out/'runtime-before.json',before)
    ledger=L.Ledger(out,E.resource_policy(CAMPAIGN),Path('/mnt/c'))
    primary,jobs=schedules();samples=[];completed=[];attempted=[];error=None;window=None
    power_state=COORD/('power-qualification-v1' if CAMPAIGN=='qualification' else 'power-formal-v1')
    sizes=K.read(frozen/'status.json')['final_wire_bytes']
    try:
        with W.PowerWindow(power_state,MAX_SECONDS+60,POWER_HELPER) as window:
          for job in jobs:
            argv,p=argv_for(out,job,profile);size=sizes[job['role']];limit=limit_for(out/'frozen',job['role'])
            timeout=G.launch_preflight(p,start+MAX_SECONDS,20,ledger.generated_wire,size,limit,MAX_WIRE)
            ledger.before_attempt(job,p,limit)
            attempted.append(job);verified=False
            try:
                receipt=G.execute(argv,p,timeout,profile['environment'],limit)
                need(p.with_suffix('.stdout').stat().st_size==p.with_suffix('.stderr').stat().st_size==0,'unexpected stdout/stderr')
                row=G.native_result(p.with_suffix('.native.json'),profile['binaries'][job['role']],out/'frozen/fixture',
                    out/'frozen/input-wires'/(job['role']+'.wire'),p.with_suffix('.wire'),p.with_suffix('.canonical'),
                    p.with_suffix('.queries'),receipt,size)
                completed.append(job)
                if job['execution_kind']=='primary':samples.append(primary_sample(primary[job['primary_index']],row))
                verified=True
            finally:ledger.record_outputs(job,p,verified=verified)
            need(time.monotonic()-start<=MAX_SECONDS,'actual window budget exceeded')
            if len(completed)%144==0:print('completed actual %d/%d primary %d/%d'%(len(completed),len(jobs),len(samples),len(primary)),flush=True)
    except Exception as caught:error=type(caught).__name__+': '+str(caught)
    need(ledger.pending is None,'active resource attempt was not preserved')
    try:after=runtime(profile)
    except (K.ProtocolError,OSError,ValueError) as caught:after={'identity_error':str(caught)}
    if not K.same(before,after):error='runtime/source/tool identity changed'
    power_files={}
    if power_state.is_dir():
        for path in power_state.iterdir():
            need(path.is_file() and not path.is_symlink(),'invalid power-state member')
            copy(path,out/'power-environment'/path.name)
        power_files=K.inventory(out/'power-environment',exclude=())
    power={'original_state_directory':str(power_state),'ready':window.ready if window else None,
        'completed':window.completed if window else None,'files':power_files}
    put(out/'power-environment.json',power)
    put(out/'runtime-after.json',after);put(out/'samples.json',samples)
    analysis=analyze(samples,CAMPAIGN);put(out/'analysis.json',analysis)
    selected=accounting(jobs,completed,attempted,primary);need(len(selected)==len(samples),'primary accounting differs')
    extras=('runtime-after.json','samples.json','analysis.json','power-environment.json',
        *('power-environment/'+name for name in power_files))
    try:storage=ledger.reconcile(extras)
    except (K.ProtocolError,OSError,ValueError) as caught:
        error=error or type(caught).__name__+': '+str(caught);storage={'status':'failed','error':str(caught),**ledger.snapshot(),'body_proofs':list(ledger.proofs)}
    put(out/'resource-ledger.json',storage)
    K.seal(out,{'schema_version':2,'kind':'xoffset-four-path-conditioned-run','campaign':CAMPAIGN,'envelope_version':E.VERSION,
        'status':'complete' if error is None and len(completed)==len(jobs) else 'failed','error':error,
        'attempted':attempted,'completed':completed,'unexecuted':jobs[len(attempted):],
        'successful_actual_children':len(completed),'successful_primary_children':len(samples),
        'successful_conditioning_children':len(completed)-len(samples),'attempted_actual_children':len(attempted),
        'unexecuted_actual_children':len(jobs)-len(attempted),
        'attempted_primary_children':sum(j['execution_kind']=='primary' for j in attempted),
        'attempted_conditioning_children':sum(j['execution_kind']=='conditioning' for j in attempted),
        'unexecuted_primary_children':sum(j['execution_kind']=='primary' for j in jobs[len(attempted):]),
        'unexecuted_conditioning_children':sum(j['execution_kind']=='conditioning' for j in jobs[len(attempted):]),
        'new_wire_bytes':ledger.generated_wire,'primary_wire_bytes':ledger.primary_wire,
        'elapsed_ns':str(int((time.monotonic()-start)*1e9)),'performance_accepted':False})
    need(error is None and len(completed)==len(jobs),'preserved structural failure: '+str(error))
    print(json.dumps({'status':'native-completed-awaiting-offline-audit','campaign':CAMPAIGN,
        'actual_children':len(completed),'primary_children':len(samples),
        'inner_measurement_qualified_pending_outer_audit':analysis['measurement_qualified']}),flush=True)

def audit(out,pin,postpin):
    K.check_inventory(out)
    actual=hashlib.sha256(b'SC-POST-RUN-PIN-v1\0'+bytes.fromhex(K.sha(out/'manifest.json'))+
                          bytes.fromhex(K.sha(out/'queue/manifest.json'))).hexdigest()
    need(actual==postpin,'external postrun composite pin differs')
    frozen=out/'frozen'; verify_frozen(frozen,pin)
    need(K.same(K.read(out/'frozen-pin.json'),{'sha256':pin}),'external frozen pin differs')
    profile=K.read(frozen/'profile.json'); queue=maintained_queue_audit(out,profile,'run'); queue_domains(out,queue)
    original=G.context_original(out)
    need(str(original)==K.read(frozen/'status.json')['window_out'],'original output differs from preregistration')
    status=K.exact(K.read(out/'status.json'),('schema_version','kind','campaign','envelope_version','status','error',
        'attempted','completed','unexecuted','successful_actual_children','successful_primary_children',
        'successful_conditioning_children','attempted_actual_children','unexecuted_actual_children',
        'attempted_primary_children','attempted_conditioning_children',
        'unexecuted_primary_children','unexecuted_conditioning_children',
        'new_wire_bytes','primary_wire_bytes','elapsed_ns','performance_accepted'),'run status')
    need(type(status['schema_version']) is int and status['schema_version']==2
         and status['kind']=='xoffset-four-path-conditioned-run' and status['campaign']==CAMPAIGN and status['envelope_version']==E.VERSION
         and status['status'] in ('complete','failed') and status['performance_accepted'] is False
         and ((status['status']=='complete' and status['error'] is None)
              or (status['status']=='failed' and type(status['error']) is str and bool(status['error']))),
         'invalid run status schema/domain')
    primary,jobs=schedules()
    selected=accounting(jobs,status['completed'],status['attempted'],primary)
    counts={'successful_actual_children':len(status['completed']),'successful_primary_children':len(selected),
        'successful_conditioning_children':len(status['completed'])-len(selected),
        'attempted_actual_children':len(status['attempted']),
        'unexecuted_actual_children':len(jobs)-len(status['attempted']),
        'attempted_primary_children':sum(j['execution_kind']=='primary' for j in status['attempted']),
        'attempted_conditioning_children':sum(j['execution_kind']=='conditioning' for j in status['attempted']),
        'unexecuted_primary_children':sum(j['execution_kind']=='primary' for j in jobs[len(status['attempted']):]),
        'unexecuted_conditioning_children':sum(j['execution_kind']=='conditioning' for j in jobs[len(status['attempted']):])}
    need(all(K.same(status[k],v) for k,v in counts.items())
         and K.same(status['unexecuted'],jobs[len(status['attempted']):]),'actual/primary accounting differs')
    need(K.integer(status['new_wire_bytes']) and K.integer(status['primary_wire_bytes']),
         'wire counters must be exact nonnegative integers')
    samples=K.read(out/'samples.json')
    expected_samples=[primary_sample(j,K.read((out/'commands'/j['id']).with_suffix('.native.json')))
                      for j in selected]
    need(K.same(samples,expected_samples),'samples include conditioners or differ from exact primary/native projection')
    previous=0; first_start=None
    expected_receipts={j['id']+'.receipt.json' for j in status['attempted']}
    need({p.name for p in (out/'commands').glob('*.receipt.json')}==expected_receipts,
         'attempted receipt inventory differs')
    allowed={j['id']+suffix for j in status['attempted'] for suffix in
             ('.receipt.json','.stdout','.stderr','.wire','.canonical','.queries','.native.json')}
    need(set(K.inventory(out/'commands',exclude=())).issubset(allowed),'unexpected native output member')
    for index,job in enumerate(status['attempted']):
        argv,_=argv_for(original,job,profile)
        p=out/'commands'/job['id']; limit=limit_for(frozen,job['role'])
        receipt,previous=G._receipt(out,job['id'],argv,profile['environment'],previous,20,limit)
        if first_start is None: first_start=int(receipt['started_monotonic_ns'])
        success=receipt['returncode']==0 and receipt['timed_out'] is False and receipt['launch_error'] is None
        expected=K.read(frozen/'status.json')['final_wire_bytes'][job['role']]
        def verify_body():
            need(p.with_suffix('.stdout').stat().st_size==p.with_suffix('.stderr').stat().st_size==0,
                 'unexpected native stdout/stderr')
            native=G.native_result(p.with_suffix('.native.json'),profile['binaries'][job['role']],frozen/'fixture',
                frozen/'input-wires'/(job['role']+'.wire'),p.with_suffix('.wire'),p.with_suffix('.canonical'),
                p.with_suffix('.queries'),receipt,expected)
            return native
        if index<len(status['completed']):
            need(success,'successful actual child has failed receipt')
            verify_body()  # Includes every conditioner, not only primary inference.
        else:
            need(index==len(status['attempted'])-1 and status['status']=='failed',
                 'unrecorded child is not final failed attempt')
            invalid=not success
            if success:
                try: verify_body()
                except (K.ProtocolError,OSError,ValueError): invalid=True
            need(invalid,'last valid actual child omitted from completed ledger')
    total_wire,primary_wire=wire_usage(out,jobs)
    need(total_wire==status['new_wire_bytes'] and primary_wire==status['primary_wire_bytes'],
         'actual/primary wire accounting differs including partial outputs')
    # Exceeding a hard cap is a preserved structural failure, never successful qualification.
    storage,budget_violations=audit_storage(out,status)
    power_audit=audit_power(out,status,K.read(frozen/'runtime.json'))
    if total_wire>MAX_WIRE: budget_violations.append('outer_wire_bytes')
    if primary_wire>MAX_PRIMARY_WIRE: budget_violations.append('primary_wire_bytes')
    retained=regular_bytes(out)
    if retained>MAX_RETAINED: budget_violations.append('retained_output_bytes')
    need(not budget_violations or status['status']=='failed','complete run exceeds actual byte budget')
    before,after=K.read(out/'runtime-before.json'),K.read(out/'runtime-after.json')
    need(K.same(before,K.read(frozen/'runtime.json')),'initial captured runtime differs')
    need(K.same(before,after) or (status['status']=='failed' and status['error']=='runtime/source/tool identity changed'),
         'unreported captured runtime change')
    elapsed=K.decimal(status['elapsed_ns'])
    need(elapsed<=queue['command_elapsed_seconds']*10**9+10**6
         and (first_start is None or previous-first_start<=elapsed+10**6)
         and (status['status']=='failed' or elapsed<=MAX_SECONDS*10**9),'receipt/run/queue elapsed budgets differ')
    analysis=analyze(samples,CAMPAIGN)
    need(K.same(analysis,K.read(out/'analysis.json')),'fixed primary analysis differs from recomputation')
    complete=(status['status']=='complete' and len(status['completed'])==len(jobs) and len(samples)==len(primary)
              and status['error'] is None and not budget_violations and K.same(before,after))
    need((queue['status']=='complete')==complete,'queue/outer run completion differs')
    need(not complete or queue['command_returncode']==0,'complete queue has nonzero command returncode')
    qualified=complete and analysis['measurement_qualified']
    return {'schema_version':2,'kind':'xoffset-four-path-conditioned-offline-audit','campaign':CAMPAIGN,'status':'passed',
        'envelope_version':E.VERSION,'complete':complete,**counts,'power_window':power_audit,
        'resource_ledger':K.file_id(out/'resource-ledger.json'),
        'new_wire_bytes':total_wire,'primary_wire_bytes':primary_wire,'retained_output_bytes':retained,
        'retained_scope':'All current regular files including copied frozen/protocol, manifests and queue.',
        'budget_violations':budget_violations,'analysis':analysis,
        'measurement_qualified':qualified,'target_achieved':complete and analysis['target_achieved'],
        'primary_targets':analysis['primary_targets'] if qualified else None,
        'auxiliary_diagnostics':analysis['auxiliary_diagnostics'] if qualified else None,
        'promotion_eligible':complete and analysis['promotion_eligible'],
        'confirmed_cost_transfer':analysis['confirmed_cost_transfer'],
        'frozen_manifest':K.file_id(frozen/'manifest.json'),'run_manifest':K.file_id(out/'manifest.json'),
        'queue_manifest':K.file_id(out/'queue/manifest.json'),'queue_result':K.file_id(out/'queue/result.json'),
        'scope':'Every completed actual native child (conditioning and primary), receipts and body bytes audited. Only primary-cohort full phase vectors enter the four-path phase statistics; Q has 30 AA tests only. Audit correctness, qualification and target are separate.'}

def main():
    parser=argparse.ArgumentParser();subs=parser.add_subparsers(dest='action',required=True)
    for action in ('freeze','run','audit'):
        p=subs.add_parser(action);p.add_argument('--campaign',choices=('qualification','formal'),required=True)
        p.add_argument('--out',type=Path,required=True)
        if action!='freeze':p.add_argument('--frozen-sha256',required=True)
        if action=='run':
            p.add_argument('--frozen',type=Path,required=True);p.add_argument('--queue',type=Path,required=True)
        if action=='audit':
            p.add_argument('--postrun-pin',required=True);p.add_argument('--report',type=Path,required=True)
    args=parser.parse_args();configure(args.campaign)
    if args.action=='freeze':freeze(args.out)
    elif args.action=='run':run(args.frozen,args.frozen_sha256,args.out,args.queue)
    else:
        context=K.read(args.out/'execution-context.json')
        protected=[args.out,Path(context['original_output']),Path(context['inputs']['--frozen']['original_path']).parent]
        need(not any(args.report.resolve().is_relative_to(p.resolve()) for p in protected),
             'audit report must be outside immutable current/original run and original frozen bundle')
        result=audit(args.out,args.frozen_sha256,args.postrun_pin);put(args.report,result)
        print(json.dumps({'status':'audit-passed','campaign':CAMPAIGN,'complete':result['complete']}),flush=True)

def configure(kind):
    """Choose the pinned closure; no adjacent pyc or original path is used offline."""
    global CAMPAIGN,G,K,P,E,L,W,NATIVE,STATS,T_SOURCE,ENVELOPE,RESOURCES,POWER_MODULE,POWER_HELPER,PROPOSAL,RUNNER
    global MAX_SECONDS,MAX_WIRE,MAX_PRIMARY_WIRE,MAX_RETAINED,MAX_ALLOCATED,METADATA_RESERVE,DISK_MARGIN
    if kind not in ('qualification','formal') or type(kind) is not str:
        raise ValueError('invalid campaign kind')
    if STAGE_BINDINGS is None:
        raise ValueError('root actual baseline/candidate/tool admission is not bound')
    if STAGE_BINDINGS['fixed_input_captures'] is None:
        raise ValueError('authoring source: normal/SAN six-role sealed-input captures remain unbound; no freeze/run/audit admission')
    if CAMPAIGN is not None:
        if CAMPAIGN!=kind:raise ValueError('one campaign per process')
        return
    CAMPAIGN=kind
    archived=(HERE/'campaign.py').exists() or (HERE.parent/'runtime.json').exists()
    if archived:
        expected=set(TOOL_MEMBERS.values());directories=set()
        for relative in expected:
            directories.update(p.as_posix() for p in Path(relative).parents if p!=Path('.'))
        files=set();actual_directories=set()
        for path in HERE.rglob('*'):
            mode=path.lstat().st_mode;relative=path.relative_to(HERE).as_posix()
            if stat.S_ISREG(mode):files.add(relative)
            elif stat.S_ISDIR(mode):actual_directories.add(relative)
            else:raise ValueError('archived tools contain a link/special member')
        if files!=expected or actual_directories!=directories:
            raise ValueError('archived tools must be exact eleven regular source members, no bytecode')
        runtime_basis=json.loads((HERE.parent/'runtime.json').read_text(encoding='utf-8'))
        for name,relative in TOOL_MEMBERS.items():
            data=(HERE/relative).read_bytes();fid={'bytes':str(len(data)),'sha256':hashlib.sha256(data).hexdigest()}
            if runtime_basis['tools'].get(name)!=fid:raise ValueError('tool changed before source import: '+name)
    NATIVE=HERE if archived else HERE/'native_helpers'
    # Compile the bytes being identity-captured, never an adjacent cached pyc or
    # a previously imported generic core/protocol module. Relative imports bind
    # the three maintained helpers inside a private package.
    package='_sc_four_path_native_'+kind
    pkg=types.ModuleType(package);pkg.__path__=[str(NATIVE)];sys.modules[package]=pkg
    def module(name,path,package_name=None):
        result=types.ModuleType(name);result.__file__=str(path);result.__package__=package_name
        sys.modules[name]=result;exec(compile(path.read_bytes(),str(path),'exec'),result.__dict__);return result
    K=module(package+'.core',NATIVE/'core.py',package)
    module(package+'.protocol',NATIVE/'protocol.py',package)
    G=module(package+'.campaign',NATIVE/'campaign.py',package)
    measurement=HERE/'performance-v5/measurement'
    STATS=(HERE/TOOL_MEMBERS['phase_protocol.py'] if archived else
        HERE/'four-path-plan-support-20261018/statistics/phase_protocol.py')
    T_SOURCE=HERE/TOOL_MEMBERS['comparison_protocol.py'] if archived else measurement/'comparison_protocol.py'
    ENVELOPE=HERE/'conditioning_envelope.py'
    RESOURCES=HERE/'runtime_resources.py'
    POWER_MODULE=HERE/'power_environment.py'
    POWER_HELPER=HERE/'power-control.ps1'
    P=module('_four_path_phase_statistics',STATS);E=module('_four_path_conditioning_envelope',ENVELOPE)
    L=module('_four_path_runtime_resources',RESOURCES)
    W=module('_four_path_power_environment',POWER_MODULE)
    need(P.FROZEN_PATH==T_SOURCE and K.file_id(T_SOURCE)==
         {'bytes':str(P.FROZEN_BYTES),'sha256':P.FROZEN_SHA256},'fixed t-source differs')
    need(E.VERSION=='sc-four-path-conditioned-lifecycle-envelope-v3'
         and E.policy('qualification')['qualification_X_role_binaries_identical'] is False,
         'obsolete identical-X qualification envelope is forbidden')
    bind_stage(kind)
    PROPOSAL=HERE/'protocol-proposal-v1.json'
    RUNNER=HERE/'run_job.py' if archived else LIVE_RUNNER
    e=E.policy(kind);MAX_SECONDS=e['maximum_window_seconds'];MAX_WIRE=e['maximum_wire_bytes']
    MAX_PRIMARY_WIRE=e['maximum_primary_wire_bytes'];MAX_RETAINED=e['maximum_logical_retained_bytes']
    MAX_ALLOCATED=e['maximum_allocated_output_bytes'];METADATA_RESERVE=e['metadata_reserve_bytes']
    DISK_MARGIN=e['disk_margin_bytes']

def power_paths():
    return {'powershell_executable':Path('/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'),
            'powercfg_executable':Path('/mnt/c/Windows/System32/powercfg.exe')}

def power_capability(directory):
    cap=K.read(directory/'registration/power-capability-v3.json')
    need(cap['status']=='passed' and cap['native_benchmark_executions']==0,'power capability missing')
    for name,path in [('module',POWER_MODULE),('helper',POWER_HELPER)]:
        need(K.file_id(path)=={'bytes':str(cap[name]['bytes']),'sha256':cap[name]['sha256']},'power capability/tool identity differs')

def registration_source(name):
    return RESOURCE_TEST_RECEIPT if name=='resources-pure-tests-v3.json' else COORD/name

def registration_domains(directory,profile,basis):
    proposal=K.read(directory/'proposal.json')
    origins=K.read(directory/'profile-origins.json')
    original_proposal=Path(origins['proposal_original_path'])
    # Fixed coordinator reference paths are provenance strings only offline.
    for name in REGISTRATION_FILES:
        fid=K.file_id(directory/'registration'/name)
        need(K.same(fid,proposal['coordinator_references'][name]),'coordinator reference changed')
    decision=K.read(directory/'registration/measurement-decision-v2.json')
    previous=K.read(directory/'registration/measurement-decision-v1.json')
    need(all(K.same(decision[k],previous[k]) for k in ('qualification','formal','common')),
         'v2 registration changed previously fixed policy')
    d=decision[CAMPAIGN];p=P.policy(CAMPAIGN);e=E.policy(CAMPAIGN)
    values={'macrocycles':p['macrocycles'],'actual_children':e['maximum_actual_children'],
        'primary':p['maximum_children'],'conditioning':p['maximum_children'],'primary_warmup':24,
        'inference_primary':p['measured_children'],'maximum_seconds':MAX_SECONDS,
        **{k:e[k] for k in ('maximum_wire_bytes','maximum_primary_wire_bytes',
            'maximum_logical_retained_bytes','maximum_allocated_output_bytes')}}
    need(all(K.same(d[k],v) for k,v in values.items()) and d['all_30_AA_equivalent_required'] is True,
         'coordinator fixed budget differs')
    need(decision['common']['AA_equivalence_factor']==1.05
         and decision['common']['AA_contrasts']==30 and decision['common']['AA_tails']==60
         and decision['common']['primary_contrasts']==12
         and decision['common']['qualification_retries']==0 and decision['common']['formal_retries']==0
         and decision['common']['formal_windows_limit']==1,'coordinator thresholds/retries differ')
    if CAMPAIGN=='qualification':need(d['cross_library_contrasts_computed'] is False,'Q contrasts forbidden')
    else:need(K.same(d['primary_thresholds'],P.PRIMARY_THRESHOLDS),'formal phase targets differ')
    def norm(fid):return {'bytes':str(fid['bytes']),'sha256':fid['sha256']}
    backing=K.read(directory/'registration/backing-volume-verification-v1.json')
    need(backing['status']=='passed' and norm(backing['windows_snapshot'])==
         K.file_id(directory/'registration/backing-volume-windows-v1.json'),'backing registration differs')
    need(norm(decision['backing_verification'])==K.file_id(directory/'registration/backing-volume-verification-v1.json')
         and norm(decision['power_capability'])==K.file_id(directory/'registration/power-capability-v3.json')
         and norm(decision['resources_pure_tests'])==K.file_id(directory/'registration/resources-pure-tests-v3.json'),
         'decision capability references differ')
    resource=K.read(directory/'registration/resources-pure-tests-v3.json')
    need(resource['status']=='passed' and norm(resource['module'])==basis['tools']['runtime_resources.py']
         and resource['native_children']==0 and resource['checks']==['normal','optimized']
         and type(resource['test_cases_each_mode']) is int and resource['test_cases_each_mode']==26,
         'actual resource helper/pure-test receipt differs')
    cap=K.read(directory/'registration/power-capability-v3.json')
    need(norm(cap['previous_capability'])==K.file_id(directory/'registration/power-capability-v2.json'),
         'previous power capability chain differs')

def queue_timeout(value):
    need(type(value) in (int,float) and value==MAX_SECONDS and math.isfinite(value),
         'queue window budget domain differs')

def audit_storage(out,status):
    """Portable logical census and original-runtime allocation proof checks.

    Original dev/ino/block records describe the executed location. Relocation may
    materialize hardlinks as separate files; it never substitutes old host paths.
    Every body is still checked by maintained native_result and full inventory.
    """
    storage=K.read(out/'resource-ledger.json');policy=E.resource_policy(CAMPAIGN)
    need(K.same(storage['policy'],policy),'resource policy differs')
    need(K.same(storage['generated_wire_bytes'],status['new_wire_bytes'])
         and K.same(storage['generated_primary_wire_bytes'],status['primary_wire_bytes']),'storage/generated counters differ')
    need(K.integer(storage['allocated_output_bytes']) and K.integer(storage['logical_retained_bytes'])
         and K.same(storage['recorded_attempts'],len(status['attempted'])),'storage counter/attempt domain differs')
    proofs=storage['body_proofs'];completed=status['completed']
    need(type(proofs) is list and len(proofs)<=3*len(completed)
         and (status['status']=='failed' or len(proofs)==3*len(completed)),'pool proof count differs')
    expected={j['id']+s for j in completed for s in ('.wire','.canonical','.queries')}
    ordered=['commands/'+j['id']+s for j in completed for s in ('.wire','.canonical','.queries')]
    need([p['path'] for p in proofs]==ordered[:len(proofs)],'pool proofs are not the actual completed body prefix')
    seen=set();anchors={}
    for proof in proofs:
        path=K.relative(proof['path']).as_posix();need(path.startswith('commands/') and path[len('commands/'):] in expected,'unexpected proof path')
        need(path not in seen,'duplicate body proof');seen.add(path)
        identity=K.file_id(out/path);need(identity=={'bytes':str(proof['body_identity']['bytes']),'sha256':proof['body_identity']['sha256']},'proof body identity differs')
        anchor='verified-body-pool/'+identity['bytes']+'-'+identity['sha256']
        need(proof['anchor']==anchor and K.file_id(out/anchor)==identity,'pool anchor identity differs')
        fresh=anchor not in anchors;anchors[anchor]=identity
        need(proof['first_verified_occurrence'] is fresh and proof['full_semantic_validation_preceded_pooling'] is True
             and proof['exact_byte_comparison_to_existing_anchor'] is (not fresh) and proof['bytes_preserved'] is True,'invalid pool provenance')
        before=K.exact(proof['original_native_file'],('dev','ino','bytes','mtime_ns','mode','blocks_bytes','links'),'native slot state')
        need(all(K.integer(before[k]) for k in before) and before['bytes']==int(identity['bytes'])
             and type(before['links']) is int and before['links']==1 and type(before['mode']) is int and before['mode']&0o222,
             'original body was not a fresh regular writable slot')
    need({p.relative_to(out).as_posix() for p in (out/'verified-body-pool').iterdir()}==set(anchors),'pool membership differs')
    need(storage['deduplicated_body_slots']==len(proofs) and storage['body_anchors']==len(anchors),'pool counters differ')
    violations=[]
    if status['new_wire_bytes']>MAX_WIRE:violations.append('outer_wire_bytes')
    if status['primary_wire_bytes']>MAX_PRIMARY_WIRE:violations.append('primary_wire_bytes')
    if regular_bytes(out)>MAX_RETAINED:violations.append('logical_retained_bytes')
    if storage['allocated_output_bytes']+METADATA_RESERVE>MAX_ALLOCATED:violations.append('original_allocated_output_bytes')
    if storage['status']!='passed':violations.append('storage_reconciliation_failed')
    need(not violations or status['status']=='failed','complete run exceeds storage contract')
    return storage,violations

def audit_power(out,status,basis):
    record=K.read(out/'power-environment.json')
    need(record['original_state_directory']==str(COORD/('power-qualification-v1' if CAMPAIGN=='qualification' else 'power-formal-v1')),
         'power-state provenance path differs')
    need(K.same(record['files'],K.inventory(out/'power-environment',exclude=())),'power-state copied inventory differs')
    required={'ready.json','completed.json','request.json'}
    if not required<=set(record['files']):
        need(status['status']=='failed','complete run lacks power receipts')
        return {'restored':False,'valid_complete_power_window':False,'incomplete_failed_power_setup_preserved':True,
                'physical_core_exclusive_or_frequency_locked_claimed':False}
    ready=K.read(out/'power-environment/ready.json');completed=K.read(out/'power-environment/completed.json')
    request=K.read(out/'power-environment/request.json')
    def normalized(fid):return {'bytes':str(fid['bytes']),'sha256':fid['sha256']}
    need(normalized(request['module'])==basis['tools']['power_environment.py']
         and normalized(request['helper'])==basis['tools']['power-control.ps1'],'actual power tool differs')
    need(all(normalized(request[k])==basis['power_executables'][k] for k in power_paths()),'actual power host executable differs')
    need(type(request['maximum_seconds']) is int and request['maximum_seconds']==MAX_SECONDS+60
         and request['background_window_style']=='Hidden','power watchdog bound/mode differs')
    valid=(ready.get('status')=='ready' and ready.get('active')=='8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'
           and type(ready.get('maximum_seconds')) is int and ready['maximum_seconds']==MAX_SECONDS+60
           and completed.get('status')=='passed' and completed.get('restored') is True
           and completed.get('reason')=='requested_stop' and completed.get('original')==ready.get('original')
           and K.same(record['ready'],ready) and K.same(record['completed'],completed))
    need(valid or status['status']=='failed','complete run did not establish temporary power and restoration')
    return {'restored':completed.get('restored') is True,'valid_complete_power_window':valid,
            'physical_core_exclusive_or_frequency_locked_claimed':False}

def maintained_queue_audit(out, profile, action):
    out = Path(out)
    context = K.read(out/"execution-context.json")
    queue = out/"queue"
    qmanifest = K.read(queue/"manifest.json")
    actual = K.inventory(queue,exclude=("manifest.json",))
    # The maintained runner uses integer byte lengths; validate and normalize explicitly.
    qfiles = {r["path"]:dict(bytes=str(r["bytes"]),sha256=r["sha256"]) for r in qmanifest["files"]}
    K.require(qfiles == actual and len(qfiles) == len(qmanifest["files"])
              and set(qfiles) == {"request.json","result.json","events.json","supervisor.log","stdout.log","stderr.log"},"queue members changed")
    request,result = K.read(queue/"request.json"),K.read(queue/"result.json")
    K.require(context["request"] == request and context["queue_request_sha256"] == K.sha(queue/"request.json")
              and context["action"] == action and request["lock_dir"] == profile["lock_dir"]
              and request["runner_sha256"] == K.sha(out/"protocol/run_job.py")
              and request["kind"] == ("timing" if action == "run" else "functional")
              and request["owner"] == result["owner"] == "W3" and request["run_id"] == result["run_id"],"queue/context identity differs")
    basis=K.read(out/("frozen/runtime.json" if action=="run" else "runtime-before.json"))
    K.require(context["tools"] == basis["tools"] and context["interpreter"] == basis["interpreter"]
              and context["tools"] == tool_inventory(out/"protocol"),"queue interpreter/tool bytes differ from frozen basis")
    K.require(result["status"] in ("complete","failed","timeout","cancelled","error") and result.get("cleanup") == {"live":[],"process_groups":[],"zombies":[]},"queue cleanup/status invalid")
    maximum=MAX_SECONDS if action=="run" else 3600
    elapsed=result.get("command_elapsed_seconds")
    K.require(type(elapsed) in (int,float) and elapsed>0,"invalid queue elapsed")
    try:
        created=datetime.datetime.fromisoformat(request["created_at"].replace("Z","+00:00"))
        finished=datetime.datetime.fromisoformat(result["finished_at"].replace("Z","+00:00"))
        K.require(created.utcoffset() is not None and finished.utcoffset() is not None,"queue timestamps require timezone")
        whole=(finished-created).total_seconds()
    except (ValueError,TypeError,AttributeError) as error:
        raise K.ProtocolError("invalid queue created/finished timestamps") from error
    K.require(whole>0 and elapsed<=whole+0.001,"queue timestamps/command elapsed differ")
    if result["status"]=="complete":
        K.require(elapsed<=maximum and whole<=maximum,"complete queue exceeds strict full-window budget")
    else:
        grace,wait=request.get("termination_grace_seconds",2),request.get("wait_seconds",0)
        K.require(type(grace) in (int,float) and grace>0 and type(wait) in (int,float) and wait>=0,"invalid failed queue cleanup/wait allowance")
        K.require(elapsed<=maximum+grace+1 and whole<=maximum+wait+grace+1,"failed queue exceeds declared cleanup allowance")
    command_argv = request["command"]
    expected_head = [context["requested_interpreter"],context["adapter_path"]]
    if len(command_argv)>1 and command_argv[1] == "-B":
        command_argv = [command_argv[0],*command_argv[2:]]
    K.validate_id(context["interpreter"]["identity"])
    K.require(command_argv[:2] == expected_head and K.absolute_string(expected_head[0]) and K.absolute_string(expected_head[1])
              and command_argv[2] == action,"queue interpreter/adapter/action differs")
    args = command_argv[3:]
    K.require(len(args)%2 == 0,"odd queue argv")
    flags = dict(zip(args[::2],args[1::2]))
    expected_flags = {"--campaign","--frozen","--frozen-sha256","--out","--queue"} if action == "run" else {"--prepared","--profile","--out","--queue"}
    K.require(set(flags) == expected_flags and len(flags)*2 == len(args) and flags["--out"] == context["original_output"]
              and flags["--queue"] == request["out"] == context["original_output"]+"/queue","queue original output differs")
    for flag,binding in context["inputs"].items():
        original = flags.get(flag)
        K.require(K.absolute_string(original) and K.absolute_string(binding["original_path"]),"relative input path")
        suffix = "/manifest.json" if flag in ("--frozen","--prepared") else ""
        K.require(original+suffix == binding["original_path"],"queue original input differs")
        archived = out/({"--frozen":"frozen/manifest.json","--prepared":"prepared/manifest.json","--profile":"profile.json"}[flag])
        K.require(K.file_id(archived) == binding["identity"],"queue input identity differs")
    K.require(flags.get("--campaign")==CAMPAIGN,"queue campaign differs")
    if action == "run":
        K.require(flags["--frozen-sha256"] == K.read(out/"frozen-pin.json")["sha256"],"external argv pin differs")
    return result

if __name__=='__main__':main()
