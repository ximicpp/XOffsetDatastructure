#!/usr/bin/env python3
"""Preserve accepted generated/executed bytes, never substitute live revisions."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
out = root / 'docs/validation/vm-final/final-v1'
diagnostic = 'docs/validation/vm-observability/frozen-implementation-v1/build/input/diagnostic/'
network = 'build/vm-observability/network/build-v2/input/source/'
original = 'build/vm-owner-measurement/proposal-v1/tools/'
items = [
    (diagnostic+'xoffsetdatastructure.hpp', 'accepted-generated/diagnostic/xoffsetdatastructure.hpp', '8d9447a81ca07f758fa813fbcc0823f8e8910d94cc2d9214bd7fda757a2964e6'),
    (diagnostic+'xoffset_virtual_memory.hpp', 'accepted-generated/diagnostic/xoffset_virtual_memory.hpp', 'b1caeed243f6ba412c3f68104730df21e0ad580193adc1c3186749c76c1c4e57'),
    (diagnostic+'n3_runtime.hpp', 'accepted-generated/diagnostic/n3_runtime.hpp', '0ee7e25b9a76f0e7b06acc25554be57219cd17982e3d6e1898a376ddacfba10a'),
    (network+'net_reference.cpp', 'accepted-generated/network/net_reference.cpp', 'e7cc3f07e2d22cbfa8ae6491e0b682a900e1760274e0cd2c9d59deb102251d7d'),
    (network+'network_support.hpp', 'accepted-generated/network/network_support.hpp', '8ec552329b13ca7c226a658adce4d6813f7d2af1a0b1ab09735546e8c0e419ec'),
    (original+'collect_owner_measurement.py', 'accepted-executed/D030/collect_owner_measurement.py', '8ab7a0dbc2f8e5e879fd3b84f74598ffd4e84e1c2f06bac9f6b3fc89003c9214'),
    (original+'audit_owner_measurement.py', 'accepted-executed/D030/audit_owner_measurement.py', '1711b558bd75376bd4ef6654427a9b0b0519bc94b288b211087bfdf1b132fced'),
    (original+'queue-runner.py', 'accepted-executed/D030/queue-runner.py', '6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909'),
    ('experiments/vm_owner_measurement/test_owner_measurement.py', 'accepted-executed/D030/test_owner_measurement.py', 'ba1aff8fff93fe3ddaf48740605d0ae794c97a0c7808475b6635e3b2b4e1f61c'),
]
pending = []
for original_path, destination, expected in items:
    path = root / original_path
    if path.is_symlink():
        raise RuntimeError('symlink input: ' + original_path)
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise RuntimeError('accepted bytes unavailable: ' + original_path)
    pending.append((original_path, destination, expected, data))
records = []
for original_path, destination, expected, data in pending:
    path = out / 'source' / destination
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(data)
    records.append({'original': original_path, 'source_path': destination,
                    'sha256': expected, 'bytes': len(data),
                    'scope': 'Accepted generated or original-D030 bytes; not the corrected D031 live revision'})
with (out / 'preserved-inputs.json').open('x') as stream:
    json.dump(records, stream, ensure_ascii=False, indent=2)
    stream.write('\n')
print(json.dumps({'preserved': len(records), 'bytes': sum(x['bytes'] for x in records),
                  'live_D031_freeze': False, 'new_native': 0}))
