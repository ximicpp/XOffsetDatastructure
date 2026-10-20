"""Finite read-only C14/environment inventory. No C++ execution or compilation."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
OLD = Path('/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发')
PRIOR = Path('/root/xoffset-receive-completion/20261019/v1')
SOURCE = PRIOR / 'final-v4/source'
P1 = Path('/root/xoffset-serialization-comparison/20261017/p1-v1')
OUT = ROOT / 'docs/coordination/c14-iteration-20261019-v1/validation'

def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8-sig'))

def fid(p):
    p = Path(p)
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''):
            h.update(b)
    return {'bytes': p.stat().st_size, 'sha256': h.hexdigest()}

def capture(argv):
    r = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=30)
    return {'argv': list(map(str, argv)), 'returncode': r.returncode,
            'stdout': r.stdout.strip(), 'stderr': r.stderr.strip()}

def dependencies(source, lock):
    trees = {}
    for n, expected in lock['include_trees'].items():
        p = source / n
        files = sorted((x for x in p.rglob('*') if x.is_file()), key=lambda x:x.relative_to(p).as_posix())
        records = ''.join(x.relative_to(p).as_posix() + ' ' + fid(x)['sha256'] + '\n' for x in files)
        actual = hashlib.sha256(records.encode()).hexdigest()
        trees[n] = {'files': len(files), 'sha256': actual, 'expected': expected, 'matches': actual == expected}
    licenses = {n: {'actual': fid(source/n)['sha256'], 'expected': expected}
                for n, expected in lock['licenses'].items()}
    return {'provider': str(source), 'trees': trees, 'licenses': licenses,
            'passed': all(x['matches'] for x in trees.values()) and
                      all(x['actual'] == x['expected'] for x in licenses.values())}

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    material = read(ROOT/'handoff/github-20261019/C14_MATERIALIZATION.json')
    members = material['non_external_files']
    actual = {n: fid(ROOT/n) for n in members}
    prod_names = list(material['production_files'])
    prod = {n: actual[n] for n in prod_names}
    prior_mismatches = [n for n in members if not (SOURCE/n).is_file() or fid(SOURCE/n) != members[n]]
    lock = read(ROOT/'SOURCE_MANIFEST.json')['dependency_lock']
    tcpath = P1/'tools/p2996-profile/toolchain.json'
    tc = read(tcpath)
    tcfiles = {n: {'expected': expected, 'actual': fid(n)['sha256']}
               for n, expected in tc['files'].items()}
    # Limit sanitizer inventory to the actual compatible resource, not SDK discovery.
    sanroot = Path(tc['sanitizer_resource'])
    sanfiles = {p.relative_to(sanroot).as_posix(): fid(p) for p in sorted(sanroot.rglob('*')) if p.is_file()}
    profiles = {}
    for mode in ('normal','sanitized'):
        b = PRIOR/f'full-final-v4-{mode}-build'
        logs = PRIOR/f'full-final-v4-{mode}-logs'
        accept = read(logs/'inventory-acceptance.json')
        junit = logs/'ctest.xml'
        cases = list(ET.parse(junit).getroot().iter('testcase'))
        commands = read(b/'compile_commands.json')
        profiles[mode] = {
            'acceptance': {'path': str(logs/'inventory-acceptance.json'), **fid(logs/'inventory-acceptance.json')},
            'source_production_matches': accept['production_files'] == prod,
            'inventory_count': accept['ctest_count'], 'junit_identity': fid(junit),
            'junit_matches_inventory': sorted(c.get('name') for c in cases) == sorted(accept['junit_names']),
            'junit_passed': all(c.get('status') == 'run' and all(c.find(t) is None for t in ('failure','error','skipped')) for c in cases),
            'configure_argv': read(logs/'configure.request.json')['argv'],
            'compile_command_example': commands[0],
            'compiler_cache': fid(b/'CMakeCache.txt'),
            'status': accept['status']}
    spec = importlib.util.spec_from_file_location('existing_global_run_job', OLD/'experiments/vm_coordination/run_job.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    lease = runner.status(OLD/'build/vm-coordination')
    dep = dependencies(SOURCE, lock)
    windows = read(OUT/'windows-host.json')
    gitlinks = windows['gitlinks']
    gitlinks_ok = all(any(line.startswith('160000 '+sha+' ') and line.endswith('\t'+n) for line in gitlinks) for n,sha in material['gitlinks'].items())
    version = capture([P1/'tools/p2996-profile/bin/clang++','--version'])
    cmake = capture([P1/'tools/cmake/bin/cmake','--version'])
    checks = {'root_154_matches': actual == members, 'production_14_matches': all(prod[n] == members[n] for n in prod),
              'gitlinks_match': gitlinks_ok, 'dependency_content_lock': dep['passed'],
              'pinned_toolchain_files_match': all(x['expected']==x['actual'] for x in tcfiles.values()),
              'compiler_version_matches': version['returncode']==0 and '9ffb96e3ce362289008e14ad2a79a249f58aa90a' in version['stdout'],
              'cmake_available': cmake['returncode']==0,
              'full_history_source_profile_inventory_match': set(prior_mismatches).issubset({'SOURCE_MANIFEST.json'}) and all(x['source_production_matches'] and x['junit_matches_inventory'] and x['junit_passed'] for x in profiles.values())}
    evidence_names = ['final-acceptance-v6.json','acceptance-checkpoint-v5.json','ACCEPTANCE_20261019.md']
    evidence = {n: {'path':str(OLD/'docs/coordination/receive-completion-20261019'/n), **fid(OLD/'docs/coordination/receive-completion-20261019'/n)} for n in evidence_names}
    result = {'schema_version':1, 'stage':'S0', 'status':'passed' if all(checks.values()) else 'blocked',
              'observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'checks':checks, 'baseline_commit':windows['head'], 'windows_host':windows,
              'root':str(ROOT), 'root_members_count':len(members), 'production_files':prod,
              'root_member_mismatches':[n for n in members if members[n]!=actual[n]],
              'prior_source_154_mismatches':prior_mismatches, 'gitlinks':gitlinks,
              'prior_manifest_difference':'final-v4/source SOURCE_MANIFEST is pre-package metadata; root SOURCE_MANIFEST is final source ZIP manifest. All 153 code/build/test/doc members and all 14 production files match.',
              'dependencies':dep, 'worktree_external_populated':(ROOT/'external/boost/LICENSE_1_0.txt').exists(),
              'dependency_plan':'Copy locked vendored files into each fresh validation snapshot; retain empty worktree gitlinks.',
              'host':{'platform':platform.platform(),'uname':list(os.uname()),'cpu':capture(['lscpu']),
                      'memory':Path('/proc/meminfo').read_text(), 'wsl':True, 'native_linux_isa':True},
              'toolchain':{'record':str(tcpath),'record_identity':fid(tcpath),'files':tcfiles,'version':version,
                           'cmake':cmake,'sanitizer_files':sanfiles,
                           'abi_conclusion':'Pinned loader/glibc/libc++/libc++abi byte identities match retained fixed profile; no new compile/link/run probe in S0.'},
              'historical_full':profiles, 'evidence':evidence,'lease':lease,
              'disk':{p:dict(zip(('total','used','free'),shutil.disk_usage(p))) for p in ('/root','/mnt/c','/mnt/d') if Path(p).exists()},
              'output_budget':{'initial_validation_reservation_bytes':8*1024**3,'output':'/root/xoffset-c14-iteration/20261019/v1/validation',
                               'formal_capture_admission':False,'note':'WSL logical free is not host backing capacity. Q/F and 72-pair screens require separate artifact/backing projection.'},
              'reuse':['Same C14 154-member source and 14 production files: retained WSL full normal73/SAN73, existing business/V1/lifecycle/package acceptance scoped to its recorded binaries/profile.',
                       'Existing fixed toolchain and vendored dependency content; no rebuild for path migration.'],
              'must_do':['Each changed candidate: new snapshot, exact input/tool binding, affected normal/SAN, bare -O3 -DNDEBUG codegen and one fixed-M mechanism observation.',
                         'Before screening: applicable complete business/V1/consumer gates and separately frozen screening policy/admission.',
                         'Windows native storage and full are separate untested C14 platforms; no WSL substitution.'],
              'new_cpp_compiles':0,'new_cpp_executions':0,'new_timing_samples':0,'platform_test_executed_here':False,
              'no_process_pause_or_power_actions':True,'formal_goal_complete':False}
    path = OUT/'S0_RESULT.json'
    with path.open('x',encoding='utf-8') as f:
        json.dump(result,f,indent=2,ensure_ascii=False); f.write('\n')
    print(json.dumps({'status':result['status'],'checks':checks,'output':str(path),'lease':lease},ensure_ascii=True))
    return 0 if result['status']=='passed' else 1

if __name__ == '__main__':
    sys.exit(main())
