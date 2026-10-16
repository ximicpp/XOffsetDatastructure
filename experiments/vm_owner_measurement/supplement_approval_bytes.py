#!/usr/bin/env python3
"""D031: preserve D030 evidence and audit one declared approval-byte repair.

This offline supplement never starts a native process or builds a harness.
Only the package command writes evidence, under W3's default validation queue.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import types

sys.dont_write_bytecode = True
AUTHORITY_REL = 'docs/coordination/vm-followup/contracts/owner-measurement-v1/approval-copy-supplement.json'
AUTHORITY_SHA = 'ea768e0857471588ff103002e7ccd545e8b501842836f9850d6bf80625f5afdb'
ACCEPTANCE_REL = 'docs/coordination/vm-followup/contracts/owner-measurement-v1/proposal-acceptance.json'
ACCEPTANCE_SHA = '33d65123ace553c6306cf9904e1c1416f8d8ff0fd101346db7794825a69a3fad'
APPROVAL_SHA = '0157b0d310d7388729f51a4031c6ca552ca32f9d15982972d7e0c5aebe1d9e12'
COLLECTOR_SHA = '8ab7a0dbc2f8e5e879fd3b84f74598ffd4e84e1c2f06bac9f6b3fc89003c9214'
AUDITOR_SHA = '1711b558bd75376bd4ef6654427a9b0b0519bc94b288b211087bfdf1b132fced'
PROPOSAL_SHA = '8dde7bf6e5536704dc8921de4507564499e8a4e208a4fe0606fe599596268b9a'
TIMING_MANIFEST_SHA = 'f8f9f7fbe1494fc8f0806a4fdc0f6382a8c90b87b01f5aea4f221d939878ae2f'
FAILED_MANIFEST_SHA = '1bd4e6e4039c3b45e3a5a75a8be0e1d35e441c36cf37934fbe2dfc74eda27e00'
ROOT_REL = 'build/vm-owner-measurement'
APPROVAL_REL = 'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json'
WINDOW_REL = ROOT_REL+'/window-v1'
TIMING_REL = ROOT_REL+'/receipts/window-v1'
FAILED_REL = ROOT_REL+'/receipts/archive-v1'
PIN_TARGETS = {
    APPROVAL_REL: 'original-approval.json',
    WINDOW_REL+'/approval.json': 'original-window/approval.json',
    WINDOW_REL+'/window.json': 'original-window/window.json',
    WINDOW_REL+'/summary.json': 'original-window/summary.json',
    TIMING_REL+'/result.json': 'timing-receipt/result.json',
    FAILED_REL+'/result.json': 'failed-archive-receipt/result.json',
    FAILED_REL+'/stderr.log': 'failed-archive-receipt/stderr.log',
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), 'missing/nonregular original evidence: '+str(path))
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1048576), b''):
            h.update(chunk)
    return h.hexdigest()


def parse(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON field: '+key)
            result[key] = value
        return result
    return json.loads(data, object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError('nonfinite JSON: '+value)))


def read(path):
    sha(path)
    return parse(Path(path).read_bytes())


def dump(value):
    return json.dumps(value, indent=2, sort_keys=True, allow_nan=False)+'\n'


def write(path, value):
    with Path(path).open('xb') as stream:
        stream.write(dump(value).encode('utf-8'))
        stream.flush()
        os.fsync(stream.fileno())


def load_module(path, name):
    module = types.ModuleType(name)
    module.__file__ = str(Path(path).resolve())
    sys.modules[name] = module
    exec(compile(Path(path).read_bytes(), str(path), 'exec'), module.__dict__)
    return module


def prove_approval(original_bytes, copied_bytes, old_dump):
    """Prove semantic identity and the exact legacy serialization defect."""
    require(type(original_bytes) is bytes and type(copied_bytes) is bytes, 'approval must be raw bytes')
    original = parse(original_bytes)
    copied = parse(copied_bytes)
    require(type(original) is dict and type(copied) is dict, 'approval must be a JSON object')
    # JSON numeric types are significant, including bool versus int.
    require(dump(original) == dump(copied), 'stored approval object differs from original approval')
    require(copied_bytes == old_dump(original).encode('utf-8'), 'stored approval is not exact legacy dump')
    require(original_bytes != copied_bytes, 'original approval has no serialization defect')
    return {'original_sha256': digest(original_bytes), 'stored_sha256': digest(copied_bytes),
            'original_bytes': len(original_bytes), 'stored_bytes': len(copied_bytes),
            'objects_equal': True, 'stored_equals_legacy_dump': True}


def tree(root):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), 'missing/nonregular original directory: '+str(root))
    directories, files = [], {}
    for path in sorted(root.rglob('*')):
        require(not path.is_symlink() and (path.is_file() or path.is_dir()), 'nonregular evidence: '+str(path))
        relative = path.relative_to(root).as_posix()
        if path.is_dir():
            directories.append(relative)
        else:
            files[relative] = {'sha256': sha(path), 'bytes': path.stat().st_size}
    return {'directories': directories, 'files': files}


def compare_windows(original, derived):
    """Require identical trees except one existing approval.json file's bytes."""
    before, after = tree(original), tree(derived)
    require(before['directories'] == after['directories'], 'original/derived directory sets differ')
    require(before['files'].keys() == after['files'].keys(), 'original/derived file sets differ')
    changed = [name for name in before['files'] if before['files'][name] != after['files'][name]]
    require(changed == ['approval.json'], 'derived window must differ only in approval.json')
    return [{'path': 'approval.json', 'original_sha256': before['files']['approval.json']['sha256'],
             'derived_sha256': after['files']['approval.json']['sha256'],
             'original_bytes': before['files']['approval.json']['bytes'],
             'derived_bytes': after['files']['approval.json']['bytes']}]


