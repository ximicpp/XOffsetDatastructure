"""Write fixed source-only registration and host derivative review artifacts."""
from pathlib import Path
import ast, difflib, hashlib, json

R = Path('/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发')
D = R / 'docs/coordination/c14-iteration-20261020-v2'
V = D / 'validation'
PREP = V / 'q32_freeze_preparation'
HERE = Path(__file__).resolve().parent
OLD = Path('/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1')

def fid(path):
    raw = Path(path).read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}

def ref(path):
    return {'path': str(path), 'identity': fid(path)}

def write(path, obj):
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(obj, stream, indent=2, sort_keys=True); stream.write('\n')

def main():
    old_author = Path('/root/xoffset-four-paths/20261018/v1/formal-registration-v4/author')
    old_helper = old_author / 'observe-backing.ps1'
    with (HERE / old_helper.name).open('xb') as output:
        output.write(old_helper.read_bytes())
    pins = {'proposal': ref(OLD / 'proposal.json')}
    for name in ('measurement-decision-v2.json', 'power-capability-v2.json',
                 'power-capability-v3.json', 'resources-pure-tests-v3.json'):
        pins[name] = ref(OLD / 'registration' / name)
    write(PREP / 'ORIGINAL_POLICY_AND_CAPABILITY_PINS_v1.json', {'files': pins,
        'scope': 'Exact archived policy JSON and capability receipts. No historical stage identity is imported.'})
    full_path = V / 'combined_full/COMBINED_FUNCTIONAL_GATES_v2.json'
    full = json.loads(full_path.read_bytes())
    own = ref(D / 'reviews/COMBINED_FULL_ACTUAL_ACCEPTANCE.json')
    source = ref(D / 'reviews/COMBINED_SOURCE_ACCEPTANCE.json')
    full_ref = ref(full_path)
    component_codegen = [ref(D / 'reviews/R1_SCREEN_GATES_v2/codegen.json'),
                         ref(D / 'reviews/M2_SCREEN_GATES_v1/codegen.json')]
    component_mech = [ref(V / 'screen/R1/gate-receipts-v1/mechanism.json'),
                      ref(D / 'reviews/M2_SCREEN_GATES_v1/mechanism.json')]
    def gate(scope, evidence, **extra):
        return {'status': 'proposed_requires_root_independent_acceptance', 'scope': scope,
                'evidence': evidence, 'own_new_execution_here': False, **extra}
    gates = {
        'codegen': gate('Actual combined compiler success in own73+73/full business closure; retained component codegen plus exact patch union. No new combined ASM run or claimed combined machine-code mechanism.',
                        [full_ref, own, source, *component_codegen], combination_asm_executed=False),
        'mechanism': gate('Both retained component mechanisms plus independently accepted exact source union and interaction precheck. Component observations are not an executed combined dynamic mechanism or speed result.',
                          [source, full_ref, *component_mech], combination_dynamic_mechanism_executed=False),
        'targeted': gate('Own combined six repository scopes each normal/SAN and corrective four external native checks; original failed compile preserved.', [full['targeted'], full_ref, own]),
        'full_normal': gate('Own combined actual73 normal tests, assertions/toolchain scope retained.', [full_ref, own, full['full_closure']['normal']['receipt']]),
        'full_san': gate('Own combined actual73 SAN tests, fixed address+undefined toolchain scope retained.', [full_ref, own, full['full_closure']['sanitized']['receipt']]),
        'business_SML': gate('Own combined80 normal+80 SAN including all S/M/L cells and original full-body offline audits; no performance inference.', [full['business'], full_ref, own]),
        'v1_bidirectional': gate('Own combined V1 main84+extra8, AB4/C4 native actions; retained old controls only in compatibility.', [full['V1_bidirectional'], full_ref, own]),
        'consumers': gate('Own combined installed/subproject/relocated/archive consumers and both shared runtime consumers.', [full['consumers'], full['shared_runtimes'], full_ref, own]),
        'lifecycle': gate('Own combined52 finite lifecycle/pool cases; no arbitrary custom lifetime or RSS proof.', [full['lifecycle52'], full_ref, own]),
        'safety': gate('Independent exact patch union/interaction source review plus own complete correctness closure and existing lifetime/callback/ADL preconditions.', [source, full_ref, own]),
        'engineering': gate('Own complete source154/provider/runtime/61B compiler guards, consumer/archive closure and original clean queues. No native Windows/platform qualification.', [full_ref, own, ref(V / 'combined_full/COMBINED_VALIDATION_COMPLETION_INDEX.json')]),
    }
    bridge = {'schema': 'combined-current-eleven-gate-composition-request-v1',
        'status': 'proposed_not_accepted_not_execution_admission', 'candidate_identity': full['candidate_identity'],
        'scope': 'Current C14 exact R1+M2 source union; own complete functional closure. Codegen/mechanism explicitly compose retained component evidence and actual own compiler success; no combined diagnostic or speed observation is fabricated.',
        'gates': gates, 'resource_actual_acceptance': ref(D / 'reviews/FINAL_SIX_ROLE_RESOURCE_ACTUAL_ACCEPTANCE.json'),
        'root_acceptance_required_fields': {'status': 'passed', 'composition_accepted': True,
            'candidate_identity': full['candidate_identity'],
            'composition_request': 'actual path and byte/hash identity of this file',
            'accepted_gate_names': list(gates)},
        'executions': {'native': 0, 'compile': 0, 'capture': 0, 'queue': 0, 'body_audit': 0,
                       'pause': 0, 'power': 0, 'freeze': 0}, 'QF_admitted': False}
    bridge_path = PREP / 'COMBINED_TYPED_GATE_COMPOSITION_REQUEST_v1.json'
    write(bridge_path, bridge)
    source_pause = HERE.parent / 'screen/m2/host/process_pause.py'
    source_ps = source_pause.with_name('pause-control.ps1')
    host = HERE.parent / 'q32_host_v1'; host.mkdir(exist_ok=False)
    old = source_pause.read_text()
    before_state = "validation/screen/M2/host/process-state-v1"
    after_state = "validation/final_formal_v1/host-qualification-v1/process-state-v1"
    new = old.replace(before_state, after_state).replace('780', '2820')
    assert old.count(before_state) == 1 and old.count('780') == 4
    assert new.replace(after_state, before_state).replace('2820', '780') == old
    with (host / source_pause.name).open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(new)
    old_ps = source_ps.read_text(); new_ps = old_ps.replace('780', '2820')
    assert old_ps.count('780') == 2
    with (host / source_ps.name).open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(new_ps)
    diff = ''.join(difflib.unified_diff(old.splitlines(True), new.splitlines(True), fromfile='M2/process_pause.py', tofile='Q32/process_pause.py'))
    diff += ''.join(difflib.unified_diff(old_ps.splitlines(True), new_ps.splitlines(True), fromfile='M2/pause-control.ps1', tofile='Q32/pause-control.ps1'))
    with (PREP / 'Q32_HOST_DERIVATION_v1.patch').open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(diff)
    # Only syntax parsing, never import the producer or host modules.
    for path in (HERE / 'register_current.py', HERE / 'observe-capacity.py', HERE / 'prepare_current.py', host / 'process_pause.py'):
        ast.parse(path.read_bytes())
    tools = {name: fid(HERE / name) for name in ('register_current.py', 'observe-capacity.py', 'observe-backing.ps1', 'prepare_current.py')}
    write(PREP / 'REGISTRATION_SOURCE_READY_v1.json', {
        'status': 'source_ready_pending_independent_gate_composition_and_preparation_admission',
        'tools': {name: ref(HERE / name) for name in tools}, 'original_register': ref(old_author / 'register.py'),
        'observer_original': ref(old_author / 'observe-capacity.py'), 'windows_helper_byte_identical': fid(old_helper) == fid(HERE / old_helper.name),
        'policy_pins': ref(PREP / 'ORIGINAL_POLICY_AND_CAPABILITY_PINS_v1.json'), 'gate_composition_request': ref(bridge_path),
        'derivation': 'Fixed small metadata producer consumes current accepted154/14/73/v2 and12 reused records. No old register imports or entry. Original proposal campaigns/common/Q/F decisions copied exactly; only identities/references/current output paths replaced. Accepted bound11 sources remain byte unchanged.',
        'capacity_output': str(PREP / 'capacity-v1'),
        'future_registration': str(V / 'final_formal_v1'),
        'missing': ['Root actual explicit eleven-gate composition acceptance with exact candidate/request IDs.',
                    'Exact admitted preparation request binds that acceptance and this four-tool source set.',
                    'Once-only original W1 validation600 preparation and current capacity metadata; no native.',
                    'Independent review of actual registration, then separate original W1 validation600 source freeze.'],
        'executed': {'syntax_parse': 4, 'project_import': 0, 'project_entry': 0, 'native': 0, 'compile': 0, 'body_audit': 0,
                     'queue': 0, 'freeze': 0, 'host_probe': 0, 'pause': 0, 'power': 0}, 'QF_admitted': False})
    write(PREP / 'Q32_HOST_SOURCE_READY_v1.json', {'status': 'source_ready_only_not_host_or_execution_admission',
        'original_python': ref(source_pause), 'original_helper': ref(source_ps),
        'derived_python': ref(host / source_pause.name), 'derived_helper': ref(host / source_ps.name),
        'exact_patch': ref(PREP / 'Q32_HOST_DERIVATION_v1.patch'), 'maximum_seconds': 2820,
        'duration_basis': {'qualification': 2700, 'bounded_setup': 60, 'restore_margin': 60},
        'only_changes': ['Fresh fixed STATE directory.', 'Python constructor/check/message/CLI780 to2820.', 'PowerShell ValidateRange/default780 to2820.'],
        'allowed_names_unchanged': ['client_ue4-Win64-Shipping', 'TGitCache'],
        'all_pid_creation_owner_handle_token_resume_and_coverage_checks_unchanged': True,
        'plan_prerequisites_start_stop_or_coverage_executed': False,
        'required_before_timing': ['Fresh actual read-only PID/creation/image/owner plan and original W1 prelude authority.',
            'Fresh global quiet record; only the two authorized names may be paused.',
            'Exact frozen manifest/registration/REVIEW/HOST_REVIEW and original2700 timing request.',
            'Prelaunch remaining guard coverage at least2700+60 seconds; restore after window, offline audit after restore.'],
        'power': 'Preparation leaves actual Balanced unchanged. Original accepted PowerWindow selects existing HighPerformance8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c during Q/F and restores actual original scheme using finally+independent watchdog. No scheme creation/parameter edits/setactive probe; exact HOST_REVIEW must disclose actual coverage.',
        'resource_scope': 'b897 actual one-pair-per-role observation was Balanced/livehost, not HighPerformance observation or future tail guarantee.',
        'executions': {'native': 0, 'queue': 0, 'host_probe': 0, 'pause': 0, 'power': 0}, 'QF_admitted': False})
    print(json.dumps({'registration_ready': ref(PREP / 'REGISTRATION_SOURCE_READY_v1.json'),
                      'host_ready': ref(PREP / 'Q32_HOST_SOURCE_READY_v1.json'), 'gate_request': ref(bridge_path)}))

if __name__ == '__main__':
    main()
