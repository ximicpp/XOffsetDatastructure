"""Receive-completion business scope; root execution only, no queue at import."""
from pathlib import Path
import sys, time, hashlib, json, subprocess, re, shlex
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import *

HERE = Path(__file__).resolve().parent
SOURCE = Path('/root/xoffset-c14-iteration/20261020/v2/validation/r1-full-v1-normal/source')
BENCH_META = B / 'metadata'
IDENTITY_FILE = C / 'candidate-source-identity-v2.json'
MATERIALIZATION_FILE = B / 'source-identity.json'
CANDIDATE_ID = 'xoffset-c14-r1-canonical-hotcold-v1'
BASELINE_ID = 'xoffset-four-paths-v1'
BASELINE_SOURCE = Path('/root/xoffset-receive-completion/20261019/v1/baseline/source')
# Explicit aliases: the immediate source predecessor is FOUR PATHS, while the
# four reused competitor binaries/closure remain the original sealed V5.
OLD_B = Path('/root/xoffset-performance-v5/20261018/v1')
OLD_C = R / 'docs/coordination/performance-v5-20261018'
OLD = OLD_B / 'final-v1/source'
PRIOR = Path('/root/xoffset-load-mutate-optimization/20261017/v1')
B8 = Path('/root/xoffset-performance-v8/20261018/v1')
C8 = R / 'docs/coordination/performance-v8-20261018'
V8_ACCEPTANCE_SHA256 = '8853ab2991e2e91baeced4581b83724c652693c099f316f8ac959251c9a27aaf'
RETAINED_SEALS = {
    str(R / 'docs/coordination/performance-v3-20261017/final-acceptance.json'):
        {'bytes': 5007, 'sha256': '69edee94a7a6dc605636c764cab27b26e37c196eaa62d9571b7d1e81aceea00c'},
    str(R / 'docs/coordination/performance-v4-20261018/final-acceptance.json'):
        {'bytes': 6340, 'sha256': '89a810dddb3cc28972195395513f2352f5f557b2973490687f15dea35bea18f8'},
    str(R / 'docs/coordination/performance-v5-20261018/final-acceptance.json'):
        {'bytes': 8323, 'sha256': 'd1d66631ec305df1685f345c57e73c4e3d2df72bb2c6acf73dee5de6ac7ac448'},
}

def require(value, message):
    if not value:
        raise RuntimeError(message)

def input_path():
    positions = [i for i,x in enumerate(sys.argv) if x == '--inputs']
    require(len(positions) <= 1, 'Duplicate --inputs')
    raw = sys.argv[positions[0]+1] if positions and positions[0]+1<len(sys.argv) else os.environ.get('XOFFSET_BUSINESS_INPUTS')
    require(raw is not None, 'Root-created --inputs absolute JSON is required')
    value = Path(raw)
    require(value.is_absolute() and value.is_file() and not value.is_symlink(), 'Regular absolute business input record required')
    return value

INPUTS_PATH = input_path()
INPUTS_ID = fid(INPUTS_PATH)
INPUTS = read(INPUTS_PATH)
require(type(INPUTS) is dict and set(INPUTS) == {'schema_version','kind','source','source_identity','full_profiles'}, 'Exact business input keys required')
require(type(INPUTS['schema_version']) is int and INPUTS['schema_version']==1 and INPUTS['kind']=='receive-completion-business-inputs' and
        INPUTS['source']==str(SOURCE) and set(INPUTS['full_profiles'])=={'normal','sanitized'}, 'Business input scope differs')

def match_ref(record):
    require(type(record) is dict and set(record)=={'path','identity'}, 'Exact file reference required')
    path=Path(record['path'])
    require(path.is_absolute() and path.is_file() and not path.is_symlink() and
            fid(path)==record['identity'], 'Referenced identity differs: '+str(path))
    return path

require(match_ref(INPUTS['source_identity'])==IDENTITY_FILE, 'Root coordination identity path differs')
_root_env, _root_queue = env, queue
def env(sanitized=False):
    result=_root_env(sanitized)
    result['XOFFSET_BUSINESS_INPUTS']=str(INPUTS_PATH)
    return result

def queue(label,script,args=(),timeout=2100,kind='validation'):
    # Existing global runner/lease remains authoritative. Scope args only go to
    # our scripts, never to the byte-exact original campaign audit CLI.
    script=Path(script)
    values=list(args)
    if script.parent==HERE:
        values=['--inputs',str(INPUTS_PATH),*values]
    return _root_queue(label,script,values,timeout=timeout,kind=kind)