def tree_digest(value):
    return digest(dump(value).encode('utf-8'))


def authority(root):
    require(sha(root/'authority.json') == AUTHORITY_SHA, 'D031 authority identity differs')
    require(sha(root/'proposal-acceptance.json') == ACCEPTANCE_SHA, 'D030 proposal acceptance identity differs')
    decision = read(root/'authority.json')
    require(decision['decision'] == 'D031' and decision['status'] == 'offline_tool_fix_and_supplement_authorized', 'D031 scope differs')
    require(decision['limits'] == {'new_native': 0, 'new_builds': 0, 'measurement_windows': 0,
                                 'existing_evidence_budget_bytes': 536870912}, 'D031 bounds differ')
    require(set(decision['original_evidence_sha256']) == set(PIN_TARGETS), 'D031 evidence pin set differs')
    for source, target in PIN_TARGETS.items():
        require(sha(root/target) == decision['original_evidence_sha256'][source], 'pinned original differs: '+source)
    acceptance = read(root/'proposal-acceptance.json')
    require(acceptance['decision'] == 'D030' and acceptance['approval'] == {'path': APPROVAL_REL, 'sha256': APPROVAL_SHA}, 'D030 approval authority differs')
    for original_path, expected in {
        'build/vm-owner-measurement/proposal-v1/proposal.json': PROPOSAL_SHA,
        'experiments/vm_owner_measurement/collect_owner_measurement.py': COLLECTOR_SHA,
        'experiments/vm_owner_measurement/audit_owner_measurement.py': AUDITOR_SHA,
    }.items():
        require(acceptance['references_sha256'][original_path] == expected, 'D030 frozen source identity differs')
    require(sha(root/'original-approval.json') == APPROVAL_SHA, 'external D030 approval bytes differ')
    require(sha(root/'timing-receipt/manifest.json') == TIMING_MANIFEST_SHA, 'original timing receipt manifest differs')
    require(sha(root/'failed-archive-receipt/manifest.json') == FAILED_MANIFEST_SHA, 'failed archive receipt manifest differs')
    original = root/'original-window'
    require(sha(original/'proposal/proposal.json') == PROPOSAL_SHA, 'original frozen proposal differs')
    require(sha(original/'proposal/tools/collect_owner_measurement.py') == COLLECTOR_SHA, 'old frozen collector differs')
    require(sha(original/'proposal/tools/audit_owner_measurement.py') == AUDITOR_SHA, 'old frozen auditor differs')
    return load_module(original/'proposal/tools/collect_owner_measurement.py', '_d031_frozen_collector')


