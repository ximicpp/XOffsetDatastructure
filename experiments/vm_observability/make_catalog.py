"""Maintainer helper to render the fully specified v1 event-kind catalog."""
import json
from pathlib import Path
k={}
def add(name,owner_identity,subject_identity,byte_meaning,value_meaning,stage='completed',accumulate='count and bytes; value only as documented',owner='any',subject='any',**extra):
    k[name]={'owner':owner,'subject':subject,'owner_identity':owner_identity,'subject_identity':subject_identity,'bytes':{'type':'uint64','unit':byte_meaning},'value':{'type':'uint64','unit':value_meaning},'stage':stage,'aggregation':accumulate,**extra}
for name in ('host_allocate','host_deallocate','host_allocate_failed'):
    add(name,'causal arena base from Scope, or 0; shared domains are not exclusive','backend allocation pointer; failure uses 0','requested backend bytes (n*sizeof(rebound T))','domain enum 1..5',
        stage='completed' if name!='host_allocate_failed' else 'attempt failed before publish',accumulate='sum calls and requested bytes separately for alloc/free/failure; ledger keyed subject; domain is not additive',subject='zero' if name=='host_allocate_failed' else 'nonzero',value_min=1,value_max=5)
for name in ('registry_register','registry_register_failed','registry_erase','registry_rebind','registry_lookup_hit','registry_lookup_miss'):
    add(name,'C++ XBufferCore object pointer; lookup miss=0','arena base key','0','erase=1, otherwise 0',stage='failed attempt' if name.endswith(('failed','miss')) else 'completed',accumulate='count only; reconstruct per-base generations from successful register sequence through erase and rebind',owner='zero' if name=='registry_lookup_miss' else 'nonzero',subject='nonzero',bytes_equals=0,value_equals=1 if name=='registry_erase' else 0)
for name in ('registry_lock','pool_lock'):
    add(name,'causal arena Scope if present else 0; shared lock is not exclusive owner cost','mutex address','wait nanoseconds','hold nanoseconds',accumulate='sum per-thread waits/holds; may exceed wall time; excludes record/unlock overhead',subject='nonzero')
for name in ('core_scan','wire_scan'):
    add(name,'arena base','visited donor address','0','0',stage='eligibility predicate visit',accumulate='count only; no claim of all allocator comparisons',owner='nonzero',subject='nonzero',bytes_equals=0,value_equals=0)
add('pool_scan','causal Scope or 0','cached mapping base','exact maximum capacity bytes','0',stage='exact-match predicate visit',accumulate='count scans; capacities may be summed as visited capacity only',subject='nonzero',value_equals=0)
entries=[
 ('core_new','new user address','requested user bytes','new block end offset','completed'),
 ('core_reuse','whole donor user address','physical donor user bytes','requested user bytes','donor selected before erase'),
 ('core_return','returned user address','physical total bytes incl AllocationHeader','new Core free-block count','completed'),
 ('core_return_failed','returned user address','physical total bytes','0','metadata allocation rejected; reclaim abandoned'),
 ('wire_reuse','wire donor address','cached block bytes','aligned request bytes','completed'),
 ('wire_return','returned block address','aligned cached bytes','new wire free bytes','completed'),
 ('wire_transfer_attempt','block address','wire block bytes','0','before Core deallocate; not necessarily successful reclaim'),
 ('extent_request','0','required end offset bytes','old logical bytes','growth decision attempt'),
 ('auto_growth','0','new logical bytes','old logical bytes','completed'),
 ('expand_check','live user address','requested new bytes','old physical user bytes','validation attempt'),
 ('expand_existing','live user address','requested new bytes','existing physical bytes','successful no enlargement'),
 ('expand_capacity_failure','live user address','requested new bytes','existing physical bytes','failed capacity attempt'),
 ('expand_success','live user address','requested new bytes','old physical bytes','pre-publication after extent success'),
 ('cache_flush','0','wire free bytes at entry','wire epoch at entry','attempt, transfers counted separately'),
 ('vector_relocation_attempt','fresh user address','planned logical element extent','new capacity bytes','attempt before relocation loop'),
 ('vector_relocated_element','completed destination element','sizeof(T) logical extent','0','completed relocation step; not recursive physical copy volume')]
