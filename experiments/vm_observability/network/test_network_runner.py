"""Artifact/ledger negative controls; no native workload execution."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
subject={'__file__':str(HERE/'run_network_observation.py'),'__name__':'network_runner_test_subject'}
exec(compile((HERE/'run_network_observation.py').read_text(),str(HERE/'run_network_observation.py'),'exec'),subject)

class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
    def test_output_escape_rejected(self):
        with self.assertRaisesRegex(ValueError,'budget root'):subject['bounded_out'](self.root/'outside',self.root/'owned')
    def test_symlink_escape_rejected(self):
        (self.root/'owned').mkdir();(self.root/'outside').mkdir();(self.root/'owned/link').symlink_to(self.root/'outside',target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'budget root'):subject['bounded_out'](self.root/'owned/link/output',self.root/'owned')
    def test_wire_exact_and_mutated(self):
        a=self.root/'a';b=self.root/'b';a.write_bytes(bytes(range(256))*100);b.write_bytes(a.read_bytes());subject['same_file'](a,b)
        b.write_bytes(b'X'+b.read_bytes()[1:])
        with self.assertRaisesRegex(ValueError,'wire'):subject['same_file'](a,b)
    def test_sealed_raw_mutation(self):
        raw=self.root/'raw.stdout';raw.write_text('{}\n');subject['seal'](self.root,{'schema':'n3-network-artifact-v1','kind':'test'});subject['audit_files'](self.root)
        raw.write_text('{"timing_evidence":true}\n')
        with self.assertRaisesRegex(ValueError,'inventory'):subject['audit_files'](self.root)
    def test_sealed_missing_and_added_case(self):
        raw=self.root/'raw';raw.write_text('wire');subject['seal'](self.root,{'schema':'n3-network-artifact-v1','kind':'test'});raw.unlink()
        with self.assertRaisesRegex(ValueError,'inventory'):subject['audit_files'](self.root)
        raw.write_text('wire');(self.root/'extra-case').write_text('unexpected')
        with self.assertRaisesRegex(ValueError,'inventory'):subject['audit_files'](self.root)
    def test_fixture_raw_flags_and_geometry(self):
        obs=dict(schema='n3-network-raw-v1',diagnostic=True,timing_evidence=False,joined=True,command='fixture',elements=1024,frame_bytes=5000,page_size=16384)
        subject['fixture_observation'](obs,{'frame_bytes':5000},4096)
        for key,value in [('joined',False),('timing_evidence',True),('elements',True),('page_size',True),('frame_bytes',5001)]:
            mutated=dict(obs);mutated[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):subject['fixture_observation'](mutated,{'frame_bytes':5000},4096)
    def ledger_fixture(self):
        identity=dict(original_collection_path='/saved/collection',original_binary_path='/saved/build/n3_network',reference_binary_path='/saved/w1/net_reference')
        subject['write'](self.root/'identity.json',identity);(self.root/'cases').mkdir();attempts=[]
        cases=['fixture-'+str(n) for n in subject['NOMINALS']]+[str(n)+'-'+pool for n in subject['NOMINALS'] for pool in subject['POOLS']]
        for case in cases:
            commands=[]
            for argv,profile in zip(subject['expected_commands'](self.root,case),('reference','diagnostic')):
                i=len(attempts);commands.append(dict(argv=argv,returncode=0,attempt_index=i));attempts.append(dict(index=i,case=case,profile=profile,command=argv,status='complete',returncode=0))
            p=self.root/'cases'/case;p.mkdir();subject['write'](p/'result.json',dict(commands=commands))
        state=dict(schema='n3-network-workload-ledger-v1',max_attempts=32,attempts=attempts);subject['write'](self.root/'workload-ledger.json',state);return state
    def test_all_32_ledger_bindings(self):
        self.ledger_fixture();subject['audit_launches'](self.root)
    def test_missing_ledger_attempt(self):
        d=self.ledger_fixture();d['attempts'].pop();subject['write'](self.root/'workload-ledger.json',d)
        with self.assertRaisesRegex(ValueError,'attempts'):subject['audit_launches'](self.root)
    def test_wrong_profile_or_binary_or_case(self):
        original=self.ledger_fixture()
        for key,value in [('profile','other'),('case','4096-warm'),('command',['wrong-binary']),('returncode',1)]:
            d=copy.deepcopy(original);d['attempts'][0][key]=value;subject['write'](self.root/'workload-ledger.json',d)
            with self.subTest(key=key),self.assertRaises(ValueError):subject['audit_launches'](self.root)
    def test_duplicate_workload_index(self):
        self.ledger_fixture();p=self.root/'cases/fixture-4096/result.json';d=subject['read'](p);d['commands'][1]['attempt_index']=0;subject['write'](p,d)
        with self.assertRaises(ValueError):subject['audit_launches'](self.root)
    def test_timing_removed_semantics_preserved(self):
        a={'semantic_checksum':'32','samples':[{'end_to_end_ns':1,'output_bytes':4096}]};b=copy.deepcopy(a);b['samples'][0]['end_to_end_ns']=2
        self.assertEqual(subject['semantics'](a),subject['semantics'](b));b['samples'][0]['output_bytes']=4097;self.assertNotEqual(subject['semantics'](a),subject['semantics'](b))

if __name__=='__main__':unittest.main()
