#!/usr/bin/env python3
"""Audit only the new delivery capsule; never execute its tools or follow evidence refs."""

import argparse
import fnmatch
import gzip
import hashlib
import json
import os
import re
import stat
import sys
import tarfile
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


MIB = 1024 * 1024
MAX_JSON = 2 * MIB
MAX_SOURCE = 32 * MIB
MAX_CAPSULE = 72 * MIB
MAX_TAR_STREAM = MAX_SOURCE + 8 * MIB
# Actual D032/D033 schemas and the separately queued nine-file source increment
# are explicitly bound below. No source-review proposal substitutes for them.
FINAL_AUTHORITY_SCHEMA_BOUND = True
SPEC = 'experiments/vm_platform_validation/final-package-spec.json'
INDEX = 'docs/validation/vm-final/build-identities.json'
EVIDENCE = 'docs/validation/vm-final/evidence-index.json'
MATRIX = 'docs/validation/vm-final/verification-matrix.json'
OWNER_PREFIX = 'experiments/vm_owner_measurement/'
OWNER_AUTHORITY_PATH = 'docs/coordination/vm-followup/owner-measurement-acceptance.json'
OWNER_AUTHORITY_SHA = 'd87332b22406e9077ab68a44cb02ba14ac611a8fba78447c6498458bf584eddd'
D032_TOOL_PATH = 'docs/coordination/vm-followup/contracts/owner-measurement-v1/supplement-tool-acceptance.json'
D032_TOOL_SHA = '3e748a86ddb2521154c6a6ebde3114aae7ebb9f86dc94a50273274c5d6c263ad'
OWNER_SOURCE_INCREMENT_PATH = 'docs/validation/vm-final/final-increment.json'
OWNER_SOURCE_INCREMENT_SHA = 'ff2d5d1b679b37bf70c1be3768a07c05fa477b632a45fa6165bfe380e9657955'
D031_AUTHORIZATION = 'docs/coordination/vm-followup/contracts/owner-measurement-v1/approval-copy-supplement.json'
D031_AUTHORIZATION_SHA = 'ea768e0857471588ff103002e7ccd545e8b501842836f9850d6bf80625f5afdb'
# Exact sources accepted by D032; they do not replace the original D030 tools.
D032_SOURCE_SHA = {
    OWNER_PREFIX + 'README.md': '25639037ce64481064da6cee9de59210c964a11a170664a48bf7ba55f51b8a4c',
    OWNER_PREFIX + 'collect_owner_measurement.py': '5f14fa1de1bd93ad5db2a9cd726856051c5508a6738ba85de888c7fb0a6c2426',
    OWNER_PREFIX + 'supplement_approval_bytes.py': '411d4e6054c18e09cd0a2f0d5caa8e972552376e1918856062941e039dc37698',
    OWNER_PREFIX + 'audit_owner_measurement.py': '1711b558bd75376bd4ef6654427a9b0b0519bc94b288b211087bfdf1b132fced',
    OWNER_PREFIX + 'test_owner_measurement.py': 'ba1aff8fff93fe3ddaf48740605d0ae794c97a0c7808475b6635e3b2b4e1f61c',
    OWNER_PREFIX + 'test_approval_bytes.py': '9cf879a9092d157621aed70c26a94bbc38e53c512f7dd11b742803c6b5d31eb8',
}
D031_ORIGINAL_PINS = {
    'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json': '0157b0d310d7388729f51a4031c6ca552ca32f9d15982972d7e0c5aebe1d9e12',
    'build/vm-owner-measurement/window-v1/approval.json': '92058415a840fe800a7e519a3835461c18032f177e9cfe2a2d0e2743ef9d3aa1',
    'build/vm-owner-measurement/window-v1/window.json': '9534af410e632fac2bb4879b2014690f2a5e36178e1f0537a52795567ce10869',
    'build/vm-owner-measurement/window-v1/summary.json': '59e192b428848734d14f3f64676481d267f0744697a372c942101442da9d2340',
    'build/vm-owner-measurement/receipts/window-v1/result.json': '86772763e903df29465aaf853d50fc02393e301b530a2fc25889723135cfb131',
    'build/vm-owner-measurement/receipts/archive-v1/result.json': '104dab0d0d58092c2d7714bdc17ab17296151881487d0757cc5bd669e2946662',
    'build/vm-owner-measurement/receipts/archive-v1/stderr.log': '95f12e176d9df2be22a469abb37b5163203b4f566847cbd9e114784399669ca6',
}
OWNER_INHERITED_OR_AUXILIARY_SOURCES = {
    OWNER_PREFIX + 'REVIEW.md': ('164213e457aeef1c5dd8b0f97a72a2ddb5f048b651b69915701bb8f39d9792d1', 2109),
    OWNER_PREFIX + 'producer-binding-v1.json': ('6b3da0f9c127dfc7fcf58ad08a8262a7e1bee9d06d93dba4230c9ff082335b14', 1471),
    OWNER_PREFIX + 'producer-binding.template.json': ('3e67f13868bcbd653e010def3037a15c5c510e34d0b8c1e0e80ebb8855440ff9', 259),
}
OWNER_ACCEPTED_RECORDS = {
    'window': {'path': 'build/vm-owner-measurement/window-v1/window.json',
        'sha256': '9534af410e632fac2bb4879b2014690f2a5e36178e1f0537a52795567ce10869'},
    'summary': {'path': 'build/vm-owner-measurement/window-v1/summary.json',
        'sha256': '59e192b428848734d14f3f64676481d267f0744697a372c942101442da9d2340'},
    'native_receipt': {'path': 'build/vm-owner-measurement/receipts/window-v1/result.json',
        'sha256': '86772763e903df29465aaf853d50fc02393e301b530a2fc25889723135cfb131'},
    'archive': {'path': 'docs/perf/vm-owner-scaling/measurement-approval-supplement-v1/result.json',
        'sha256': 'cbaf56a9e4ad79267c90cb2a0c9853d502f62e1cbb0903c8c1ed5492578ca1f7'},
    'archive_receipt': {'path': 'build/vm-owner-measurement/receipts/approval-supplement-v1/result.json',
        'sha256': '24dcf164f4873455550cf1c049bd6d3a3abff99636cdc2be2973acbfe4986055'},
}
CONTROLS = {'CAPSULE.md', 'stable-source-manifest.json',
            'source-differences.json', 'preserved-inputs.json'}
