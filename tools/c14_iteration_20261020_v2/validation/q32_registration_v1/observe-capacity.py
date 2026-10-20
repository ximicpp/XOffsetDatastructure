"""Fresh read-only dual-volume observation; original observation checks retained.

Derived from formal-registration-v4/author/observe-capacity.py. Imports are
limited to the standard library and B is the current existing Linux work area.
The adjacent Windows helper is byte-identical to the original helper.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, os, shutil, subprocess

B = Path('/root/xoffset-c14-iteration/20261020/v2/validation')

def need(value, message):
    if not value:
        raise ValueError(message)

def fid(path):
    value = Path(path).read_bytes()
    return {'bytes': str(len(value)), 'sha256': hashlib.sha256(value).hexdigest()}

def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def read(path):
    return json.loads(Path(path).read_bytes())

def write(path, value):
    with Path(path).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')

def fresh_observation(record):
    moment = datetime.datetime.fromisoformat(record['captured_at'].replace('Z', '+00:00'))
    age = (datetime.datetime.now(datetime.timezone.utc) - moment).total_seconds()
    need(0 <= age <= 600, 'fresh observation within 600 seconds required')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    need(os.name == 'posix' and str(args.out).startswith('/mnt/c/'), 'new Windows-backed observation directory required')
    args.out.mkdir(exist_ok=False)
    helper = Path(__file__).resolve().with_name('observe-backing.ps1')
    destination = args.out / 'windows.json'
    windows_path = 'C:\\' + str(destination.relative_to('/mnt/c')).replace('/', '\\')
    argv = ['/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',
        '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File',
        'C:\\' + str(helper.relative_to('/mnt/c')).replace('/', '\\'), '-Out', windows_path]
    write(args.out / 'request.json', {'argv': argv, 'helper': fid(helper), 'created_at': utc()})
    with (args.out / 'stdout').open('xb') as stdout, (args.out / 'stderr').open('xb') as stderr:
        result = subprocess.run(argv, stdout=stdout, stderr=stderr, timeout=30, check=False)
    write(args.out / 'receipt.json', {'returncode': result.returncode, 'finished_at': utc()})
    need(result.returncode == 0, 'capacity probe failed; preserve output')
    snapshot = read(destination)
    fresh_observation(snapshot)
    need(snapshot['distribution'] == 'Ubuntu-22.04' and snapshot['windows_backing_root'] == 'C:\\' and
         snapshot['linux_backing_probe'] == '/mnt/c' and snapshot['read_only_inspection'] is True and
         snapshot['helper_sha256'] == fid(helper)['sha256'], 'capacity provenance differs')
    guest, backing = shutil.disk_usage(B).free, shutil.disk_usage('/mnt/c').free
    need(abs(backing - snapshot['volume_free_bytes']) <= 1024**3, 'host/backing probes disagree')
    required = 8 * 1024**3 + 2 * 1024**3
    need(min(guest, backing, snapshot['volume_free_bytes']) >= required, 'fresh F worst-cap plus margin unavailable')
    write(args.out / 'verification.json', {'status': 'passed', 'captured_at': utc(),
        'windows_snapshot': fid(destination), 'guest_free_bytes': guest,
        'backing_free_bytes': backing, 'delta_tolerance_bytes': 1024**3,
        'initial_minimum_required_bytes': required,
        'observer': fid(Path(__file__)), 'windows_helper': fid(helper),
        'scope': 'Fresh dual-volume observation, not a promise of future free capacity. Freeze and every actual child must separately recheck caps/peak/guest/backing admission.'})

if __name__ == '__main__':
    main()