for name,sub,b,v,stage in entries:
    add(name,'arena base',sub,b,v,stage=stage,accumulate='count; sum bytes only as event-named extents (do not combine related steps as copies)',owner='nonzero',subject='zero' if sub=='0' else 'nonzero',**({'value_equals':0} if v=='0' else {}))
add('core_erase_movement_estimate','arena base','0','following FreeBlock records*sizeof(FreeBlock)','0',stage='calculated before vector erase',accumulate='estimated logical movement bytes only; not libc/machine copy instructions',owner='nonzero',subject='zero',value_equals=0)
add('core_metadata_capacity','arena base, or 0 for moved-from empty owner','C++ owner object pointer','vector capacity*sizeof(FreeBlock)','sizeof(free_blocks_) fixed object bytes',stage='snapshot after register/free/reuse/move/swap',accumulate='sampled max single owner dynamic capacity; not sum of snapshots; ledger provides observed concurrent requested totals',subject='nonzero')
for name in ('string_relocate_copy','string_assign_copy','vector_insert_shift','vector_insert_staged','vector_erase_shift','save_header_copy','save_payload_copy','load_header_copy','load_payload_copy'):
    add(name,'arena base; load_header_copy=0 before admission','destination address','completed explicit memcpy/memmove requested bytes','source pointer identity (uint64, not additive)',accumulate='sum completed explicit requested bytes per kind; do not add relocation extent again',owner='any' if name=='load_header_copy' else 'nonzero',subject='nonzero')
add('grow_zero','mapping base','destination address','completed explicit zeroed bytes','0',subject='nonzero',owner='nonzero',value_equals=0)
for name in ('save_header_append_extent','save_payload_append_extent','copy_bytes_string_extent'):
    add(name,'arena base','0','completed logical string output extent','0',accumulate='logical extent, not internal libc physical copies',owner='nonzero',subject='zero',value_equals=0)
for base in ('os_reserve','os_replace','os_enable','os_protect','os_unmap','os_reclaim','os_advise'):
    for failed in (False,True):
        add(base+('_failed' if failed else ''),'causal Scope or 0; use subject mapping interval for storage identity','mapping result address (reserve) or affected subrange; failed anonymous reserve can be 0','requested syscall range bytes','errno on failure, 0 on success',stage='syscall failed' if failed else 'syscall succeeded',accumulate='calls and requested extent by kind; os_replace/reclaim indicates completed discard; enable/protect indicate permission range, not RSS',subject='any' if failed and base=='os_reserve' else 'nonzero',**({} if failed else {'value_equals':0}))
add('overflow_control','0','0','0','0',stage='negative control only; invalid full trace must be rejected',accumulate='never interpret cost',owner='zero',subject='zero',bytes_equals=0,value_equals=0)
add('expand_rejected','arena base, or 0 for empty owner','attempted user pointer, possibly 0','requested new bytes','reason enum 1 invalid request/empty, 2 pointer/alignment, 3 physical header/non-tail, 4 capacity',stage='failed decision',accumulate='count by reason; request bytes only',value_min=1,value_max=4)
add('relocate_trivial_copy','causal arena Scope, otherwise 0 for direct primitive use','destination address','completed explicit memcpy bytes','source pointer identity; not additive',subject='nonzero')
add('vm_injected_failure','causal Scope or 0','0','0','one-shot VmOperation bit (1/2/4/8/16)',stage='injected rejection; no syscall claimed',accumulate='count by bit only',subject='zero',bytes_equals=0,value_min=1,value_max=16)
output={'schema':'n3-event-kinds-v1','integer_rule':'uint64 exact integer; bool forbidden; pointer 0 only where allowed','kinds':k}
Path(__file__).with_name('event_kinds.json').write_text(json.dumps(output,indent=2,sort_keys=True)+'\n')
print(len(k),'event kinds')
