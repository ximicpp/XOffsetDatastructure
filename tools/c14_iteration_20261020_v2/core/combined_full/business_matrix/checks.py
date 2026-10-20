"""Strict small-JSON checks for the frozen matrix CPP output, no native code."""
import json,re,os
from pathlib import Path

def need(value,message):
    if not value:raise ValueError(message)
def read(path):
    def unique(pairs):
        result={}
        for key,value in pairs:
            need(key not in result,'duplicate JSON key');result[key]=value
        return result
    def invalid(value):raise ValueError('nonfinite JSON: '+value)
    return json.loads(Path(path).read_text(),object_pairs_hook=unique,parse_constant=invalid)
def u(value):
    need(type(value) is str and re.fullmatch(r'0|[1-9][0-9]{0,19}',value),'uint64 decimal string')
    result=int(value);need(result<2**64,'uint64 overflow');return result
def ints(value,expected):need(type(value) is int and value==expected,'exact JSON integer')
def nums(values):return {k:u(v) for k,v in values.items()}
def duration(value):
    need(set(value)=={'wall_ns','process_cpu_ns'},'clock fields');return nums(value)
def global_vm(value):
    need(value['status']=='not_measured' and value['active_bytes'] is None and value['retained_bytes'] is None,'VM global unmeasured is null')
def clocks(c,total_key,phases):
    need(set(c['phases'])==set(phases) and c['phase_accounting_exhaustive'] is False,'phase inventory/nonexhaustive label')
    total=duration(c[total_key]);times={k:duration(v) for k,v in c['phases'].items()};extra=duration(c['unattributed_time'])
    for key in total:need(sum(t[key] for t in times.values())+extra[key]==total[key],'phase plus unattributed must equal total')
    global_vm(c['vm_global_active_and_retained']);return total
ABDE_PHASES=('input_decode','build','A_empty','hot_output_prepare','A_hot','B_full_freeze_E_scratch','D_ownership_index_prepare','D_ownership_queries','B_cancel_backpressure_completion_model','semantic_oracle','release_cycle_owners')
C_PHASES=('input_decode','producer_build','producer_owning_save','C_token_prepare','C_token_grow','C_byte_fill','C_actual_operation_complete','C_cancel_retention_and_rearm','C_full_verify_activate_publish','query_before','C_nine_stage_mutation','full_value_stage_oracles','query_after','C_modified_owning_resave','C_reload_token_prepare','C_reload_token_grow','C_reload_byte_fill','C_reload_actual_operation_complete','C_reload_full_verify_activate_publish','query_reloaded','release_cycle_owners')
POOL_PHASES=('explicit_pool_setup','producer_build','producer_owning_save','first_token_prepare','first_token_grow','first_operation_acquire','header_and_prefix_byte_fill','token_destroy_while_operation_holds','cancel_without_completion','post_cancel_suffix_byte_fill','last_operation_complete_and_return_to_pool','second_token_prepare_pool_hit','second_token_grow','second_operation_acquire','second_full_byte_fill','second_operation_complete','full_raw_verify_activate_publish','initial_business_truth','received_owner_mutation','mutated_business_truth','modified_independent_owning_save','secondary_copyload_full_verify_and_truth','release_received_owner_to_pool','explicit_pool_clear','pool_and_VMA_observers','release_remaining_fixture_and_metadata')

def common(raw,main,fixture,n,mode,target):
    ints(raw['schema_version'],1);need(raw['error'] is None and raw['diagnostic_only'] is True,'passed native semantics/error')
    need(raw['source_main_sha256']==main and raw['input_canonical_sha256']==fixture['input']['sha256'],'source/fixture identity')
    ints(raw['records'],n);need(raw['mode']==mode,'mode')
    count=16 if mode=='repeat16' else 1;receivers=3 if mode=='shared3' else 1
    for key in ('expected_cycles','attempted_cycles','completed_cycles'):ints(raw[key],count)
    need(len(raw['cycles'])==count and raw['process_thread_exit_cleanup_not_timed'] is True,'complete cycle prefix')
    for i,c in enumerate(raw['cycles']):
        ints(c['cycle'],i);ints(c['receivers'],receivers)
        need(c['started'] is True and c['status']=='passed','complete cycle status')
    return count,receivers

