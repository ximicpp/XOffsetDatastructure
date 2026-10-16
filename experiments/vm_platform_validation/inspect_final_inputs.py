#!/usr/bin/env python3
"""Bounded final-delivery input inspection; invoke through the shared queue."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
mapping = json.loads((root / 'experiments/vm_platform_validation/g4-preparation/identity-mapping.json').read_text())
for key in ('candidate_source_identity', 'field_identity_observations_now'):
    value = mapping[key]
    print(key, 'type', type(value).__name__, 'keys', list(value)[:24] if isinstance(value, dict) else None)
for name in ('vm_network', 'vm_observability', 'vm_network_measurement', 'vm_owner_scaling', 'vm_owner_measurement'):
    directory = root / 'experiments' / name
    files = [str(p.relative_to(root)) for p in directory.rglob('*') if p.is_file() and '__pycache__' not in p.parts and not any(x in p.parts for x in ('evidence', 'artifacts', 'snapshots'))]
    print(name, 'files', len(files), 'bytes', sum((root / p).stat().st_size for p in files))
    handoff = json.loads((directory / 'handoff.json').read_text())
    for key in ('frozen_prepared', 'frozen_builds', 'selected_build', 'tool_source_sha256', 'toolkit', 'prepared', 'source', 'producer_binding', 'proposal'):
        if key in handoff:
            print(name, key, json.dumps(handoff[key], ensure_ascii=False)[:7000])
    print(name, 'paths', files[:65])
