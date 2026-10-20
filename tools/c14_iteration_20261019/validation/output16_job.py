"""One C14 fixed-M setup and exactly16 owning emits; diagnostic counts, no clocks."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess

H=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('cg',H/'m1_codegen_job.py')
C=importlib.util.module_from_spec(s);s.loader.exec_module(C);J=C.J
OUT=J.OUTPUT/'s1-output16-v1';LEASE=J.OUTPUT/'leases/s1-output16-v1'
ADMISSION=J.DOC/'S1_OUTPUT16_ADMISSION.json'

def once(text,old,new):
    J.require(text.count(old)==1,'ambiguous output observation injection')
    return text.replace(old,new,1)

def worker():
    a=J.read(ADMISSION)
    for p,v in a['inputs'].items():J.require(J.fid(p)==v,'output16 input changed: '+p)
    holder=J.read(J.LOCK/'queue.lock');J.require(holder['status']=='running' and Path(holder['out'])==LEASE,'wrong lease')
    OUT.mkdir(parents=True,exist_ok=False)
    source=OUT/'source';source.mkdir()
    material=J.read(J.ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    # Header-only consumer: copy exact fourteen-file closure; no baseline mutation.
    for n in material['production_files']:shutil.copyfile(J.ROOT/n,source/n)
    h=(source/'xoffsetdatastructure.hpp').read_text()
    old='''                std::memcpy(output, &header, sizeof(header));
                std::memcpy(output + sizeof(header), base, payload_size);'''
    new='''                ++output16::header_copies; output16::header_bytes += sizeof(header);
                std::memcpy(output, &header, sizeof(header));
                ++output16::payload_copies; output16::payload_bytes += payload_size;
                std::memcpy(output + sizeof(header), base, payload_size);'''
    (source/'xoffsetdatastructure.hpp').write_text(once(h,old,new))
    exp=J.ROOT/'handoff/continuation-20261019-v1/experiments/serialization_comparison'
    driver=(exp/'driver.cpp').read_text()
    old='''    auto final_wire=A::emit(state);state={};auto reloaded=A::load(sc::wire_view(final_wire));'''
    new='''    // Fixed16 sequential independent owning values. No page preparation.
    std::string final_wire;
    for(unsigned iteration=0;iteration<16;++iteration) {
        rusage before{},after{}; output16::reset();
        getrusage(RUSAGE_SELF,&before); output16::active=true;
        auto owned=A::emit(state);
        output16::active=false; getrusage(RUSAGE_SELF,&after);
        const auto digest=sc::sha256_hex(sc::wire_view(owned));
        std::cout << "OUTPUT16 iteration=" << iteration << " bytes=" << owned.size()
          << " allocations=" << output16::allocations << " requested=" << output16::requested
          << " minor=" << after.ru_minflt-before.ru_minflt << " major=" << after.ru_majflt-before.ru_majflt
          << " allocation_minor=" << output16::allocation_minor << " allocation_major=" << output16::allocation_major
          << " header_copies=" << output16::header_copies << " header_bytes=" << output16::header_bytes
          << " payload_copies=" << output16::payload_copies << " payload_bytes=" << output16::payload_bytes
          << " wire_sha256=" << digest << '\\n';
        if(iteration==15) final_wire=std::move(owned);
    }
    state={};auto reloaded=A::load(sc::wire_view(final_wire));'''
    cpp=OUT/'output16_driver.cpp';cpp.write_text(once(driver,old,new))
    compiler=J.TOOLS/'p2996-profile/bin/clang++-libcxx'
    includes=['-I'+str(J.VENDOR/'external/typelayout/include')]
    includes+=['-I'+str(J.VENDOR/f'external/boost/libs/{n}/include') for n in ('config','container','container_hash','core','intrusive','predef','static_assert','throw_exception','type_traits','winapi')]
    binary=OUT/'output16'
    commands=[['/usr/bin/python3','-V']] if False else []
    def run(label,argv):
        argv=list(map(str,argv))
        with (OUT/(label+'.log')).open('xb') as log:r=subprocess.run(argv,cwd=OUT,stdout=log,stderr=subprocess.STDOUT,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
        commands.append({'label':label,'argv':argv,'returncode':r.returncode})
        J.write(OUT/(label+'-receipt.json'),commands[-1]);J.require(r.returncode==0,label+' failed; inspect log')
    run('compile',[compiler,'-std=gnu++26','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread','-O3','-DNDEBUG',
       '-include',H/'output16_observe.hpp','-I'+str(source),'-I'+str(exp),'-I'+str(exp/'include'),
       '-DSC_ADAPTER_HEADER="adapters/xoffset/adapter.hpp"','-DSC_ADAPTER_TYPE=sc::XOffsetAdapter',*includes,cpp,'-o',binary])
    portable=J.ROOT/'handoff/continuation-20261019-v1/portable-inputs'
    run('fixed-M-output16',[binary,'validate','--wire',portable/'old-V8-initial-wire.bin','--trace',portable/'fixed-M/trace.bin','--result',OUT/'validation.json'])
    for p,v in a['inputs'].items():J.require(J.fid(p)==v,'input changed after output16: '+p)
    J.write(OUT/'receipt.json',{'status':'passed','commands':commands,'inputs':a['inputs'],'lease_holder':holder,
        'diagnostic_main_identity':J.fid(source/'xoffsetdatastructure.hpp'),'diagnostic_driver_identity':J.fid(cpp),
        'binary_identity':J.fid(binary),'actual_native_runs':1,'owning_emits':16,'timing_samples':0})

def main():
    p=argparse.ArgumentParser();p.add_argument('--worker',action='store_true');args=p.parse_args()
    if args.worker:worker();return
    J.require(not OUT.exists() and not LEASE.exists(),'fresh output16 directory required')
    J.require(shutil.disk_usage('/mnt/c').free>8*1024**3,'backing budget required')
    material=J.read(J.ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    inputs={str(J.ROOT/n):J.fid(J.ROOT/n) for n in material['production_files']}
    for n,v in material['production_files'].items():J.require(inputs[str(J.ROOT/n)]==v,'baseline changed')
    for p in (H/'output16_job.py',H/'output16_observe.hpp',J.RUNNER,J.TOOLS/'p2996-profile/bin/clang++-libcxx'):inputs[str(p)]=J.fid(p)
    a=J.read(C.ADMISSION)
    inputs.update(a['experiment_files']);inputs.update(a['tool_files'])
    portable=J.ROOT/'handoff/continuation-20261019-v1/portable-inputs'
    for p in (portable/'old-V8-initial-wire.bin',portable/'fixed-M/trace.bin'):inputs[str(p)]=J.fid(p)
    J.write(ADMISSION,{'status':'passed','scope':'S1 fixed16 owning materialization only; one fixed-M trajectory distinct from candidate-specific S2 counter pair',
         'inputs':inputs,'compile_budget':1,'native_budget':1,'emits':16,'deadline_seconds':600,
         'limitations':'No clocks or speed claim. Global new observes string allocation requests; allocation page faults are a subset of emit page faults. Copy pages cannot be separated from normalization fault events; no prefault/touch/hotcopy interventions.'})
    argv=['/usr/bin/python3','-I','-B',str(J.RUNNER),'run','--owner','W1','--kind','diagnostic','--out',str(LEASE),'--timeout-seconds','600','--wait-seconds','0','--','/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'--worker']
    J.write(J.DOC/'S1_OUTPUT16_DISPATCH_REQUEST.json',{'argv':argv,'admission':J.fid(ADMISSION)})
    r=subprocess.run(argv,cwd=J.ROOT,env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','TZ':'UTC'})
    J.write(J.DOC/'S1_OUTPUT16_DISPATCH_RESULT.json',{'returncode':r.returncode,'lease_result':str(LEASE/'result.json')})
    raise SystemExit(r.returncode)

if __name__=='__main__':main()
