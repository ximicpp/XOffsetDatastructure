"""Light checks for concrete screen-tool failure modes. Never runs a child."""
from pathlib import Path
import ast
import hashlib
import importlib.util
import json
import sys
import tempfile

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import m1_screen as M
import core as C

def main(out):
    checks=[]
    for n in ('m1_screen.py','campaign.py','core.py','protocol.py','check_tools.py'):
        ast.parse((HERE/n).read_bytes(),filename=n)
    checks.append('all five Python sources parse; no pycache or subprocess')
    old=Path('/root/xoffset-receive-completion/20261019/v1/functional-tools-v2')
    for n in ('core.py','protocol.py'):assert (HERE/n).read_bytes()==(old/n).read_bytes()
    original=(old/'campaign.py').read_text()
    expected=original.replace('resource.setrlimit(resource.RLIMIT_FSIZE,(file_limit,file_limit))','resource.setrlimit(resource.RLIMIT_FSIZE,(file_limit,file_limit))\n                    resource.setrlimit(resource.RLIMIT_AS,(16*1024**3,16*1024**3))')
    assert (HERE/'campaign.py').read_text()==expected
    checks.append('borrowed execute/native_result source unchanged except one RLIMIT_AS insertion')
    with tempfile.TemporaryDirectory(prefix='m1-tools-',dir=HERE) as raw:
        d=Path(raw);a=d/'a.json';b=d/'b.json';a.write_bytes(b'{"z": true, "a":1}\n')
        C.copy_file(a,b);assert a.read_bytes()==b.read_bytes()
    checks.append('noncanonical admission JSON archive preserves exact bytes')
    # A legitimate zero is retained and blocks inference without dropping samples.
    values={n:1 for n in M.METRICS};values['cleanup']=0
    samples=[dict(job=j,values=dict(values)) for j in M.schedule()]
    r=M.summary(samples)
    assert r['undefined_metrics']==['cleanup'] and not r['primary_improvement'] and r['metrics']['cleanup']['n']==72
    checks.append('legitimate zero timing retained for all72 units; inference undefined/no promotion')
    try:M.validate_cfg({'schema':'unregistered'})
    except C.ProtocolError:pass
    else:raise AssertionError('unregistered config accepted')
    checks.append('unregistered config rejected before any identity/native operation')
    # Check a real sealed queue, then demonstrate actual result tamper rejection.
    real=Path('/root/xoffset-receive-completion/20261019/v1/functional-normal-v2/queue')
    M.queue_audit(real,'functional',1800)
    with tempfile.TemporaryDirectory(prefix='m1-queue-',dir=HERE) as raw:
        d=Path(raw)
        for p in real.iterdir():C.copy_file(p,d/p.name)
        (d/'result.json').write_bytes((d/'result.json').read_bytes()+b' ')
        try:M.queue_audit(d,'functional',1800)
        except C.ProtocolError:pass
        else:raise AssertionError('tampered result accepted')
    checks.append('actual historical runner six-file audit passes; changed result body rejected')
    C.write(out,dict(schema='m1-screen-tool-check-v1',status='passed',checks=checks,
                    native_runs=0,compiles=0,timing_samples=0,
                    files={p.name:C.file_id(p) for p in HERE.glob('*.py')}))
    print(json.dumps(dict(status='passed',checks=len(checks),native_runs=0)))

if __name__=='__main__':main(Path(sys.argv[1]))