ACCEPTED_CONTROLS = {
    'CAPSULE.md': {'sha256': 'e66694d43ede2815d4852ba8713087ebfd1a7bb4f2874cddbc05f1f64caca45c', 'bytes': 1953},
    'stable-source-manifest.json': {'sha256': '562d9a6ba2b0b5702097111cd1405135bce5a37f34a7047b949fd26a402f2ed1', 'bytes': 34115},
    'source-differences.json': {'sha256': '86aead6132c24100e5a05c8f14c28e87fda77d0fb6174e46b94617cba912b806', 'bytes': 16627},
    'preserved-inputs.json': {'sha256': '63161fb2ca6d94fd75227eeb8676d385c8976e9737f5c5e3f64dd189b26cb5b2', 'bytes': 3385},
}
TOP_FILES = CONTROLS | {'source-manifest.json', 'source.tar.gz',
    'final-input-differences.json', 'metadata-manifest.json',
    'metadata-path-aliases.json', 'cut.json'}
PRESERVED = {
    'accepted-generated/diagnostic/xoffsetdatastructure.hpp':
        ('8d9447a81ca07f758fa813fbcc0823f8e8910d94cc2d9214bd7fda757a2964e6', 187645),
    'accepted-generated/diagnostic/xoffset_virtual_memory.hpp':
        ('b1caeed243f6ba412c3f68104730df21e0ad580193adc1c3186749c76c1c4e57', 19618),
    'accepted-generated/diagnostic/n3_runtime.hpp':
        ('0ee7e25b9a76f0e7b06acc25554be57219cd17982e3d6e1898a376ddacfba10a', 19990),
    'accepted-generated/network/net_reference.cpp':
        ('e7cc3f07e2d22cbfa8ae6491e0b682a900e1760274e0cd2c9d59deb102251d7d', 48584),
    'accepted-generated/network/network_support.hpp':
        ('8ec552329b13ca7c226a658adce4d6813f7d2af1a0b1ab09735546e8c0e419ec', 7003),
    'accepted-executed/D030/collect_owner_measurement.py':
        ('8ab7a0dbc2f8e5e879fd3b84f74598ffd4e84e1c2f06bac9f6b3fc89003c9214', 38406),
    'accepted-executed/D030/audit_owner_measurement.py':
        ('1711b558bd75376bd4ef6654427a9b0b0519bc94b288b211087bfdf1b132fced', 13905),
    'accepted-executed/D030/queue-runner.py':
        ('6f155fcb1f17d6a01d68c4c9393bc636bf4af82bafdc830ba8dc8b759ea1d909', 16898),
    'accepted-executed/D030/test_owner_measurement.py':
        ('ba1aff8fff93fe3ddaf48740605d0ae794c97a0c7808475b6635e3b2b4e1f61c', 50410),
}
BINARY_SHA = {
    'W1-network-timing': ['5cf8a18e11ab38676cd7a1791556233b239b8791c9b4e665642f0092dcec7f18'],
    'W1-network-diagnostic': ['ae6ce12e8ed06b246e19b73995f774da0d3c74f7a369e9f336b2d7fbc30558c8'],
    'W2-foundation-original-and-diagnostic': [
        '498928f7ed536fe1ebda17b213e285525dd27b82febd673d58620c0c649a33a8',
        '784d8e36a5a0a2f8c827a6991f7c41d04fad304f387728affca8aeae08cd083e'],
    'W2-network-generated-diagnostic': ['12d97eb430a8d894a4491944827fce7b169835ebb9dec040b02b92a66f8ba2ed'],
    'owner26-accepted-build-v4': [
        'a9c176625111d668b501bf6b7a95d44352a27e4380baaa1e171031b001617f8f',
        '2ff7b74f1561276c151a4b9323735d06eb9a65f87513317f1000ff3dfe703b45'],
    'default-D-F-producers': [
        '389916a0a454fd775474995d9e970fa15bc9dc7421ac10111a6d74c7e302163d',
        '2ef8eea55d1152d68fdabc084f7158492109ec4f7b3d200f286ac70bf4d286af'],
    'network112-producer-binding': ['5cf8a18e11ab38676cd7a1791556233b239b8791c9b4e665642f0092dcec7f18'],
    'network88-producer-binding': ['5cf8a18e11ab38676cd7a1791556233b239b8791c9b4e665642f0092dcec7f18'],
    'owner78-D030-actual-execution': ['2ff7b74f1561276c151a4b9323735d06eb9a65f87513317f1000ff3dfe703b45'],
}


class AuditError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise AuditError(message)


def relative(name):
    require(isinstance(name, str) and name and '\\' not in name and ':' not in name
            and '\x00' not in name, 'unsafe path: ' + repr(name))
    p = PurePosixPath(name)
    require(not p.is_absolute() and name == p.as_posix()
            and all(x not in {'.', '..'} for x in p.parts) and bool(p.parts),
            'noncanonical relative path: ' + name)
    return name


def digest(value):
    require(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value),
            'invalid SHA256: ' + repr(value))
    return value


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def json_file(path):
    require(path.stat().st_size <= MAX_JSON, 'oversize JSON: ' + str(path))
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=no_duplicates)


def identity(path):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode), 'not regular: ' + str(path))
    require(info.st_size <= MAX_SOURCE, 'oversize capsule file: ' + str(path))
    h = hashlib.sha256()
    total = 0
    with path.open('rb') as stream:
        while block := stream.read(1024 * 1024):
            total += len(block)
            require(total <= MAX_SOURCE, 'file grew beyond budget: ' + str(path))
            h.update(block)
    after = path.lstat()
    require((info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns) ==
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns),
            'file changed during audit: ' + str(path))
    return {'sha256': h.hexdigest(), 'bytes': total}