def audit_payload(root):
    """Audit copies only; never change, sample, retry, or reinterpret producers."""
    old = authority(root)
    require(set(path.name for path in (root/'original-window').iterdir()) == {
        'approval.json', 'coordination.json', 'window.json', 'window-marker.json',
        'summary.json', 'proposal', 'samples'}, 'original window has missing/extra top-level evidence')
    proof = prove_approval((root/'original-approval.json').read_bytes(),
                           (root/'original-window/approval.json').read_bytes(), old.dump)
    differences = compare_windows(root/'original-window', root/'derived-window')
    require((root/'derived-window/approval.json').read_bytes() == (root/'original-approval.json').read_bytes(), 'derived approval is not original bytes')
    require(read(root/'approval-proof.json') == proof and read(root/'differences.json') == differences, 'declared proof/difference manifest differs')
    require(read(root/'original-inventory.json') == tree(root/'original-window'), 'complete original inventory differs')
    # Both old functions remain unchanged, including their original approval SHA gate.
    proposal, state = old.audit_window(root/'derived-window')
    require(state['approval_sha256'] == APPROVAL_SHA, 'original state approval SHA differs')
    timing = old.verify_receipt(root/'timing-receipt', proposal, APPROVAL_SHA,
                                state['coordination'], root/'original-window/proposal/tools/queue-runner.py')
    captured = read(root/'package-coordination.json')
    request, metadata = captured['request'], captured['metadata']
    execution = proposal['execution']
    repo = Path(execution['repository'])
    command = request['command']
    require(type(command) is list and len(command) == 9, 'supplement validation command shape differs')
    destination = Path(command[6])
    require(destination.is_absolute() and destination.is_relative_to(repo/'docs/perf/vm-owner-scaling')
        and destination != repo/'docs/perf/vm-owner-scaling', 'supplement validation output is outside owned root')
    require(command == [proposal['python']['executable'], str(repo/'experiments/vm_owner_measurement/supplement_approval_bytes.py'),
        'package', '--repo', str(repo), '--out', str(destination), '--tool-sha256', sha(root/'supplement_approval_bytes.py')],
        'supplement exact validation command differs')
    require(request['owner'] == metadata['owner'] == 'W3' and request['kind'] == metadata['kind'] == 'validation'
        and request['runner_sha256'] == old.QUEUE_SHA and request['source_headers'] == old.BASELINE
        and request['lock_dir'] == str(repo/'build/vm-coordination') and request['cwd'] == str(repo)
        and 0 < request['timeout_seconds'] <= 900 and request['run_id'] == metadata['run_id']
        and request['out'] == metadata['out'] and metadata['status'] == 'running', 'supplement captured validation queue differs')
    queue = old.module(root/'original-window/proposal/tools/queue-runner.py', '_d031_failed_receipt_auditor')
    queue.audit(root/'failed-archive-receipt')
    failed_request = old.read(root/'failed-archive-receipt/request.json')
    failed = old.read(root/'failed-archive-receipt/result.json')
    require(failed_request['command'] == [proposal['python']['executable'], execution['collector'], 'archive',
        '--window', execution['window'], '--receipt', execution['queue_receipt'], '--out',
        str(Path(execution['repository'])/'docs/perf/vm-owner-scaling/measurement-v1')], 'original failed archive command differs')
    require(failed_request['owner'] == failed['owner'] == 'W3' and failed_request['kind'] == failed['kind'] == 'validation'
            and failed_request['runner_sha256'] == old.QUEUE_SHA
            and failed_request['lock_dir'] == str(Path(execution['repository'])/'build/vm-coordination')
            and failed_request['cwd'] == execution['repository']
            and failed_request['out'] == str(Path(execution['repository'])/FAILED_REL)
            and failed_request['source_headers'] == old.BASELINE
            and failed_request['timeout_seconds'] == 900
            and failed['status'] == 'failed' and failed['reason'] == 'command_exit'
            and type(failed['command_returncode']) is int and failed['command_returncode'] == 1
            and failed['cleanup']['live'] == [] and failed['cleanup']['process_groups'] == [], 'original failed archive result differs')
    require((root/'failed-archive-receipt/stderr.log').read_bytes() == b'ValueError: independent parent approval SHA mismatch\n', 'failed archive explanation differs')
    return {'approval_proof': proof, 'differences': differences, 'original_inventory_sha256': tree_digest(tree(root/'original-window')),
            'derived_inventory_sha256': tree_digest(tree(root/'derived-window')), 'timing_receipt': timing,
            'failed_archive_manifest_sha256': sha(root/'failed-archive-receipt/manifest.json'),
            'summary_sha256': sha(root/'original-window/summary.json'), 'cpp_processes': 78,
            'new_native_processes': 0, 'new_builds': 0, 'new_measurement_windows': 0}


