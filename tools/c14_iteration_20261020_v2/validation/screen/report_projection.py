"""Read an already closed finite capture and report its conservative projection.

No native, compile, queue, host operation or new observation is performed.
The final frozen worker recomputes the same projection itself.
"""
from pathlib import Path
import argparse,json,sys
H=Path(__file__).resolve().parent
def main():
    p=argparse.ArgumentParser();p.add_argument('candidate',choices=('r1','m2'));p.add_argument('--result',type=Path,required=True);a=p.parse_args()
    sys.path.insert(0,str(H/a.candidate));import screen as S
    D=S.ROOT/f'docs/coordination/c14-iteration-20261020-v2/validation/screen/{a.candidate.upper()}'
    cfg=S.C.read(D/'config.json');directory=Path(f'/root/xoffset-c14-iteration/20261020/v2/validation/{a.candidate}-screen-resource-capture-v1')
    result=S.projection(cfg,directory)
    S.write(a.result,dict(status='projection_passed_pending_independent_screen_freeze_admission',candidate=cfg['candidate'],tool=dict(path=str(Path(__file__).resolve()),identity=S.fid(__file__)),worker=dict(path=str(H/a.candidate/'screen.py'),identity=S.fid(H/a.candidate/'screen.py')),capture_manifest=S.fid(directory/'manifest.json'),projection=result,compile=0,native=0,timing_inference=False,screen_admitted=False))
    print(json.dumps(dict(status='projection_passed_not_screen_admitted',physical_peak_bytes=result['physical_peak_bytes'],projected_online_seconds=result['projected_online_seconds'],result=str(a.result))))
if __name__=='__main__':main()