def manifest(value, label):
    require(isinstance(value, dict) and value, 'empty/nonobject ' + label)
    folded = set()
    for name, record in value.items():
        relative(name)
        require(name.casefold() not in folded, 'case collision in ' + label)
        folded.add(name.casefold())
        require(isinstance(record, dict) and set(record) == {'bytes', 'sha256'},
                'invalid identity record: ' + label + '/' + name)
        digest(record['sha256'])
        require(type(record['bytes']) is int and record['bytes'] >= 0,
                'invalid byte count: ' + name)
    return value


def inventory(root):
    files, dirs = set(), set()
    require(stat.S_ISDIR(root.lstat().st_mode), 'capsule root is not a real directory')
    for current, subdirs, names in os.walk(root, followlinks=False):
        for name in subdirs + names:
            p = Path(current) / name
            mode = p.lstat().st_mode
            rel = relative(p.relative_to(root).as_posix())
            if stat.S_ISDIR(mode):
                dirs.add(rel)
            else:
                require(stat.S_ISREG(mode), 'link/special capsule entry: ' + rel)
                files.add(rel)
    return files, dirs


def selector_match(name, pattern):
    parts, patterns = PurePosixPath(name).parts, PurePosixPath(relative(pattern)).parts
    return len(parts) == len(patterns) and all(
        fnmatch.fnmatchcase(part, pat) for part, pat in zip(parts, patterns))


class BoundedTarReader:
    """Bound all decompressed bytes, including tar extension records and padding."""

    def __init__(self, stream):
        self.stream, self.total = stream, 0

    def read(self, size=-1):
        requested = MAX_TAR_STREAM + 1 if size < 0 else size
        block = self.stream.read(min(requested, MAX_TAR_STREAM - self.total + 1))
        self.total += len(block)
        require(self.total <= MAX_TAR_STREAM, 'decompressed tar budget exceeded')
        return block


