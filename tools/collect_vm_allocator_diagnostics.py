#!/usr/bin/env python3
"""Collect allocator counters from separate diagnostic binaries; never formal time evidence."""
import argparse,json,sys
from pathlib import Path
import run_vm_allocator as R

MICRO_COUNTERS={'host_allocation_calls','host_requested_bytes','host_metadata_peak_bytes','core_scans','core_moved_bytes','core_new','core_whole','core_split','wire_scans'}
PROCESS_COUNTERS={'core_scans','core_moved_bytes','core_new','core_whole','core_split','wire_scans','wire_split','wire_reuse','wire_retire','wire_flush','wire_transfer_blocks','expand_attempts','expand_existing','expand_grown','element_migration_bytes','metadata_allocation_calls','metadata_allocated_bytes','max_owner_metadata_capacity'}
def counter_schema(suite):
    if suite=='micro':return MICRO_COUNTERS
    if suite in ['fragment','trace']:return PROCESS_COUNTERS
    raise RuntimeError('unknown diagnostic counter suite')
def matrix(meta):
    return [(variant,suite,case) for variant in meta['variants'] for suite in ['micro','fragment','trace'] for case in R.cases(suite)]
def semantic(sample):
    checksum=sample.get('semantic_checksum',sample.get('checksum'))
    count=sample.get('operations',sample.get('operation_count',sample.get('size')))
    if type(checksum) is not int or type(count) is not int or count<=0:raise RuntimeError('invalid diagnostic semantic identity')
    return checksum,count,sample.get('trace_checksum')
def audit(out,prepared_override=None):
    state=json.loads((out/'result.json').read_text())
    if state.get('status')!='complete' or state.get('formal_timing_forbidden') is not True or (out/'COMPLETE').read_text().strip()!=R.sha(out/'result.json'):raise RuntimeError('incomplete diagnostic record')
    prep=Path(prepared_override or state['prepared'])
    if R.sha(prep/'prepared.json')!=state['prepared_sha256']:raise RuntimeError('diagnostic prepared identity changed')
    meta=R.prepared(prep,external=False)
    if R.sha(out/'raw.jsonl')!=state['raw_sha256']:raise RuntimeError('diagnostic raw hash changed')
    rows=[json.loads(line) for line in (out/'raw.jsonl').read_text().splitlines()]
    expected=matrix(meta)
    if len(rows)!=len(expected) or state['samples']!=len(rows):raise RuntimeError('missing/extra diagnostic rows')
    reference={}
    for sequence,((variant,suite,case),row) in enumerate(zip(expected,rows)):
        target={'micro':'diag','fragment':'fragment_diag','trace':'trace_diag'}[suite]
        command=[str(Path(state['prepared'])/variant/target)]+case['args']
        if (row['variant'],row['suite'],row['case'],row['sequence'])!=(variant,suite,case['id'],sequence) or row['command']!=command:raise RuntimeError('diagnostic matrix/command mismatch')
        R.validate_sample(case,row['sample'])
        identity=semantic(row['sample']);key=(suite,case['id'])
        if key in reference and reference[key]!=identity:raise RuntimeError('diagnostic logical work differs')
        reference[key]=identity
        if row['diagnostic'] is not True:raise RuntimeError('missing diagnostic classification')
        if type(row['counters']) is not dict or set(row['counters'])!=counter_schema(suite):raise RuntimeError('missing/extra diagnostic counters')
        if any(type(v) is not int or v<0 for v in row['counters'].values()):raise RuntimeError('invalid diagnostic counter')
        if suite=='micro' and any(row['sample'].get(k)!=v for k,v in row['counters'].items()):raise RuntimeError('micro sample/counter report mismatch')
    return {'status':'audited','samples':len(rows),'formal_timing_forbidden':True}

