import json,sys,statistics as st,collections,math
rows=[json.loads(l) for l in open(sys.argv[1])]
ref=sys.argv[2] if len(sys.argv)>2 else 'xoffset_r1m2'
by=collections.defaultdict(lambda: collections.defaultdict(list))
for r in rows:
    p=r['phase_ns']; d=dict(p); d['read_sum']=p['query_before']+p['query_after']; d['faults']=r['minor_faults']
    for k,v in d.items(): by[r['lib']][k].append(v)
ph=['load','query_before','mutate','emit_after','retire_modified_state','reload','query_after','cleanup','read_sum','faults']
print('median ms (faults count)'); print('%-14s'%'lib'+''.join('%11s'%x[:10] for x in ph))
for lib in by:
    print('%-14s'%lib+''.join('%11.3f'%(st.median(by[lib][k])/1e6) if k!='faults' else '%11d'%st.median(by[lib][k]) for k in ph))
print(f'\nratio of medians {ref} / competitor (targets read<=1.05 mutate<=1.05 emit<=0.20)')
for lib in ['cista','flatbuffers','capnproto','cereal']:
    if lib in by: print('%-12s read %.3f  mutate %.3f  emit %.3f  reload %.3f'%(lib,*(st.median(by[ref][k])/st.median(by[lib][k]) for k in ['read_sum','mutate','emit_after','reload'])))
if 'xoffset_c14' in by and ref!='xoffset_c14':
    print('%s/c14: read %.3f mutate %.3f emit %.3f reload %.3f'%(ref,*(st.median(by[ref][k])/st.median(by['xoffset_c14'][k]) for k in ['read_sum','mutate','emit_after','reload'])))
