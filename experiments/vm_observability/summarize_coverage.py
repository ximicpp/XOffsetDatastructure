"""Render scoped execution coverage; declared events are not execution claims."""
import collections,json
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
p=ROOT/'docs/validation/vm-observability/collection-06'
aux=ROOT/'docs/validation/vm-observability/runtime-controls-01'
catalog=json.loads((HERE/'event_kinds.json').read_text())['kinds']
exact={
'copy_bytes_string_extent':['owner_basic bytes=4096; owner_transfer count=0 including empty owner under outer Scope'],
'core_new':['owner_basic=1, core_metadata=11, threads=2'],
'core_return':['owner_basic=1, core_metadata=12, failure_metadata=1'],
'core_return_failed':['failure_metadata=1'],
'core_reuse':['core_metadata=1, threads=2'],
'core_scan':['core_metadata=1, threads=2'],
'core_erase_movement_estimate':['core_metadata logical estimate bytes=112'],
'core_metadata_capacity':['owner_transfer four empty-owner snapshots have owner=0 and bytes=0; other capacities structurally audited'],
'host_allocate':['core_metadata domain1 calls=7, requested bytes=352, two origin owners; all traces allocation ledger'],
'host_deallocate':['all completed frees match live allocation subject/domain/bytes; fixed probes retain only registry metadata'],
'host_allocate_failed':['failure_registry=1, failure_metadata=1; actual allocator auxiliary=1'],
'load_payload_copy':['copy_wire count=2 and bytes=2*payload_bytes=1200'],
'save_payload_copy':['copy_wire bytes=payload_bytes=600'],
'load_header_copy':['copy_wire count=3'],
'save_header_copy':['copy_wire count=1'],
'string_relocate_copy':['copy_wire bytes=3'],
'vector_relocated_element':['copy_wire logical completed extent bytes=16'],
'grow_zero':['failure_vm completed bytes=65536'],
'pool_scan':['pool_reclaim count=1'],
'pool_lock':['pool_reclaim count>=5 (actual8); waits/holds not performance evidence'],
'os_unmap':['pool_reclaim count>=2 (actual2)'],
'vm_injected_failure':['failure_vm count=2, operation bits=[4,8]'],
'registry_register':['owner_basic=1, core_metadata=2, threads=2, owner_transfer=2'],
'registry_erase':['owner_basic=1, core_metadata=2, threads=2, owner_transfer=2; active registry empty at fixed-probe dump'],
'registry_lookup_hit':['owner_basic=2; registry ledger checks owner mapping across all traces'],
'registry_lookup_miss':['owner_transfer=1 and exact-zero owner legal constraint'],
'registry_rebind':['owner_transfer=4; ledger checks base generation and current C++ owner'],
'registry_register_failed':['failure_registry=2'],
'os_replace':['failure_vm os_replace+os_reclaim count=1 (macOS actual os_replace1)']}
rows={k:{'kind':k,'ordinary_runs':[],'invalid_controls':[],'assertions':exact.get(k,[])} for k in catalog}
for case in json.loads((p/'result.json').read_text())['cases']:
    trace=json.loads((p/(case+'-diagnostic.stdout')).read_text())['trace']
    for kind,count in collections.Counter(e['kind'] for e in trace['events']).items():
        rows[kind]['ordinary_runs' if trace['valid'] else 'invalid_controls'].append({'case':case,'count':count,'source':str((p/(case+'-diagnostic.stdout')).relative_to(ROOT))})
for case in ('threads','labels','allocator'):
    trace=json.loads((aux/(case+'.stdout')).read_text())
    for kind,count in collections.Counter(e['kind'] for e in trace['events']).items():
        rows[kind]['ordinary_runs' if trace['valid'] else 'invalid_controls'].append({'case':'runtime_'+case,'count':count,'source':str((aux/(case+'.stdout')).relative_to(ROOT))})
for row in rows.values():
    row['status']='actual_with_asserted_counts_or_ledger' if row['ordinary_runs'] and row['assertions'] else 'actual_structural_audit_only' if row['ordinary_runs'] else 'invalid_control_only' if row['invalid_controls'] else 'declared_catalog_not_executed_this_round'
    if row['status']=='declared_catalog_not_executed_this_round':row['note']='Known-kind/legal-value catalog is loaded and schema constraints are unit-tested generically; this does not execute or verify this C++ path. Linux/Windows and fallback string branches are separate prerequisites where applicable.'
summary=dict(collections.Counter(r['status'] for r in rows.values()))
result={'schema':'n3-event-validation-coverage-v1','declared_kinds':len(rows),'classification_counts':summary,'scope':'macOS arm64 explicit frozen SDK26.5; collection06+auxiliary01 only; invalid controls are never cost evidence; generic catalog tests are not per-kind native execution','kinds':list(rows.values())}
(HERE/'event-validation-coverage.json').write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
print(json.dumps(summary,sort_keys=True))