def layout(suite,sample):
    identity=semantic(sample)
    if suite=='micro':detail={k:sample[k] for k in ['used_end','logical_bytes']}
    elif suite=='fragment':detail={k:sample['metrics'][k] for k in ['used_end','pressure_successes','pressure_verified_bytes','pressure_used_end','live_payload_bytes']}
    elif suite=='trace':detail={k:sample[k] for k in ['logical_bytes','used_end','core_free_bytes','wire_free_bytes','wire_physical_bytes','wire_blocks','accessible_bytes','reserved_bytes','primary_offset','secondary_offset','primary_physical_bytes','secondary_physical_bytes','primary_capacity','secondary_capacity','allocation_retries','observed_growth_operations','retired_values_checked']}
    else:raise RuntimeError('unsupported layout comparison suite')
    return identity,detail

def compare_timing(out,timing_records,prepared_override=None):
    audit(out,prepared_override)
    state=json.loads((out/'result.json').read_text());diagnostic_prep=Path(prepared_override or state['prepared']);meta=R.prepared(diagnostic_prep,external=False)
    diagnostics=[json.loads(line) for line in (out/'raw.jsonl').read_text().splitlines()]
    expected={(r['variant'],r['suite'],r['case']):layout(r['suite'],r['sample']) for r in diagnostics}
    found=set();matched=0
    for window,prep in timing_records:
        R.audit(window,prepared_override=prep)
        timing_meta=R.prepared(prep,external=False);result=json.loads((window/'result.json').read_text());suite=result['suite']
        if suite not in ['micro','fragment','trace']:continue
        for line in (window/'raw.jsonl').read_text().splitlines():
            row=json.loads(line);key=(row['variant'],suite,row['case']);header=row['variant']+'/source/xoffsetdatastructure.hpp'
            if key not in expected:raise RuntimeError('formal row has no diagnostic counterpart')
            for filename in ['xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp']:
                source=row['variant']+'/source/'+filename
                if timing_meta['files'][source]!=meta['files'][source]:raise RuntimeError('diagnostic and timing candidate sources differ')
            if layout(suite,row['sample'])!=expected[key]:raise RuntimeError('diagnostic/timing logical layout differs')
            matched+=1;found.add(key)
    if found!=set(expected):raise RuntimeError('missing formal layout crosschecks')
    return {'status':'audited','diagnostic_samples':len(diagnostics),'matched_timing_samples':matched,'timing_values_compared':False}

def collect(a):
    prep=a.prepared.resolve();meta=R.prepared(prep);identity=R.sha(prep/'prepared.json');out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    state={'status':'running','started':R.now(),'prepared':str(prep),'prepared_sha256':identity,'formal_timing_forbidden':True}
    R.write(out/'result.json',state);rows=[]
    try:
        with (out/'raw.jsonl').open('w') as stream:
            for variant,suite,case in matrix(meta):
                target={'micro':'diag','fragment':'fragment_diag','trace':'trace_diag'}[suite]
                command=[prep/variant/target]+case['args'];result=R.run(command,300);sample=json.loads(result.stdout)
                if suite=='micro':counters={k:v for k,v in sample.items() if k.startswith(('core_','wire_','host_'))}
                else:
                    lines=[line[len('S3DIAG:'):] for line in result.stderr.splitlines() if line.startswith('S3DIAG:')]
                    if len(lines)!=1:raise RuntimeError('missing/duplicate diagnostic report')
                    counters=json.loads(lines[0])
                row={'variant':variant,'suite':suite,'case':case['id'],'sequence':len(rows),'command':list(map(str,command)),'diagnostic':True,'sample':sample,'counters':counters}
                rows.append(row);stream.write(json.dumps(row)+'\n');stream.flush()
            R.prepared(prep)
            if R.sha(prep/'prepared.json')!=identity:raise RuntimeError('prepared manifest changed')
        state.update(status='complete',finished=R.now(),samples=len(rows),raw_sha256=R.sha(out/'raw.jsonl'));R.write(out/'result.json',state);(out/'COMPLETE').write_text(R.sha(out/'result.json')+'\n');audit(out)
    except BaseException as e:R.record_failure(out,state,e);raise

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='mode',required=True)
    a=sub.add_parser('collect');a.add_argument('--prepared',type=Path,required=True);a.add_argument('--out',type=Path,required=True)
    a=sub.add_parser('audit');a.add_argument('out',type=Path)
    args=p.parse_args()
    if args.mode=='collect':collect(args)
    else:print(json.dumps(audit(args.out),indent=2))
if __name__=='__main__':
    try:main()
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