def payload_tree(root):
    value = tree(root)
    value['files'].pop('result.json', None)
    value['files'].pop('COMPLETE', None)
    return value


def audit(root):
    root = Path(root).resolve()
    result = read(root/'result.json')
    require(result['schema'] == 'owner-measurement-approval-byte-supplement-v1' and result['decision'] == 'D031'
        and result['status'] == 'passed' and result['derived_is_original_bytes'] is False
        and result['native_reports_timing_evidence'] is False and result['measurement_authorized'] is True,
        'supplement qualification profile differs')
    require((root/'COMPLETE').read_bytes() == (sha(root/'result.json')+'\n').encode(), 'supplement completion identity differs')
    require(result['payload'] == payload_tree(root), 'supplement full inventory differs')
    require(sha(root/'supplement_approval_bytes.py') == result['tool_sha256'], 'supplement source identity differs')
    require(sha(Path(__file__)) == result['tool_sha256'], 'audit entry differs from reviewed supplement source')
    require(result['original_inventory_origin'] == 'First complete inventory created during D031 W3 validation; not an earlier D030 authority pin', 'original inventory provenance differs')
    require(result['audit'] == audit_payload(root), 'supplement recomputation differs')
    relocation = result['relocation_audit']
    require(relocation == {'status': 'passed', 'scope': 'complete package including result.json, COMPLETE, both windows, all frozen tools and both full receipts',
        'payload_sha256': tree_digest(result['payload']), 'new_native_processes': 0}, 'relocation proof differs')
    require(result['confidence_interval'] is False and result['tail_inference'] is False
        and result['optimization_or_adoption_conclusion'] is False, 'forbidden inference')
    return result


