"""Read-only current Windows identities and power; never start a pause guard."""
import datetime
import json
import os
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H / 'm2'))
import execute as E


def main():
    holder = E.S.C.read(E.S.LOCK / 'queue.lock')
    expected = '/root/xoffset-c14-iteration/20261020/v2/validation/m2-screen-host-observe-v1-lease'
    E.S.need(holder['status'] == 'running' and holder['owner'] == 'W1' and holder['kind'] == 'validation' and holder['out'] == expected and holder['supervisor_pid'] == os.getppid(), 'own original read-only observation lease required')
    E.HD.mkdir(parents=True, exist_ok=True)
    plan = E.P.prepare_plan(E.HD / 'pre-review-process-plan-v1.json')
    cfg = E.S.C.read(E.D / 'config.json')
    power = E.power(cfg['host_policy']['power_guid'])
    E.S.write(E.HD / 'HOST_OBSERVATION.json', dict(status='read_only_current_host_observed', plan=plan, power=power,
        observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='microseconds'), original_lease=holder,
        tool=E.ref(__file__), controller=E.ref(H / 'm2/execute.py'), pause_module=E.ref(H / 'm2/host/process_pause.py'),
        helper=E.ref(H / 'm2/host/pause-control.ps1'), paused_processes=0, guard_started=False, power_changed=False,
        fresh_plan_still_required_at_prelude=True, screen_admitted=False))
    print(json.dumps(dict(status='read_only_host_observed', plan=plan, power_guid=power['guid'], paused_processes=0)))


if __name__ == '__main__':
    main()