def abde(raw,main,fixture,n,mode):
    count,receivers=common(raw,main,fixture,n,mode,'matrix_abde')
    need(raw['kind']=='production-ABDE-lifecycle-model' and raw['status']=='passed_ABDE_C_not_exercised','ABDE exact scope')
    need(raw['C_receive']['status']=='not_exercised_here' and all(raw['C_receive'][k] is None for k in ('token_prepare_cpu_ns','network_body_fill_bytes','token_upfront_max_accessible')),'ABDE does not replace C')
    need(raw['explicit_output_scratch_index_retention_released'] is True and raw['no_old_owning_save_target_substitution'] is True and raw['host_metrics_are_cpp_new_requests_not_malloc_RSS_or_arena_calls'] is True,'ABDE retention/scope')
    total=duration(raw['total_including_oracles_and_final_release']);sums=duration(raw['explicit_context_setup']);final=duration(raw['final_output_and_scratch_release'])
    for k in total:sums[k]+=final[k]
    for c in raw['cycles']:
        observed=clocks(c,'total_including_oracle',ABDE_PHASES)
        for k in total:sums[k]+=observed[k]
        wire=u(c['wire_bytes']);need(wire>88 and u(c['A_owned_full_output_copy_bytes'])==2*wire and u(c['modeled_network_bytes_no_actual_network'])==wire*receivers,'ABDE wire/copy/network model')
        ints(c['cancelled_send_leases'],1);ints(c['application_model_backpressure_events'],1)
        vm=nums(c['vm']);need(vm['logical_snapshot']+88==wire and vm['logical_snapshot']<=vm['accessible_bytes']<=vm['reserved_bytes'] and vm['logical_snapshot']<=vm['maximum_capacity'],'ABDE VM extents')
        idx=c['ownership_index'];need(type(idx['enabled']) is bool,'index enabled type')
        index={k:u(v) for k,v in idx.items() if k!='enabled'}
        need(index['strict_negative_queries']+index['fallback_queries']==n*receivers,'ownership not business ID query count')
        need(nums(c['scratch'])['active_raw_after']==0,'ABDE active scratch')
        h=nums(c['host_after_cycle']);need(h['live_cpp_requested']<=h['peak_cpp_requested_so_far'] and h['live_meter_malloc_requested']<=h['peak_meter_malloc_requested_so_far'],'ABDE host requests')
    for k in total:need(sums[k]<=total[k],'ABDE whole case clock')
    nums(raw['host_before']);nums(raw['host_after_final_release'])
    return {'cycles':count,'receiver_completions':count*receivers}