def package(repo, out, tool_sha):
    repo, out = Path(repo).resolve(), Path(out).resolve()
    require(sha(Path(__file__)) == tool_sha, 'reviewed supplement tool SHA differs')
    require(out.is_relative_to(repo/'docs/perf/vm-owner-scaling') and out != repo/'docs/perf/vm-owner-scaling'
        and not out.exists(), 'supplement archive must be fresh under owned root')
    require(sha(repo/AUTHORITY_REL) == AUTHORITY_SHA and sha(repo/ACCEPTANCE_REL) == ACCEPTANCE_SHA, 'parent authority identity differs')
    # Read only these small pinned inputs before acquiring/validating W3's queue.
    original_tool = repo/WINDOW_REL/'proposal/tools/collect_owner_measurement.py'
    require(sha(original_tool) == COLLECTOR_SHA, 'original frozen collector differs')
    old = load_module(original_tool, '_d031_package_frozen_collector')
    proposal = old.read(repo/WINDOW_REL/'proposal/proposal.json')
    require(sha(repo/WINDOW_REL/'proposal/proposal.json') == PROPOSAL_SHA, 'D030 proposal identity differs')
    require(str(Path(sys.executable).absolute()) == proposal['python']['executable']
        and Path(__file__).absolute() == repo/'experiments/vm_owner_measurement/supplement_approval_bytes.py',
        'exact supplement Python/source entry differs')
    queue = old.active_queue(proposal, APPROVAL_SHA, 'validation', validation_argv=[proposal['python']['executable'],
        str(Path(__file__).absolute()), 'package', '--repo', str(repo), '--out', str(out), '--tool-sha256', tool_sha])
    require(proposal['execution']['repository'] == str(repo), 'original repository differs')
    decision = read(repo/AUTHORITY_REL)
    require(set(decision['original_evidence_sha256']) == set(PIN_TARGETS), 'D031 evidence pin set differs')
    for relative, expected in decision['original_evidence_sha256'].items():
        require(sha(repo/relative) == expected, 'D031 original evidence differs: '+relative)
    # No mutation of the original window, frozen proposal, or original receipts.
    originals = {relative: tree(repo/relative) for relative in (WINDOW_REL, TIMING_REL, FAILED_REL)}
    window_bytes = sum(item['bytes'] for item in originals[WINDOW_REL]['files'].values())
    receipt_bytes = sum(item['bytes'] for relative in (TIMING_REL, FAILED_REL) for item in originals[relative]['files'].values())
    payload_bytes = 2*window_bytes+receipt_bytes+4*1024*1024
    old.budget(repo, 2*payload_bytes)  # archive and one temporary complete relocation copy.
    out.mkdir(parents=True)
    shutil.copy2(repo/AUTHORITY_REL, out/'authority.json')
    shutil.copy2(repo/ACCEPTANCE_REL, out/'proposal-acceptance.json')
    shutil.copy2(repo/APPROVAL_REL, out/'original-approval.json')
    shutil.copy2(Path(__file__), out/'supplement_approval_bytes.py')
    shutil.copytree(repo/WINDOW_REL, out/'original-window')
    shutil.copytree(repo/WINDOW_REL, out/'derived-window')
    shutil.copytree(repo/TIMING_REL, out/'timing-receipt')
    shutil.copytree(repo/FAILED_REL, out/'failed-archive-receipt')
    require(tree(out/'original-window') == originals[WINDOW_REL]
        and tree(out/'timing-receipt') == originals[TIMING_REL]
        and tree(out/'failed-archive-receipt') == originals[FAILED_REL], 'copy changed original evidence')
    proof = prove_approval((out/'original-approval.json').read_bytes(),
        (out/'original-window/approval.json').read_bytes(), old.dump)
    (out/'derived-window/approval.json').write_bytes((out/'original-approval.json').read_bytes())
    write(out/'approval-proof.json', proof)
    write(out/'differences.json', compare_windows(out/'original-window', out/'derived-window'))
    write(out/'original-inventory.json', originals[WINDOW_REL])
    write(out/'package-coordination.json', queue)
    checks = audit_payload(out)
    payload = payload_tree(out)
    result = {'schema': 'owner-measurement-approval-byte-supplement-v1', 'decision': 'D031', 'status': 'passed',
        'derived_is_original_bytes': False, 'native_reports_timing_evidence': False, 'measurement_authorized': True,
        'original_inventory_origin': 'First complete inventory created during D031 W3 validation; not an earlier D030 authority pin',
        'qualification_scope': 'D030 single descriptive window audited through declared approval-byte restoration only; each24cell retains original noise qualification',
        'tool_sha256': tool_sha, 'audit': checks, 'payload': payload,
        'relocation_audit': {'status': 'passed', 'scope': 'complete package including result.json, COMPLETE, both windows, all frozen tools and both full receipts',
            'payload_sha256': tree_digest(payload), 'new_native_processes': 0},
        'confidence_interval': False, 'tail_inference': False, 'optimization_or_adoption_conclusion': False}
    # Audit the exact final wrapper and completion marker at a distinct path
    # before writing those same bytes into the final archive.
    with tempfile.TemporaryDirectory(prefix='d031-relocation-', dir=repo/ROOT_REL) as temporary:
        moved = Path(temporary)/'package'
        shutil.copytree(out, moved)
        require(payload_tree(moved) == payload, 'relocated complete payload differs')
        write(moved/'result.json', result)
        (moved/'COMPLETE').write_text(sha(moved/'result.json')+'\n')
        require(audit(moved) == result, 'relocated complete package audit differs')
    require(all(tree(repo/relative) == identity for relative, identity in originals.items()), 'original evidence changed during supplement')
    require(all(sha(repo/relative) == expected for relative, expected in decision['original_evidence_sha256'].items())
        and sha(repo/AUTHORITY_REL) == AUTHORITY_SHA and sha(repo/ACCEPTANCE_REL) == ACCEPTANCE_SHA,
        'pinned original authority/evidence changed during supplement')
    write(out/'result.json', result)
    (out/'COMPLETE').write_text(sha(out/'result.json')+'\n')
    old.budget(repo)
    audit(out)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('package')
    command.add_argument('--repo', type=Path, required=True)
    command.add_argument('--out', type=Path, required=True)
    command.add_argument('--tool-sha256', required=True)
    command = commands.add_parser('audit')
    command.add_argument('--record', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'package':
            package(args.repo, args.out, args.tool_sha256)
        else:
            audit(args.record)
        print(json.dumps({'status': 'passed', 'command': args.command, 'new_native_processes': 0, 'new_builds': 0}))
        return 0
    except Exception as error:
        print(type(error).__name__+': '+str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
