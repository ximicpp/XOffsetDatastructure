"""Bind actual combined full73x2, six repository targeted scopes and external4."""
import json,hashlib,re,shlex,xml.etree.ElementTree as ET
from pathlib import Path
R=Path(__file__).resolve().parents[3]
D=R/'docs/coordination/c14-iteration-20261020-v2/validation/combined_full'
Q=R/'docs/coordination/c14-iteration-20261020-v2/core/REQUESTS_COMBINED_INITIAL.json'
V=Path('/root/xoffset-c14-iteration/20261020/v2/validation')
CORRECTIVE=Q.with_name('REQUESTS_COMBINED_FULL_CORRECTIVE_v2.json')
def read(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def fid(p):
 b=Path(p).read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def ref(p):return {'path':str(p),'identity':fid(p)}
def need(ok,msg):
 if not ok:raise ValueError(msg)
def exact(v):
 p=Path(v['path']);need(fid(p)==v['identity'],'changed ref: '+str(p));return p
def passed(p):
 v=read(p);need(v['status']=='passed','actual pass absent: '+str(p));return v
def verify():
 q=read(Q);prod=q['production14'];correction=read(CORRECTIVE)
 for p,v in correction['tool_files'].items():need(fid(p)==v,'corrective tool/proof changed: '+p)
 need(fid(Q)==correction['original_initial']['identity'],'original INITIAL changed')
 for p,v in q['tool_files'].items():need(fid(p)==v,'initial frozen tool/input changed: '+p)
 identity=R/'build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/IDENTITY.json'
 need(fid(identity)==q['candidate_identity'],'combined identity changed')
 full=passed(D/'COMBINED_FULL_RESULT.json');need(full['request_identity']==fid(Q) and full['production_files']==prod and full['normal_count']==full['sanitized_count']==73,'full initial closure differs')
 checked={};coverage={};names=q['six_repository_targeted']
 for mode,key in [('normal','normal_receipt'),('sanitized','sanitized_receipt')]:
  path=exact(full[key]);v=passed(path);base=path.parent;build=base/'build';source=base/'source'
  req=D/('COMBINED_FULL_'+mode.upper()+'_REQUEST_v1.json')
  need(v['request']['path']==str(req) and {k:v['request'][k] for k in ('bytes','sha256')}==fid(req),'full child request changed')
  inventory=read(build/'test-inventory.json');tests=inventory['runtime_test_programs']+inventory['subprocess_scenarios']+inventory['tooling_checks']
  need(inventory['mode']=='full' and len(tests)==73 and sorted(v['junit_names'])==sorted(tests),'actual73 inventory differs')
  for name,value in q['source_members'].items():need(fid(source/name)=={'bytes':value['bytes'],'sha256':value['candidate_sha256']},'combined full source changed '+mode+'/'+name)
  need(v['production_files']==prod and v['inventory_identity']==fid(build/'test-inventory.json') and v['junit_identity']==fid(base/'ctest.xml') and v['compile_commands_identity']==fid(build/'compile_commands.json'),'full actual files/production changed')
  cases={c.get('name'):c for c in ET.parse(base/'ctest.xml').getroot().iter('testcase')}
  database=read(build/'compile_commands.json');ctestpath=build/'tests/CTestTestfile.cmake';ctest=ctestpath.read_text(encoding='utf-8');rows={}
  for name in names:
   need(name in tests and name in cases,'targeted missing from real full: '+mode+'/'+name);case=cases[name]
   need(case.get('status')=='run' and all(case.find(t) is None for t in ('failure','error','skipped')),'targeted failed/skipped '+name)
   src=source/'tests'/(name+'.cpp');records=[x for x in database if Path(x['file'])==src]
   need(len(records)==1,'targeted compileDB inventory differs: '+name)
   row=records[0];argv=row.get('arguments') or shlex.split(row['command'])
   need(Path(argv[0])==Path(q['compiler_launcher']['path']),'targeted compiler differs '+name)
   for flag in ('-O3','-DNDEBUG','-UNDEBUG','-freflection','-fexpansion-statements','-stdlib=libc++','-pthread'):need(flag in argv,'targeted missing flag '+name+': '+flag)
   dialects=[x for x in argv if x.startswith('-std=')];need(dialects and dialects[-1] in ('-std=c++26','-std=gnu++26'),'targeted not original C++26 '+name)
   need(argv.index('-UNDEBUG')>argv.index('-DNDEBUG') and '-fno-exceptions' not in argv,'assertion/exception scope differs '+name)
   san=[x for x in argv if x.startswith('-fsanitize=')]
   if mode=='sanitized':
    need(san==['-fsanitize=address,undefined'] and '-fno-omit-frame-pointer' in argv and '-resource-dir=/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/sanitizer-resource' in argv,'targeted SAN scope differs '+name)
   else:need(not san,'normal unexpectedly sanitized '+name)
   token=r'(?:\[=\['+re.escape(name)+r'\]=\]|'+re.escape(name)+r')(?=\s)'
   lines=[x for x in ctest.splitlines() if re.match(r'add_test\('+token,x)]
   need(len(lines)==1 and re.findall(r'"([^"]*)"',lines[0])==[str(build/'bin/Release'/name)],'targeted CTest command is not original Release no-argument case '+name)
   props=[x for x in ctest.splitlines() if re.match(r'set_tests_properties\('+token,x)]
   need(not any(re.search(r'\b(?:DISABLED|WILL_FAIL)\s+(?:TRUE|ON|1)\b',x) for x in props),'targeted disabled/expected-failure override '+name)
   rows[name]={'fixture':ref(src),'compile_argv':argv,'CTest_add':lines[0],'CTest_properties':props,'normal_SAN_assertions_and_failpoint_source_preserved':True,
     'dialect_scope':'Original project CMake supported C++26 policy; effective '+dialects[-1]+'. Manual targeted requests used gnu++26; actual full project adds c++26. Both retain P2996, O3, assertions and identical fixture/hook source. This explicit scope is for independent review, not byte-identical argv claim.'}
  need(v['environment']['ASAN_OPTIONS']=='detect_leaks=1:halt_on_error=1:allocator_may_return_null=1' and v['environment']['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1','full failpoint/SAN environment differs')
  coverage[mode]=rows;checked[mode]={'receipt':ref(path),'request':ref(req),'junit':ref(base/'ctest.xml'),'inventory':ref(build/'test-inventory.json'),'compileDB':ref(build/'compile_commands.json'),'CTestfile':ref(ctestpath)}
 ep=exact(correction['corrective_external_request']);eq=read(ep);external=Path(correction['corrective_external_receipt']);ev=passed(external)
 original_external=read(exact(q['external_request']))
 proof=read(exact(correction['validator_snapshot_equivalence']))
 need(proof['status']=='passed_source_equivalence_only' and proof['candidate_and_snapshot154_exact_equal'] and proof['production14']==prod and eq['source']==proof['verified_linux_snapshot'],'corrective provider equivalence differs')
 expected_source={n:{'bytes':v['bytes'],'sha256':v['candidate_sha256']} for n,v in q['source_members'].items()}
 need(proof['source154']==expected_source,'corrective full154 differs')
 core_proof=read(exact(correction['core_snapshot_equivalence']));need(core_proof['source154']==expected_source and core_proof['corrective_source']==eq['source'],'core provider proof differs')
 need(eq['source_members']==q['source_members'] and eq['candidate']==q['candidate'],'corrective candidate differs')
 for oldjob,newjob in zip(original_external['commands'],eq['commands']):
  want=[a.replace(original_external['source'],eq['source']).replace(original_external['output'],eq['output']) for a in oldjob['argv']]
  need(newjob['argv']==want and all(newjob[k]==oldjob[k] for k in ('kind','label','timeout_seconds')),'corrective flags/fixture/standard changed')
  if newjob['kind']=='compile':
   includes=[a[2:] for a in newjob['argv'] if a.startswith('-I')]
   need(len(includes)==13 and all(p.startswith('/root/') and not (Path(p)/'version').exists() for p in includes),'case-insensitive project include or shadowing version remains')
 failure=read(exact(correction['preserved_failure']))
 need(failure['compile_attempts']==1 and failure['successful_compiles']==failure['native_attempts']==0,'historical attempt accounting differs')
 failed_receipt=read(exact(failure['receipt']));need(failed_receipt['status']=='failed' and len(failed_receipt['commands'])==1 and failed_receipt['commands'][0]['kind']=='compile' and failed_receipt['commands'][0]['returncode']!=0,'failed original receipt differs')
 need(ev['request']['path']==str(ep) and {k:ev['request'][k] for k in ('bytes','sha256')}==fid(ep),'external actual request differs')
 need(ev['candidate']==q['candidate'] and len(ev['commands'])==len(eq['commands'])==8,'external inventory differs')
 for actual,job in zip(ev['commands'],eq['commands']):need(actual['label']==job['label'] and actual['argv']==job['argv'] and actual['kind']==job['kind'] and actual['returncode']==0,'external command differed/failed')
 for name,v in eq['source_members'].items():need(fid(Path(eq['source'])/name)=={'bytes':v['bytes'],'sha256':v['candidate_sha256']},'external combined source changed')
 return {'status':'passed','candidate':q['candidate'],'scope':'Actual combined full73x2 plus six repository targeted source/assertions/SAN/CTest scopes and two external fixtures each normal/SAN; no independent candidate substitutions',
  'initial_request':ref(Q),'production_files':prod,'full':ref(D/'COMBINED_FULL_RESULT.json'),'full_closure':checked,'repository_targeted':coverage,'repository_programs_each_mode':6,
  'external':ref(external),'external_request':ref(ep),'corrective_addendum':ref(CORRECTIVE),'preserved_external_v1_failure':correction['preserved_failure'],'provider_equivalence':correction['validator_snapshot_equivalence'],'historical_failed_compile_attempts':1,'historical_failed_native_attempts':0,'external_programs':4,'unique_targeted_sources':8,'performance_accepted':False,'QF_admitted':False}
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();v=verify()
 with a.out.open('x',encoding='utf-8') as f:json.dump(v,f,indent=2);f.write('\n')
