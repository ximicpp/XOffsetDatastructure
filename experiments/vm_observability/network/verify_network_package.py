#!/usr/bin/env python3
"""Offline controls against relocated small copies of actual native evidence."""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
sys.dont_write_bytecode=True

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--collection',type=Path,required=True);ap.add_argument('--build',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
    collection=a.collection.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    source=collection/'input/tool/run_network_observation.py';namespace={'__name__':'offline_package_controls','__file__':str(source)};exec(compile(source.read_text(),str(source),'exec'),namespace)
    read,write=namespace['read'],namespace['write'];case='4096-warm';records=[]
    def copy_case(name):
        p=out/name;p.mkdir();shutil.copytree(collection/'input',p/'input');shutil.copy2(collection/'identity.json',p/'identity.json');shutil.copy2(collection/'build-result.json',p/'build-result.json');(p/'cases').mkdir()
        for entry in (case,'fixture-4096'):shutil.copytree(collection/'cases'/entry,p/'cases'/entry)
        return p
    def audit_case(p):
        namespace['collection_identity'](p);namespace['audit'](p/'input');namespace['audit_case_dir'](p/'cases'/case,p)
    def reseal_case(p):
        target=p/'cases'/case;d=read(target/'result.json');namespace['seal'](target,d)
    def control(name,mutate,checker=audit_case,reseal=True):
        p=copy_case(name);mutate(p)
        if reseal:reseal_case(p)
        try:checker(p)
        except Exception as error:
            records.append({'control':name,'status':'rejected_as_required','error_type':type(error).__name__,'reason':str(error)})
        else:raise RuntimeError('negative accepted: '+name)
    p=copy_case('relocated-positive');audit_case(p);records.append({'control':'relocated-positive','status':'passed','scope':'prepared input and complete warm case; large full collection not duplicated'})
    def change_json(p,path,key,value):
        f=p/path;d=read(f);d[key]=value;write(f,d)
    control('raw-stdout-semantic-mismatch',lambda p:change_json(p,'cases/'+case+'/observed.stdout','semantic_checksum','1'))
    control('raw-stdout-timing-promotion',lambda p:change_json(p,'cases/'+case+'/observed.stdout','timing_evidence',True))
    control('dump-before-join',lambda p:change_json(p,'cases/'+case+'/observed/n3-observation.json','joined',False))
    def mutate_byte(p,path):
        f=p/path;data=f.read_bytes();f.write_bytes(bytes([data[0]^1])+data[1:])
    control('work-wire-byte-mismatch',lambda p:mutate_byte(p,'cases/'+case+'/observed/output-0.wire'))
    control('warm-wire-byte-mismatch',lambda p:mutate_byte(p,'cases/'+case+'/observed/n3-warm-output.wire'))
    control('input-wire-identity-mismatch',lambda p:mutate_byte(p,'cases/fixture-4096/reference.wire'))
    control('sidecar-trace-identity-mismatch',lambda p:change_json(p,'cases/'+case+'/sidecar.json','trace_sha256','0'*64))
    control('binary-identity-mismatch',lambda p:change_json(p,'identity.json','binary_sha256','0'*64))
    def bad_header(p):
        header=p/'input/source/xoffsetdatastructure.hpp';header.write_text(header.read_text()+'\n// negative identity mutation\n');namespace['seal'](p/'input',read(p/'input/result.json'))
    control('frozen-diagnostic-header-mismatch',bad_header,checker=lambda p:namespace['audit'](p/'input'))
    def deleted_warm(p):
        f=p/'cases'/case/'observed/n3-events.json';d=read(f);d['events']=[e for e in d['events'] if '.warm.' not in e['phase']]
        for i,e in enumerate(d['events'],1):e['sequence']=i
        write(f,d)
    control('deleted-recorded-warm-prefix',deleted_warm)
    # Native binary integrity is verified without executing it.
    build=out/'binary-integrity';shutil.copytree(a.build,build);binary=build/'n3_network';data=binary.read_bytes();binary.write_bytes(bytes([data[0]^1])+data[1:]);namespace['seal'](build,read(build/'result.json'))
    try:namespace['audit'](build)
    except Exception as error:records.append({'control':'native-binary-byte-mismatch','status':'rejected_as_required','error_type':type(error).__name__,'reason':str(error)})
    else:raise RuntimeError('binary mutation accepted')
    manifest={'schema':'n3-network-package-controls-v1','diagnostic':True,'timing_evidence':False,'cpp_executions':0,'native_collection_result_sha256':namespace['sha'](collection/'result.json'),'runner_sha256':namespace['sha'](source),'controls':records}
    write(out/'controls.json',manifest);namespace['check_budget']();print(json.dumps({'status':'passed','positive_controls':1,'negative_controls':len(records)-1,'cpp_executions':0}))
if __name__=='__main__':main()