def receive(raw,main,fixture,n,mode):
    count,receivers=common(raw,main,fixture,n,mode,'matrix_receive')
    need(raw['kind']=='production-C-direct-receive-lifecycle-model' and raw['status']=='passed_C_direct_receive','actual C status')
    need(raw['trace_sha256']==fixture['trace']['sha256'] and raw['actual_network'] is False and raw['copyload_used_as_receive'] is False,'C trace/no substituted copyload')
    need(u(raw['configured_receive_maximum'])==536870912 and raw['explicit_scratch_released'] is True and raw['host_metrics_are_requests_not_RSS'] is True,'C scope/cleanup')
    total=duration(raw['total_including_oracles_and_release']);sums=duration(raw['explicit_context_setup']);final=duration(raw['final_scratch_release'])
    for k in total:sums[k]+=final[k]
    operations=sum(x['mutation_count'] for x in fixture['stages']);completed=0;payload_zero=0;filled=0
    for c in raw['cycles']:
        observed=clocks(c,'total_including_oracles',C_PHASES)
        for k in total:sums[k]+=observed[k]
        wire=u(c['input_wire_bytes']);need(wire>88 and len(c['owners'])==receivers,'C owner inventory');ints(c['actual_cancel_reject_rearm_probes'],1)
        scratch=nums(c['scratch']);need(scratch['active_after']==0 and scratch['raw_upstream_retained_after']<=scratch['raw_upstream_peak_so_far'],'C scratch requests')
        for i,r in enumerate(c['owners']):
            number={k:u(v) for k,v in r.items() if k not in ('pre_zero_bytes','initial_same_address','reload_same_address','initial_token_consumed','reload_token_consumed','modified_wire_sha256')}
            need(all(r[k] is True for k in ('initial_same_address','reload_same_address','initial_token_consumed','reload_token_consumed')),'C genuine owner transfer/consume')
            need(re.fullmatch('[0-9a-f]{64}',r['modified_wire_sha256']) is not None,'modified wire digest')
            need(number['initial_body_bytes']+88==wire and number['actual_initial_fill_bytes']==wire and number['actual_reload_fill_bytes']==number['modified_body_bytes']+88,'C actual fill bytes')
            need(number['initial_maximum_bytes']==number['reload_maximum_bytes']==536870912 and number['initial_upfront_accessible_bytes']>=536870912 and number['reload_upfront_accessible_bytes']>=536870912,'C up-front max accessible')
            need(number['logical_after_mutation']<=number['accessible_after_mutation']<=number['reserved_after_mutation'],'C mutation VM extents')
            need(number['validated_stages']==9 and number['validated_operations']==operations and number['actual_operation_completions']==(3 if i==0 else 2),'C complete stage/operation/completion truth')
            zero=r['pre_zero_bytes'];need(zero['method']=='successful logical growth extents plus pinned VM memset semantics; not instrumented machine write traffic','prezero source interpretation')
            z={k:u(v) for k,v in zero.items() if k!='method'}
            need(z['prepare_payload']==number['initial_body_bytes']//2 and z['prepare_payload']+z['grow_payload']==number['initial_body_bytes'] and z['reload_prepare_payload']==number['modified_body_bytes']//2 and z['reload_prepare_payload']+z['reload_grow_payload']==number['modified_body_bytes'] and z['each_token_header_value_init']==88,'C exact logical prezero ledger')
            payload_zero+=number['initial_body_bytes']+number['modified_body_bytes']
            filled+=number['actual_initial_fill_bytes']+number['actual_reload_fill_bytes']
            completed+=number['actual_operation_completions']
        nums(c['host_after_cycle'])
    for k in total:need(sums[k]<=total[k],'C whole case clock')
    nums(raw['host_before']);nums(raw['host_after_final_release'])
    return {'cycles':count,'receiver_lifecycles':count*receivers,'successful_finishes':2*count*receivers,'cancelled_finish_rejections':count,'operation_completions':completed,'validated_stages':9*count*receivers,'source_verified_full_query_batches':3*count*receivers,'source_derived_payload_prezero_bytes':payload_zero,'source_derived_header_value_init_bytes':176*count*receivers,'actual_modeled_transport_byte_stores':filled}

def specials(raw,main,fixture):
    ints(raw['schema_version'],1);need(raw['kind']=='production-C-additional-functional-lifecycle-groups' and raw['status']=='passed' and raw['error'] is None,'special groups passed')
    need(raw['source_main_sha256']==main and raw['input_canonical_sha256']==fixture['input']['sha256'] and raw['trace_sha256']==fixture['trace']['sha256'],'special source/fixture')
    for k in ('expected_groups','attempted_groups','completed_groups'):ints(raw[k],3)
    need(raw['diagnostic_only'] is True and raw['timing_inference'] is False and raw['process_TLS_exit_cleanup_not_observed'] is True,'special scope')
    global_vm(raw['vm_global_active_and_retained']);g=raw['groups'];a=g['seven_domains'];ints(a['simultaneous_C_owners'],7)
    need(u(a['complete_value_query_checks'])==42 and a['legacy_normal_read_after_complete_C_drain'] is True,'seven-owner/drain truth')
    b=g['short_completion'];need(u(b['actual_partial_fill_bytes'])>0 and u(b['expected_finish_rejections'])==u(b['successful_finish_after_suffix_completion'])==1 and b['same_payload_address'] is True,'short completion')
    c=g['operation_last_owner'];need(u(c['actual_body_stores_after_token_destroy'])==2*u(b['actual_partial_fill_bytes']) or u(c['actual_body_stores_after_token_destroy'])==2*u(b['actual_partial_fill_bytes'])+1,'same wire partial/body ledger')
    need(c['cancel_retained_spans'] is True and c['actual_complete_dropped_State_pin'] is True,'last-operation pin')

def pool(raw,main):
    ints(raw['schema_version'],1);need(raw['kind']=='independent-small-C-explicit-pool-lifetime-probe' and raw['status']=='passed_cache_ledger_and_business_truth' and raw['error'] is None and raw['source_main_sha256']==main,'pool status/source')
    for k in ('diagnostic_only','active_count_is_model_ledger','VMA_observer_is_optional_not_pass_criterion','phase_sum_within_total','last_operation_holder_released_to_pool','same_address_pool_reuse','initial_receive_finished_directly','initial_business_truth','mutated_business_truth','secondary_copyload_business_truth'):need(raw[k] is True,'pool positive '+k)
    for k in ('actual_network','formal_performance_measurement','production_policy','RSS_or_physical_commit_measured','pool_clear_proves_OS_unmap_success'):need(raw[k] is False,'pool limited scope '+k)
    maximum=u(raw['configured_maximum_bytes']);reserved=u(raw['captured_reservation_bytes']);accessible=u(raw['captured_accessible_bytes']);body=u(raw['body_bytes']);wire=u(raw['wire_bytes'])
    need(u(raw['page_bytes'])==os.sysconf('SC_PAGE_SIZE') and maximum==u(raw['page_bytes'])*16==reserved==accessible and 0<body<maximum and wire==body+88,'independent small pool policy')
    for k in ('first_factory_plus_grow_prezero_bytes','second_factory_plus_grow_prezero_bytes'):need(u(raw[k])==body,'pool prezero ledger')
    for k in ('first_actual_byte_fill_bytes','second_actual_byte_fill_bytes'):need(u(raw[k])==wire,'pool fill ledger')
    need(set(raw['phases'])==set(POOL_PHASES),'pool phase inventory');times=[duration(x) for x in raw['phases'].values()];total=duration(raw['total_including_observers_and_release']);reported=duration(raw['phase_time_sum']);extra=duration(raw['unattributed_including_wrapper_checks'])
    for k in total:need(sum(t[k] for t in times)==reported[k] and reported[k]+extra[k]==total[k],'pool clock ledger')
    names=('baseline','prepared_operation_holds','token_destroyed_operation_holds','cancelled_operation_still_holds','last_complete_idle_cached','same_mapping_reacquired','published_mutable_owner','owner_released_idle_cached','after_pool_clear')
    need([c['checkpoint'] for c in raw['checkpoints']]==list(names),'pool checkpoint exact prefix')
    for i,c in enumerate(raw['checkpoints']):
        need(c['recorded'] is True,'pool checkpoint recorded')
        numeric={k:u(v) for k,v in c.items() if k not in ('checkpoint','recorded','VMA_observation','VMA_observer_errno')}
        cached=1 if i in (4,7) else 0;live=1 if i in (1,2,3,5,6) else 0
        need(numeric['model_live_receive_mappings']==live and numeric['cached_blocks']==cached and numeric['cached_reserved_bytes']==reserved*cached and numeric['cached_accessible_bytes']==accessible*cached,'pool active/idle ledger')
        need(numeric['pool_hits']==(1 if i>=5 else 0) and numeric['pool_misses']==(1 if i>=1 else 0),'pool reuse counter')
        need(type(c['VMA_observer_errno']) is int and c['VMA_observer_errno']>=0 and numeric['VMA_overlap_bytes']<=reserved,'optional VMA observation type')
        need(c['VMA_observation'] in ('not_attempted','unresolved_read_error','no_overlap_observed','unresolved_reused_or_unreleased','full_reservation_coverage_observed','partial_or_no_coverage_observed'),'optional VMA disposition')
