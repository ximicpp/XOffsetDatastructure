"""Four-path Q32 / F64 envelope: V8 control and actual final candidate in both.

Derived from sealed V5 v2; schedule, counts, wall/byte/resource bounds unchanged.
Only the version and the obsolete identical-X qualification assertion change.
No child execution or clock access.
"""
VERSION = 'sc-four-path-conditioned-lifecycle-envelope-v3'
KINDS = ('qualification', 'formal')
ROLES = ('old_xoffset','new_xoffset','cista','flatbuffers','capnproto','cereal')

def policy(kind):
    if type(kind) is not str or kind not in KINDS:
        raise ValueError('invalid campaign kind')
    q = kind == 'qualification'
    n = 4656 if q else 9264
    return {'version':VERSION,'campaign':kind,'maximum_actual_children':n,
        'required_primary_children':n//2,'required_conditioning_children':n//2,
        'maximum_child_seconds':20,'maximum_window_seconds':2700 if q else 5400,
        'maximum_wire_bytes':(128 if q else 256)*1024**3,
        'maximum_primary_wire_bytes':(64 if q else 128)*1024**3,
        'maximum_logical_retained_bytes':(256 if q else 512)*1024**3,
        'maximum_allocated_output_bytes':(4 if q else 8)*1024**3,
        'metadata_reserve_bytes':n*65536+64*1024**2,'disk_margin_bytes':2*1024**3,
        'backing_volume_path':'/mnt/c','cpu_affinity_virtual_cpu':2,
        'maximum_windows':1,'retries':0,'conditioners_in_inference':False,
        'read_conditioner_time_for_decisions':False,
        'verified_body_deduplication':'sha256-size-and-full-byte-compare-then-atomic-hardlink',
        'logical_generated_and_apparent_bytes_are_not_reduced_by_deduplication':True,
        'qualification_X_role_binaries_identical':False,
        'interpretation':'conditioned fresh-process M trusted detached full lifecycle; not cold start'}

def resource_policy(kind):
    p = policy(kind)
    return {k:p[k] for k in ('maximum_wire_bytes','maximum_primary_wire_bytes',
        'maximum_logical_retained_bytes','maximum_allocated_output_bytes',
        'metadata_reserve_bytes','disk_margin_bytes')}

def schedule(primary, kind):
    p = policy(kind)
    if type(primary) is not list or len(primary) != p['required_primary_children']:
        raise ValueError('need exact fixed primary schedule')
    result=[]
    for i,j in enumerate(primary):
        if type(j) is not dict or j.get('id') != 'j%05d'%i or j.get('role') not in ROLES:
            raise ValueError('invalid primary serial ID/role')
        for execution_kind, identity in (('conditioning','c%05d'%i),('primary',j['id'])):
            result.append({'id':identity,'role':j['role'],'execution_kind':execution_kind,
                'primary_index':i,'primary_id':j['id']})
    return result

def counts(completed, kind):
    if type(completed) is not int or not 0<=completed<=policy(kind)['maximum_actual_children']:
        raise ValueError('invalid completed count')
    return {'primary':completed//2,'conditioning':(completed+1)//2}
