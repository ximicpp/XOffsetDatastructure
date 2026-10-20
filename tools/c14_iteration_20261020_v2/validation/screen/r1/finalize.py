"""Read-only evidence finalizer. No native, queue, host control, retry or Q/F."""
import argparse
from pathlib import Path
import screen as S
D=S.ROOT/'docs/coordination/c14-iteration-20261020-v2/validation/screen/R1'
OUT=Path('/root/xoffset-c14-iteration/20261020/v2/validation/R1-screen-20261020-v2-v1')
def main():
    p=argparse.ArgumentParser();p.add_argument('--freeze-sha256',required=True);p.add_argument('--independent-review',type=Path,required=True);p.add_argument('--result',type=Path,required=True);a=p.parse_args()
    f=S.check_freeze(OUT/'freeze.json',a.freeze_sha256);r=S.C.read(D/'RESTORED.json');S.need(r['status']=='passed' and r['user_processes_restored'] is True and r['freeze_sha256']==a.freeze_sha256,'actual restoration required')
    report=S.C.read(D/'AUDIT.json') if (D/'AUDIT.json').exists() else S.C.read(D/'FAILURE.json');review=S.C.read(a.independent_review)
    S.need(review['kind']=='v2-screen-final-independent-review' and review['status']=='passed' and review['freeze_sha256']==a.freeze_sha256 and review['candidate']=='r1-canonical-hotcold-v1','exact independent final review required')
    auditpath=D/('AUDIT.json' if (D/'AUDIT.json').exists() else 'FAILURE.json');S.need(S.checked_file(review['audit'])==auditpath,'independent audit reference differs')
    for evidence in review['evidence']:S.checked_file(evidence)
    complete=report['status']=='passed';eligible=False
    if complete:
        coverage=D/'host/TIMING_PAUSE_COVERAGE.json';S.need(S.C.read(coverage)['status']=='passed' and S.checked_file(review['pause_coverage'])==coverage,'actual fresh pause interval coverage required')
        summary=report['summary'];S.need(review['recalculated_summary']==summary and review['statistics_recalculated_from_raw144'] is True and review['engineering_gates_passed'] is True,'independent raw144 statistics and engineering review required')
        eligible=summary['statistically_eligible'] and not summary['undefined_metrics']
    else:S.need(report.get('summary') is None and review['performance_inference_from_prefix'] is False,'failed window has no summary/inference/replacement')
    S.write(a.result,dict(kind='v2-screen-final-disposition',candidate='r1-canonical-hotcold-v1',freeze_sha256=a.freeze_sha256,status='statistically_and_engineering_eligible_pending_parent_promotion' if eligible else 'closed_not_promoted',audit=dict(path=str(auditpath),identity=S.fid(auditpath)),independent_review=dict(path=str(a.independent_review),identity=S.fid(a.independent_review)),complete_144=complete,performance_accepted=False,QF_admitted=False,retry=0,replacement=0,summary=report.get('summary') if complete else None))
if __name__=='__main__':main()