class Capsule:
    def __init__(self, root, args, report):
        self.root, self.args, self.report = root, args, report
        actual, dirs = inventory(root)
        self.outer = manifest(json_file(root / 'manifest.json'), 'outer manifest')
        require(identity(root / 'manifest.json')['sha256'] == args.manifest_sha256,
                'external manifest trust anchor mismatch')
        require(actual == set(self.outer) | {'manifest.json'}, 'outer regular-file closed set')
        expected_dirs = {str(parent) for name in self.outer for parent in
                         PurePosixPath(name).parents if str(parent) != '.'}
        require(dirs == expected_dirs, 'unexpected/empty capsule directory')
        require(sum(x['bytes'] for x in self.outer.values()) <= MAX_CAPSULE,
                'capsule total budget exceeded')
        for name, expected in self.outer.items():
            require(identity(root / name) == expected, 'outer bytes mismatch: ' + name)
        self.metadata = manifest(json_file(root / 'metadata-manifest.json'), 'metadata')
        self.aliases = json_file(root / 'metadata-path-aliases.json')
        require(isinstance(self.aliases, dict), 'invalid aliases object')
        for alias, target in self.aliases.items():
            relative(alias); relative(target)
            require(alias != target and alias.casefold() == target.casefold()
                    and alias not in self.metadata and target in self.metadata,
                    'invalid metadata alias: ' + alias)
        require(set(self.outer) == TOP_FILES | {'metadata/' + n for n in self.metadata},
                'unexpected capsule top-level/metadata member set')
        require(len(self.metadata) <= 1500 and
                sum(x['bytes'] for x in self.metadata.values()) <= 32 * MIB,
                'metadata budget exceeded')
        for name, record in self.metadata.items():
            require(record['bytes'] <= MAX_JSON and self.outer['metadata/' + name] == record,
                    'metadata identity mismatch: ' + name)
        self.source = manifest(json_file(root / 'source-manifest.json'), 'source')
        self.report['outer'] = {'files': len(self.outer), 'directories': len(dirs),
                                'metadata_files': len(self.metadata), 'aliases': len(self.aliases)}

    def unchanged_at_end(self):
        files, dirs = inventory(self.root)
        expected_dirs = {str(parent) for name in self.outer for parent in
                         PurePosixPath(name).parents if str(parent) != '.'}
        require(files == set(self.outer) | {'manifest.json'} and dirs == expected_dirs,
                'capsule set changed during audit')
        for name, record in self.outer.items():
            require(identity(self.root / name) == record, 'capsule changed during audit: ' + name)
        require(identity(self.root / 'manifest.json')['sha256'] == self.args.manifest_sha256,
                'manifest changed during audit')

    def lookup(self, name):
        relative(name)
        canonical = self.aliases.get(name, name)
        if canonical in self.metadata:
            return self.root / 'metadata' / canonical
        prefix = self.args.original_capsule_relative.rstrip('/') + '/'
        if name.startswith(prefix) and name[len(prefix):] in CONTROLS:
            return self.root / name[len(prefix):]
        raise AuditError('required small record absent from capsule: ' + name)

    def record(self, name):
        return json_file(self.lookup(name))

    def accepted(self, name, expected_sha):
        p = self.lookup(name)
        require(identity(p)['sha256'] == digest(expected_sha), 'authority SHA mismatch: ' + name)
        value = json_file(p)
        require(str(value.get('status', '')).startswith('accepted'),
                'authority is not accepted: ' + name)
        return value

    def source_archive(self):
        require(len(self.source) <= 1500 and
                sum(v['bytes'] for v in self.source.values()) <= MAX_SOURCE and
                all(v['bytes'] <= MAX_JSON for v in self.source.values()), 'source budget exceeded')
        seen = set()
        with gzip.open(self.root / 'source.tar.gz', 'rb') as decompressed:
            bounded = BoundedTarReader(decompressed)
            with tarfile.open(fileobj=bounded, mode='r|') as archive:
                # Inspect any members after zero padding too; duplicate/foreign
                # members cannot hide behind an earlier end-of-archive marker.
                archive.ignore_zeros = True
                for member in archive:
                    require(member.name.startswith('source/'), 'tar member prefix')
                    name = relative(member.name[len('source/'):])
                    require(member.type in {tarfile.REGTYPE, tarfile.AREGTYPE}
                            and not member.sparse, 'tar nonregular/sparse member: ' + name)
                    require(name in self.source and name not in seen, 'foreign/duplicate tar member: ' + name)
                    seen.add(name)
                    expected = self.source[name]
                    require(member.size == expected['bytes'], 'tar declared size: ' + name)
                    h, total = hashlib.sha256(), 0
                    stream = archive.extractfile(member)
                    require(stream is not None, 'missing tar content: ' + name)
                    with stream:
                        while block := stream.read(65536):
                            total += len(block)
                            require(total <= expected['bytes'], 'oversize tar stream')
                            h.update(block)
                    require({'bytes': total, 'sha256': h.hexdigest()} == expected,
                            'tar content mismatch: ' + name)
            while bounded.read(65536):
                pass
        require(seen == set(self.source), 'tar exact source set mismatch')
        self.report['source_archive'] = {'regular_members': len(seen),
            'unpacked_bytes': sum(x['bytes'] for x in self.source.values()),
            'total_decompressed_bytes': bounded.total, 'exact': True}

    def final_authority_contents(self):
        owner, tool = self.owner_authority, self.tool_authority
        require(owner['schema_version'] == 1 and owner['decision'] == 'D033' and
                owner['status'] == 'accepted_executed_descriptive_partial_qualification_with_offline_supplement',
                'unexpected final owner acceptance schema/status')
        require(tool['schema_version'] == 1 and tool['decision'] == 'D032' and
                tool['status'] == 'accepted_tool_fix_and_offline_execution' and
                tool['authority'] == {'path': D031_AUTHORIZATION, 'sha256': D031_AUTHORIZATION_SHA},
                'unexpected D032 tool acceptance schema/status/authority')
        require(owner['tool_fix'] == {'acceptance': D032_TOOL_PATH, 'normal_tests': 51,
                'optimized_tests': 51, 'frozen_original_tools_unchanged': True},
                'D033 tool acceptance or frozen-original boundary changed')
        require(all(tool['limits'][k] == 0 for k in
                ['new_native', 'new_builds', 'new_measurement_windows']) and
                tool['limits']['package_attempts'] == 1,
                'D032 authorizes additional native/build/window work')
        tests = tool['tests']
        require(tests['count_each'] == 51 and tests['native_processes'] == 0 and
                tests['existing_tests'] == 41 and tests['new_regressions'] == 10,
                'D032 accepted offline test counts changed')
        for mode in ['normal', 'optimized']:
            require(identity(self.lookup(tests[mode]['path']))['sha256'] == tests[mode]['sha256'],
                    'D032 offline test log identity: ' + mode)
        require(identity(self.lookup(tool['handoff']['path']))['sha256'] == tool['handoff']['sha256'],
                'D032 accepted W2 handoff changed')
        require(tool['execution']['proposed_receipt_is_not_a_completed_receipt'] is True,
                'D032 historical execution proposal relabeled as completed proof')
        accepted_sources = {relative(r['path']): digest(r['sha256']) for r in tool['sources'].values()}
        require(len(accepted_sources) == len(tool['sources']) and accepted_sources == D032_SOURCE_SHA,
                'D032 directly accepted source identities differ')
        require(owner['accepted_records'] == OWNER_ACCEPTED_RECORDS, 'D033 accepted record identities changed')
        records = {}
        for role, record in OWNER_ACCEPTED_RECORDS.items():
            path = self.lookup(record['path'])
            require(identity(path)['sha256'] == record['sha256'], 'D033 accepted record bytes: ' + role)
            records[role] = json_file(path)
        for name, sha in D031_ORIGINAL_PINS.items():
            require(identity(self.lookup(name))['sha256'] == sha, 'D031 preserved original record: ' + name)
        execution = owner['execution']
        require(all(execution[k] == v for k, v in {'role': 'W3', 'native_processes': 78,
                'preruns': 6, 'measured_processes': 72, 'samples_per_condition': 12,
                'same_condition_pairs': 6, 'conditions': 6, 'phases': 4,
                'requests_per_allocate_and_free': 8192, 'new_native_after_original_window': 0,
                'rebuilds': 0, 'windows': 1, 'all_exit_zero': True, 'timed_out': False,
                'cleanup_empty': True}.items()), 'D033 execution scope/counts changed')
        qualification = owner['qualification']
        require(all(qualification[k] == v for k, v in {'cells_total': 24,
                'qualified_cells_count': 7, 'uncertain_cells_count': 17,
                'comparisons_total': 24, 'qualified_comparisons_count': 2,
                'non_self_qualified_comparisons_count': 1, 'uncertain_comparisons_count': 22,
                'all_multithread_comparisons_uncertain': True}.items()),
                'D033 partial qualification relabeled')
        summary, window = records['summary'], records['window']
        require(summary['schema'] == 'owner-measurement-statistics-v1' and summary['status'] == 'complete'
                and summary['native_processes'] == 78 and summary['measured_processes'] == 72
                and summary['prerun_processes'] == 6 and summary['samples_per_condition'] == 12
                and len(summary['cells']) == 24 and len(summary['comparisons']) == 24,
                'accepted original summary metadata changed')
        require(window['schema'] == 'owner-measurement-window-v1' and
                window['status'] == 'native_complete_pending_receipt' and len(window['attempts']) == 78 and
                window['native_timing_evidence_claim'] is False,
                'accepted original window metadata changed')
        for role, kind in [('native_receipt', 'timing'), ('archive_receipt', 'validation')]:
            receipt = records[role]
            require(receipt['status'] == 'complete' and receipt['owner'] == 'W3' and
                    receipt['kind'] == kind and receipt['command_returncode'] == 0 and
                    all(receipt['cleanup'][k] == [] for k in ['live', 'zombies', 'process_groups']),
                    'accepted completed queue receipt invalid: ' + role)
        package, archive = records['archive'], owner['archive']
        require(package['schema'] == 'owner-measurement-approval-byte-supplement-v1' and
                package['status'] == 'passed' and package['decision'] == 'D031' and
                package['tool_sha256'] == accepted_sources[OWNER_PREFIX + 'supplement_approval_bytes.py'],
                'accepted D031 package schema/tool changed')
        require(all(package[k] is False for k in ['derived_is_original_bytes',
                'native_reports_timing_evidence', 'optimization_or_adoption_conclusion',
                'confidence_interval', 'tail_inference']), 'D031 inference boundary changed')
        difference = [{'derived_bytes': 688, 'derived_sha256': D031_ORIGINAL_PINS[
                'docs/coordination/vm-followup/contracts/owner-measurement-v1/window-approval.json'],
            'original_bytes': 688, 'original_sha256': D031_ORIGINAL_PINS[
                'build/vm-owner-measurement/window-v1/approval.json'], 'path': 'approval.json'}]
        audit = package['audit']
        require(audit['differences'] == archive['only_derived_difference'] == difference,
                'accepted derived view changes beyond approval bytes')
        proof = audit['approval_proof']
        require(proof == {'objects_equal': True, 'original_bytes': 688,
                'original_sha256': difference[0]['derived_sha256'], 'stored_bytes': 688,
                'stored_equals_legacy_dump': True, 'stored_sha256': difference[0]['original_sha256']},
                'accepted approval byte/semantic proof changed')
        require(audit['cpp_processes'] == 78 and all(audit[k] == 0 for k in
                ['new_builds', 'new_measurement_windows', 'new_native_processes']) and
                audit['summary_sha256'] == OWNER_ACCEPTED_RECORDS['summary']['sha256'] and
                audit['timing_receipt']['result_sha256'] == OWNER_ACCEPTED_RECORDS['native_receipt']['sha256'],
                'D031 package original/native identity or zero-new-work boundary changed')
        require(package['relocation_audit'] == archive['relocation_audit'] and
                package['relocation_audit']['status'] == 'passed' and
                package['relocation_audit']['new_native_processes'] == 0 and
                package['relocation_audit']['payload_sha256'] == archive['payload_sha256'] and
                archive['original_window_preserved'] is True and
                archive['original_failed_archive_preserved'] is True and
                archive['derived_window_is_original_bytes'] is False and archive['new_native_processes'] == 0,
                'D033 archive/relocation scope changed')
        payload = manifest(package['payload']['files'], 'external accepted payload index')
        require(len(payload) == archive['payload_files'] == 698 and
                sum(r['bytes'] for r in payload.values()) == archive['payload_bytes'] == 8891188,
                'accepted external package recorded inventory counts changed')
        self.owner_records, self.direct_owner_sources = records, accepted_sources
        self.report['owner_acceptance'] = {'decision': 'D033', 'native_processes': 78,
            'qualified_cells': 7, 'cells': 24, 'qualified_comparisons': 2,
            'comparisons': 24, 'non_self_qualified_comparisons': 1,
            'all_multithread_comparisons_uncertain': True,
            'external_payload_recorded_files': 698, 'external_payload_recorded_bytes': 8891188,
            'external_payload_bytes_reread': 0, 'statistics_recomputed': False}

    def owner_source_closure(self):
        path = self.lookup(OWNER_SOURCE_INCREMENT_PATH)
        require(identity(path)['sha256'] == OWNER_SOURCE_INCREMENT_SHA,
                'external queued final source increment identity changed')
        increment = json_file(path)
        require(increment['schema_version'] == 1 and increment['status'] ==
                'final_owner_source_increment_recorded_for_independent_static_review' and
                increment['authority'] == {
                    'owner': {'path': OWNER_AUTHORITY_PATH, 'sha256': OWNER_AUTHORITY_SHA},
                    'tools': {'path': D032_TOOL_PATH, 'sha256': D032_TOOL_SHA}} and
                increment['accepted_historical_source_constraints'] == 8 and
                increment['new_static_auxiliary_templates'] == 1 and
                increment['new_CPP_jobs'] == increment['new_native_processes'] == 0 and
                increment['native_or_historical_statistics_recomputed'] is False,
                'queued final source increment schema/scope changed')
        closure = increment['source_closed_set']
        require(len(closure) == 9, 'final owner source closure count')
        expected = dict(self.direct_owner_sources)
        expected.update({n: record[0] for n, record in OWNER_INHERITED_OR_AUXILIARY_SOURCES.items()})
        require(set(closure) == set(expected) and
                sum(r['bytes'] for r in closure.values()) == 152208,
                'final owner source closed set differs from independent identities')
        for name, sha in expected.items():
            require(closure[name]['sha256'] == sha and
                    self.source.get(name) == {'sha256': sha, 'bytes': closure[name]['bytes']},
                    'final owner source not bound to accepted/static identity: ' + name)
        auxiliary = OWNER_PREFIX + 'producer-binding.template.json'
        require(all(r['accepted_historical_identity_matched'] is (n != auxiliary)
                    for n, r in closure.items()), 'auxiliary template relabeled historical evidence')
        before = self.record('build/vm-owner-measurement/d031/handoff-before.json')
        review_name = OWNER_PREFIX + 'REVIEW.md'
        require(before['source']['REVIEW.md']['sha256'] == expected[review_name] and
                identity(self.lookup('build/vm-owner-measurement/d031/handoff-before.json'))['sha256'] ==
                'f19ee2fe3609060a365c45f504f980768a11fc13ae0daf46f35e02065679bef7',
                'D023 inherited review identity changed')
        proposal = self.record('build/vm-owner-measurement/proposal-v1/proposal.json')
        require(proposal['producer']['binding_sha256'] == expected[OWNER_PREFIX + 'producer-binding-v1.json'],
                'D030 inherited producer binding changed')
        self.owner_source_sha256 = expected
        self.report['owner_source_closure'] = {'files': 9, 'bytes': 152208,
            'historical_or_parent_accepted': 8, 'unexecuted_auxiliary_template': 1,
            'increment_sha256': OWNER_SOURCE_INCREMENT_SHA}

    def source_lineage(self):
        spec, cut = self.record(SPEC), json_file(self.root / 'cut.json')
        require(spec['state'] == 'ready_to_seal', 'spec not final/ready')
        require(self.args.owner_acceptance_path == OWNER_AUTHORITY_PATH and
                self.args.owner_acceptance_sha256 == OWNER_AUTHORITY_SHA,
                'unexpected final owner78 authority path')
        require(self.args.d031_acceptance_path == D032_TOOL_PATH and
                self.args.d031_acceptance_sha256 == D032_TOOL_SHA, 'unexpected D032 tool acceptance')
        require(spec['owner_acceptance_path'] == self.args.owner_acceptance_path and
                spec['owner_acceptance_sha256'] == self.args.owner_acceptance_sha256,
                'spec owner78 authority differs from external trust anchor')
        require(spec['D031_tool_acceptance'] == self.args.d031_acceptance_path and
                spec['D031_tool_acceptance_sha256'] == self.args.d031_acceptance_sha256,
                'spec D031 authority differs from external trust anchor')
        self.owner_authority = self.accepted(self.args.owner_acceptance_path,
                                           self.args.owner_acceptance_sha256)
        self.tool_authority = self.accepted(self.args.d031_acceptance_path,
                                          self.args.d031_acceptance_sha256)
        authorization_path = self.lookup(D031_AUTHORIZATION)
        require(identity(authorization_path)['sha256'] == D031_AUTHORIZATION_SHA,
                'D031 original authorization identity changed')
        authorization = json_file(authorization_path)
        require(authorization['decision'] == 'D031' and
                authorization['original_evidence_sha256'] == D031_ORIGINAL_PINS and
                all(authorization['limits'][k] == 0 for k in
                    ['new_native', 'new_builds', 'measurement_windows']),
                'D031 original evidence/no-execution boundaries changed')
        for name, sha in D032_SOURCE_SHA.items():
            require(self.source.get(name, {}).get('sha256') == sha,
                    'D032 accepted source changed: ' + name)
        self.final_authority_contents()
        self.owner_source_closure()
        require(FINAL_AUTHORITY_SCHEMA_BOUND,
                'final owner78/D031 accepted-content schema binding is pending; do not release audit')
        require(cut['owner_acceptance'] == self.args.owner_acceptance_path and
                cut['owner_acceptance_sha256'] == self.args.owner_acceptance_sha256,
                'cut owner authority binding')
        require(cut['new_CPP_jobs'] == 0 and cut['new_container_jobs'] == 0 and
                cut['source_tar_verified'] is True and cut['parent_G4_accepted'] is False,
                'unexpected new-work or premature G4 claim')
        require(spec['initial_control_identities'] == ACCEPTED_CONTROLS,
                'staged controls differ from independent accepted identities')
        for name, expected in ACCEPTED_CONTROLS.items():
            require(identity(self.root / name) == expected, 'staged control changed: ' + name)
        stable = manifest(json_file(self.root / 'stable-source-manifest.json'), 'stable225')
        require(len(stable) == 225, 'stable source count differs from accepted225')
        expected = dict(stable)
        for name, record in stable.items():
            require(self.source.get(name) == record, 'stable source delta: ' + name)
        preserved = json_file(self.root / 'preserved-inputs.json')
        require(isinstance(preserved, list) and len(preserved) == 9, 'preserved9 count')
        mapped = {x['source_path']: x for x in preserved}
        require(set(mapped) == set(PRESERVED), 'preserved9 exact path set')
        for name, (sha, size) in PRESERVED.items():
            require(mapped[name]['sha256'] == sha and mapped[name]['bytes'] == size,
                    'preserved accepted identity: ' + name)
            require(name not in expected and self.source.get(name) == {'sha256': sha, 'bytes': size},
                    'preserved tar bytes differ from acceptance: ' + name)
            expected[name] = self.source[name]
        owner_files = spec['expected_final_source']
        require(owner_files == self.owner_source_sha256 and
                {n for n in self.source if n.startswith(OWNER_PREFIX)} == set(owner_files),
                'final D031 owner source closed set')
        for name, sha in owner_files.items():
            require(self.source[name]['sha256'] == digest(sha), 'D031 final source SHA: ' + name)
            expected[name] = self.source[name]
        for name in ['experiments/vm_platform_validation/preserve_final_inputs.py',
                     'experiments/vm_platform_validation/seal_delivery.py']:
            require(name in self.source and name not in expected, 'packaging helper collision/absence')
            expected[name] = self.source[name]
        require(spec.get('extra_source_files', []) == [], 'unaccepted supplemental source scope')
        require(expected == self.source, 'unknown/unreviewed source entries')
        differences = json_file(self.root / 'source-differences.json')
        base = differences['base_inheritance_candidate_checks']
        require(len(base) == 21 and all(x['same'] is True and x['sha256'] == x['expected_sha256']
                    for x in base.values()), 'accepted21 base closure mismatch')
        for name, record in base.items():
            require(stable.get(name) == {'sha256': record['sha256'], 'bytes': record['bytes']},
                    'base comparison not bound to stable source')
        comparison = differences['stageIII_129_record_comparison']
        require(comparison['records'] == 129 and {x['path'] for x in comparison['differences']} ==
                {'README.md', 'tools/audit_vm_allocator.py', 'tools/tests/test_audit_vm_allocator.py'},
                'unexpected historical runtime source delta')
        for item in comparison['differences']:
            require(stable.get(item['path']) == item['current'],
                    'historical comparison not bound to selected source')
        lineage = json_file(self.root / 'final-input-differences.json')
        require(lineage['base_stable_deltas'] == [] and lineage['new_CPP_jobs'] == 0,
                'new base/source work claimed')
        old_by_name = {PurePosixPath(n).name: v[0] for n, v in PRESERVED.items()
                       if n.startswith('accepted-executed/D030/') and not n.endswith('queue-runner.py')}
        rows = {PurePosixPath(x['path']).name: x for x in lineage['owner_tool_lineage']}
        require(set(rows) == set(old_by_name) and len(rows) == len(lineage['owner_tool_lineage']),
                'D030/D031 lineage rows')
        for name, old_sha in old_by_name.items():
            row = rows[name]; final = self.source[OWNER_PREFIX + name]
            require(row['original_D030_sha256'] == old_sha and row['final_live'] == final and
                    row['same'] == (old_sha == final['sha256']) and
                    row['verification'] == self.args.d031_acceptance_path, 'D030/D031 conflation: ' + name)
        self.spec = spec
        self.report['lineage'] = {'stable': 225, 'preserved': 9, 'base_closure': 21,
            'historical_records': 129, 'nonruntime_deltas': 3, 'D030_original_and_D031_final_separate': True}

    def metadata_and_builds(self):
        index = self.record(INDEX)
        explicit = set(self.spec['metadata_files']) | set(index['suggested_capsule_metadata_files'])
        for name in explicit:
            self.lookup(name)
        explicit = {self.aliases.get(name, name) for name in explicit}
        for name in self.metadata:
            admitted = name in explicit or any(selector_match(name, p)
                for p in self.spec.get('metadata_globs', [])) or any(
                name.startswith(relative(p).rstrip('/') + '/') and
                PurePosixPath(name).suffix in {'.json', '.md', '.py', '.log'}
                for p in self.spec['metadata_trees']) or any(
                str(PurePosixPath(name).parent) == p and PurePosixPath(name).name.lower().startswith('handoff')
                for p in self.spec['handoff_directories'])
            require(admitted, 'unselected metadata: ' + name)
        for name, sha in self.spec.get('expected_metadata', {}).items():
            require(identity(self.lookup(name))['sha256'] == digest(sha), 'accepted metadata SHA: ' + name)
        for record in index['metadata_records_read']:
            require(identity(self.lookup(record['path'])) ==
                    {'sha256': record['sha256'], 'bytes': record['bytes']}, 'build metadata reference changed')
        profiles = {x['id']: x for x in index['profiles']}
        require(len(profiles) == len(index['profiles']) and set(BINARY_SHA).issubset(profiles),
                'missing/duplicate accepted build profile')
        for name, hashes in BINARY_SHA.items():
            profile = profiles[name]
            actual = [x['sha256'] for x in profile.get('binaries', [])]
            if 'binary' in profile:
                actual.append(profile['binary']['sha256'])
            require(sorted(actual) == sorted(hashes), 'recorded binary identity changed: ' + name)
        for record in index['required_capsule_generated_content']:
            matches = [n for n, item in self.source.items() if item['sha256'] == record['sha256']
                       and n.startswith('accepted-generated/')]
            require(matches, 'accepted generated source absent from tar')
        owner = profiles['owner78-D030-actual-execution']
        final = owner['D031_followup']['final_parent_acceptance']
        require(isinstance(final, dict) and final.get('path') == self.args.d031_acceptance_path
                and final.get('sha256') == self.args.d031_acceptance_sha256,
                'build index owner78/D031 authority deferred or mismatched')
        require(owner['D031_followup']['accepted_supplement_source_hashes'] ==
                self.spec['expected_final_source'], 'build index final owner tools differ from spec')
        self.report['builds'] = {'profiles': len(profiles), 'accepted_binary_SHA_values': 9,
            'binary_bytes_read': 0, 'external_include_trees_read': 0}

    def classifications(self):
        evidence, matrix = self.record(EVIDENCE), self.record(MATRIX)
        authorities = {x['id']: x for x in evidence['authorities']}
        require(len(authorities) == len(evidence['authorities']), 'duplicate evidence authority')
        require(set(matrix['authority_document_sha256']) == set(authorities),
                'matrix/evidence authority closed sets differ')
        for name, record in authorities.items():
            actual = identity(self.lookup(record['path']))
            require(actual == {'sha256': record['sha256'], 'bytes': record['bytes']},
                    'accepted authority reference changed: ' + name)
            require(matrix['authority_document_sha256'].get(name) == record['sha256'],
                    'matrix/evidence authority disagreement: ' + name)
        rows = {x['id']: x for x in matrix['rows']}
        require(len(rows) == len(matrix['rows']), 'duplicate verification row')
        for name, row in rows.items():
            ids = row.get('authority_ids')
            require(isinstance(ids, list) and ids and len(ids) == len(set(ids))
                    and all(x in authorities for x in ids), 'unbound row authority: ' + name)
        allowed = {'new_passed', 'inherited_same_identity', 'failed',
                   'prerequisite_unavailable', 'not_applicable_with_reason'}
        require(all(x['status'] in allowed for x in rows.values()), 'nonterminal/invalid final matrix status')
        failed = {'default_FF_noise', 'network_AA_other_sizes_noise', 'D026_pool_stable_windows',
            'owner_original_checker_failure', 'owner_group01_preflight_failure',
            'W2_network_initial_link_failure', 'D031_approval_copy_offline_failure',
            'D033_owner_unqualified_subset'}
        unavailable = {'G3_owner-reuse', 'G3_new-staging-direct-read', 'G3_reduce-output-copy',
            'W1_network_ASan_runtime', 'native_Windows_storage_consumer', 'native_Windows_full_P2996',
            'independent_native_Linux_performance', 'real_business_consumer', 'default_DF_attribution'}
        for name in failed:
            require(rows[name]['status'] == 'failed', 'retained failure relabeled: ' + name)
        for name in unavailable:
            require(rows[name]['status'] == 'prerequisite_unavailable', 'unmet prerequisite relabeled: ' + name)
        expected_counts = {'mac_full_48_and_configure3': {'ctest': 48, 'configure': 3},
            'wire_504': {'saved': 84, 'cross_version': 420},
            'linux_sanitizer_26_plus2': {'first_run': 26, 'second_run': 2},
            'W2_network_diagnostics': {'processes': 32, 'valid_traces': 16},
            'default_FF_execution_audit': {'processes': 78},
            'network_AA_execution_audit': {'processes': 112},
            'D026_pool_execution_audit': {'processes': 88},
            'D027_owner_functional26': {'processes': 26, 'success': 18, 'expected_failure': 8},
            'D028_G3_decision_deliverable': {'candidate_directions': 3, 'implementations': 0,
                                          'experimental_rejections': 0, 'adoptions': 0}}
        for name, counts in expected_counts.items():
            require(all(rows[name].get('counts', {}).get(k) == v for k, v in counts.items()),
                    'accepted case/process counts changed: ' + name)
        require(rows['owner78_actual_measurement']['status'] == 'new_passed',
                'owner78 actual execution not finally accepted')
        require(rows['owner78_actual_measurement'].get('counts', {}).get('processes') == 78,
                'owner78 execution count missing or changed')
        owner_ids = {n for n, record in authorities.items() if
                     record['path'] == OWNER_AUTHORITY_PATH and record['sha256'] == OWNER_AUTHORITY_SHA}
        require(len(owner_ids) == 1 and owner_ids.issubset(
                rows['owner78_actual_measurement']['authority_ids']) and owner_ids.issubset(
                rows['D033_owner_unqualified_subset']['authority_ids']),
                'owner execution/partial qualification rows lack final D033 authority')
        require(rows['owner78_actual_measurement']['qualification_summary'] == {
                'qualified_cells': 7, 'uncertain_cells': 17, 'qualified_comparisons': 2,
                'nonself_qualified_comparisons': 1, 'uncertain_comparisons': 22,
                'all_multithread_comparisons_uncertain': True},
                'matrix erases owner partial qualification')
        g3 = self.record('docs/coordination/vm-followup/G3-acceptance.json')
        require(len(g3['candidates']) == 3 and all(x['decision'] == 'prerequisite_unmet_retain_F'
                for x in g3['candidates']) and g3['all_candidate_implementations'] == 0 and
                g3['experimental_rejections'] == 0 and g3['adoptions'] == 0, 'G3 retainF decision changed')
        counts = dict(Counter(x['status'] for x in rows.values()))
        reported = matrix['row_counts_by_classification']
        require(set(reported).issubset(allowed | {'pending_parent_acceptance'}) and
                all(type(v) is int and v >= 0 for v in reported.values()) and
                all(reported.get(k, 0) == counts.get(k, 0) for k in allowed) and
                reported.get('pending_parent_acceptance', 0) == 0,
                'matrix status totals inconsistent')
        self.report['classification'] = {'rows': len(rows), 'counts': counts,
            'retained_failed_axes': sorted(failed), 'unmet_prerequisites': sorted(unavailable),
            'large_external_evidence_followed': False, 'statistics_recomputed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capsule', type=Path, required=True)
    parser.add_argument('--manifest-sha256', required=True)
    parser.add_argument('--owner-acceptance-path', required=True)
    parser.add_argument('--owner-acceptance-sha256', required=True)
    parser.add_argument('--d031-acceptance-path', required=True)
    parser.add_argument('--d031-acceptance-sha256', required=True)
    parser.add_argument('--original-capsule-relative', default='docs/validation/vm-final/final-v1')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    capsule, output = args.capsule.absolute(), args.report.absolute()
    require(not output.exists() and not output.is_symlink(), 'report already exists')
    require(not output.resolve().is_relative_to(capsule.resolve()), 'report must be outside capsule')
    for value in [args.manifest_sha256, args.owner_acceptance_sha256, args.d031_acceptance_sha256]:
        digest(value)
    for value in [args.owner_acceptance_path, args.d031_acceptance_path, args.original_capsule_relative]:
        relative(value)
    report = {'schema_version': 1, 'role': 'independent_final_capsule_audit',
        'created_at': datetime.now(timezone.utc).isoformat(), 'status': 'failed',
        'parent_G4_accepted': False, 'capsule': str(capsule),
        'trust_anchors': {'manifest_sha256': args.manifest_sha256,
            'owner_acceptance_path': args.owner_acceptance_path,
            'owner_acceptance_sha256': args.owner_acceptance_sha256,
            'D031_acceptance_path': args.d031_acceptance_path,
            'D031_acceptance_sha256': args.d031_acceptance_sha256,
            'owner_source_increment_path': OWNER_SOURCE_INCREMENT_PATH,
            'owner_source_increment_sha256': OWNER_SOURCE_INCREMENT_SHA},
        'audit_script': {'path': str(Path(__file__).absolute()), 'identity': identity(Path(__file__))},
        'completed_phases': [], 'findings': [], 'new_native_workloads': 0,
        'compiler_or_test_or_Docker_or_SSH_invocations': 0,
        'historical_archives_or_external_binaries_read': 0,
        'scope': 'New source/metadata capsule bytes only; external accepted evidence referenced, not reread or reexecuted.'}
    phase = 'outer_and_metadata_closed_sets'
    try:
        c = Capsule(capsule, args, report)
        report['completed_phases'].append('outer_and_metadata_closed_sets')
        for name in ['source_archive', 'source_lineage', 'metadata_and_builds', 'classifications']:
            phase = name
            getattr(c, name)()
            report['completed_phases'].append(name)
        phase = 'same_capsule_bytes_at_end'
        c.unchanged_at_end()
        report['completed_phases'].append('same_capsule_bytes_at_end')
        report['status'] = 'passed_with_scope_limits'
    except Exception as error:
        report['findings'].append({'phase': phase,
            'type': type(error).__name__, 'message': str(error)})
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps({'status': report['status'], 'report': str(output),
                      'report_sha256': identity(output)['sha256'], 'findings': len(report['findings'])}))
    return 0 if report['status'] == 'passed_with_scope_limits' else 1


if __name__ == '__main__':
    sys.exit(main())