def require_final_identity():
    prepared=read(C/'PREPARED.json')
    require(prepared['status']=='prepared', 'Current R1 prepared bindings required')
    for path,value in prepared['bindings'].items():
        require(fid(Path(path))==value, 'Frozen R1 source/tool/input changed: '+path)
    require(fid(INPUTS_PATH)==INPUTS_ID, 'Business input record changed')
    require(match_ref(INPUTS['source_identity'])==IDENTITY_FILE, 'Candidate identity reference changed')
    identity=read(IDENTITY_FILE)
    require(identity['candidate_id']==CANDIDATE_ID and identity['baseline_candidate_id']==BASELINE_ID,
            'Require actual receive-completion candidate on four-path baseline')
    names=identity['production_files']
    require(type(names) is dict and {'xoffsetdatastructure.hpp','xoffset_virtual_memory.hpp',
            'xoffset_verified_receive.hpp','receive_raw.hpp'} <= set(names), 'Complete received production file record required')
    require(all(type(n) is str and n and Path(n).name==n and not Path(n).is_absolute() for n in names), 'Unsafe production file name')
    actual={name:fid(SOURCE/name) for name in sorted(names)}
    require(actual==names, 'Actual dynamic production files differ')
    digest=hashlib.sha256(json.dumps(actual,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    require(digest==identity['source_content_sha256'], 'Production content digest differs')
    materialized=read(MATERIALIZATION_FILE)
    require(materialized['source']==str(SOURCE) and materialized['identity']==identity,
            'Materialization and coordination identity differ')
    expected=materialized['full_expected_ctest']
    require(type(expected) is int and expected>0, 'Actual full_expected_ctest required')
    return identity

def require_baseline_identity():
    record=read(R/'docs/coordination/receive-completion-20261019/baseline.json')
    identity=record['identity']
    require(record['source']==str(BASELINE_SOURCE) and identity['candidate_id']==BASELINE_ID,
            'Immediate baseline source is not four paths')
    require({n:fid(BASELINE_SOURCE/n) for n in identity['production_files']}==identity['production_files'],
            'Immediate baseline production changed')
    return identity

def full_record(kind):
    require(kind in ('normal','sanitized'), 'Invalid full profile kind')
    identity=require_final_identity(); record=INPUTS['full_profiles'][kind]
    path=match_ref(record['acceptance']); value=read(path)
    receipt_path=Path(value['receipt']['path']); receipt=read(receipt_path)
    require(fid(receipt_path)==value['receipt']['identity'] and receipt['status']=='passed', 'Full receipt differs')
    require(value['source']==str(SOURCE) and value['production_files']==identity['production_files'], 'Full source identity differs')
    require(value['ctest_count']==73 and len(receipt['junit_names'])==73 and len(set(receipt['junit_names']))==73, 'Full73 inventory required')
    require(receipt['production_files']==identity['production_files'], 'Full production closure differs')
    require(Path(record['build'])==receipt_path.parent/'build', 'Full build path differs')
    for n,v in identity['production_files'].items(): require(fid(receipt_path.parent/'source'/n)==v, 'Full snapshot differs: '+n)
    require(fid(Path(record['build'])/'compile_commands.json')==receipt['compile_commands_identity'], 'Full compile database differs')
    return record,value

def full_build(kind='normal'):
    return Path(full_record(kind)[0]['build'])

def test_binary(name,kind='normal'):
    build=full_build(kind)
    metadata=build/'tests/CTestTestfile.cmake'
    matches=[]
    for line in metadata.read_text().splitlines():
        found=re.fullmatch(r'add_test\('+re.escape(name)+r' "([^"\r\n]+)"\)',line)
        if found:matches.append(Path(found.group(1)))
    require(len(matches)==1 and matches[0].is_absolute() and matches[0].resolve().is_relative_to(build.resolve()) and
            matches[0].is_file() and not matches[0].is_symlink(), 'Actual registered test binary missing/ambiguous: '+name)
    return matches[0]

def sealed_v8(kind):
    require(kind in ('normal','sanitized'), 'Invalid V8 profile kind')
    acceptance=C8/'final-acceptance.json'
    require(fid(acceptance)['sha256']==V8_ACCEPTANCE_SHA256, 'V8 seal differs')
    sealed=read(acceptance)
    require(sealed['execution_status']=='completed' and sealed['functional_acceptance'] is True, 'V8 functional control not accepted')
    external=C8/('functional-'+kind+'-external-pin.json')
    require(fid(external)==sealed['evidence'][kind]['business_external_pin'], 'V8 external pin differs')
    pin=read(external);bundle=B8/('functional-'+kind+'-v1')
    require(fid(bundle/'manifest.json')==pin['manifest'] and fid(bundle/'queue/manifest.json')==pin['queue_manifest'], 'V8 manifests differ')
    require(hashlib.sha256(b'SC-POST-RUN-PIN-v1\0'+bytes.fromhex(pin['manifest']['sha256'])+
            bytes.fromhex(pin['queue_manifest']['sha256'])).hexdigest()==pin['composite_pin'], 'V8 composite pin differs')
    return bundle,read(bundle/'manifest.json')['files'],{'acceptance':fid(acceptance),'external_pin':fid(external),
            'manifest':fid(bundle/'manifest.json'),'queue_manifest':fid(bundle/'queue/manifest.json')}

def run(out,name,args,timeout=180,environ=None):
    out=Path(out);out.mkdir(parents=True,exist_ok=True);args=list(map(str,args))
    write(out/(name+'.request.json'),{'argv':args});started=time.monotonic()
    with (out/(name+'.log')).open('xb') as stream:
        result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout,env=environ or env())
    write(out/(name+'.result.json'),{'returncode':result.returncode,'seconds':time.monotonic()-started})
    require(result.returncode==0,'Preserved failure: '+str(out/(name+'.log')))
    return result
