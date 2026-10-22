"""Independent model/encoding contract tests; no candidate library or timing."""
from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
SPEC=importlib.util.spec_from_file_location("sc_reference",ROOT/"model"/"reference.py")
R=importlib.util.module_from_spec(SPEC);sys.modules[SPEC.name]=R;SPEC.loader.exec_module(R)


class Contracts(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.out=Path(self.temp.name)

    def test_empty_manual_encoding(self):
        expected=b"SCCAN001"+struct.pack("<IQQ",1,0,0)
        self.assertEqual(R.encode_canonical(R.Snapshot()),expected)
        self.assertEqual(R.decode_canonical(expected),R.Snapshot())

    def test_one_manual_encoding(self):
        s=R.Snapshot(1,9,[R.Record(1,0xffffffff,-2,"A",[0,0xffffffff],b"\0\xff",[R.Attr("",b"z")])])
        blob=lambda b:struct.pack("<Q",len(b))+b
        expected=(b"SCCAN001"+struct.pack("<IQQQIq",1,9,1,1,0xffffffff,-2)+blob(b"A")+
                  struct.pack("<QII",2,0,0xffffffff)+blob(b"\0\xff")+struct.pack("<Q",1)+blob(b"")+blob(b"z"))
        self.assertEqual(R.encode_canonical(s),expected);self.assertEqual(R.decode_canonical(expected),s)

    def test_unicode_and_unsigned_key_order(self):
        s=R.Snapshot(records=[R.Record(id=1,name="é😀",attrs=[R.Attr("z",b""),R.Attr("é",b"\0")])])
        self.assertEqual(R.decode_canonical(R.encode_canonical(s)),s)
        s.records[0].attrs.reverse()
        with self.assertRaises(ValueError):R.encode_canonical(s)

    def test_integer_boundaries(self):
        minimal=R.Snapshot(records=[R.Record(id=1)])
        self.assertEqual(len(R.encode_canonical(minimal)),80)
        self.assertEqual(R.decode_canonical(R.encode_canonical(minimal)),minimal)
        for score in (-(1<<63),(1<<63)-1):
            s=R.Snapshot(epoch=R.U64,records=[R.Record(id=R.U64,revision=(1<<32)-1,score=score,samples=[(1<<32)-1])])
            self.assertEqual(R.decode_canonical(R.encode_canonical(s)),s)
        for field,value in (("id",-1),("score",1<<63),("revision",1<<32)):
            s=R.Snapshot(records=[R.Record(id=1)]);setattr(s.records[0],field,value)
            with self.assertRaises(ValueError):R.encode_canonical(s)

    def test_duplicate_and_zero_ids(self):
        for records in ([R.Record(id=0)],[R.Record(id=1),R.Record(id=1)],[R.Record(id=2),R.Record(id=1)]):
            with self.assertRaises(ValueError):R.encode_canonical(R.Snapshot(records=records))

    def test_invalid_utf8_nul_and_surrogates(self):
        raw=bytearray(R.encode_canonical(R.Snapshot(records=[R.Record(id=1,name="ab")])) )
        for invalid in (b"\0b",b"\xc0\x80",b"\xff\xff",b"\xe2\x82"):
            bad=raw[:];bad[56:58]=invalid
            with self.assertRaises((ValueError,UnicodeError)):R.decode_canonical(bad)
        with self.assertRaises(UnicodeError):R.encode_canonical(R.Snapshot(records=[R.Record(id=1,name="\ud800")]))

    def test_truncations_and_trailing(self):
        raw=R.encode_canonical(R.Snapshot(records=[R.Record(id=1,name="x",samples=[2],attrs=[R.Attr("a",b"b")])]))
        for n in range(len(raw)):
            with self.assertRaises(ValueError):R.decode_canonical(raw[:n])
        with self.assertRaises(ValueError):R.decode_canonical(raw+b"\0")

    def test_huge_count_rejected_before_record_reader(self):
        raw=b"SCCAN001"+struct.pack("<IQQ",1,0,R.U64)
        with self.assertRaises(ValueError):R.decode_canonical(raw)
        raw=R.encode_canonical(R.Snapshot(records=[R.Record(id=1)]))
        for limits in (R.Limits(max_records=0),R.Limits(max_total_elements=0),R.Limits(max_bytes=20)):
            with self.assertRaises(ValueError):R.decode_canonical(raw,limits)

    def test_query_modular_and_missing(self):
        s=R.Snapshot(records=[R.Record(id=1,score=-1,name="é",samples=[0xffffffff,1],payload=b"\0\xff",attrs=[R.Attr("x",b"")])])
        queries=[R.Query(0,1,1),R.Query(1,1,9),R.Query(2,2,1,"x"),R.Query(3,2,1,"missing"),R.Query(4,3)]
        results=R.evaluate_queries(s,queries)
        self.assertEqual(results[0].values,(0,R.U64,2,364,2,1<<32,2,255))
        self.assertEqual(results[1],R.QueryResult(1,1));self.assertEqual(results[2],R.QueryResult(2,0))
        self.assertEqual(results[3],R.QueryResult(3,1));self.assertEqual(results[4].values[3],R.U64)
        self.assertEqual(R.decode_query_results(R.encode_query_results(results)),results)
        pair=R.encode_query_pair(results,results)
        self.assertEqual(R.decode_query_pair(pair),(results,results))

    def test_missing_mutation_does_not_change(self):
        s=R.generate_snapshot(1);before=R.encode_canonical(s)
        self.assertEqual(R.apply_mutation(s,R.Mutation(0,1,1,999,score=2)),1)
        self.assertEqual(R.apply_mutation(s,R.Mutation(0,5,5,1,text="missing",bytes=b"x")),1)
        self.assertEqual(R.encode_canonical(s),before)
        with self.assertRaises(ValueError):R.apply_mutation(s,R.Mutation(0,6,6,1,text="k0000"))

    def test_generator_repeat_and_all_stages(self):
        for n in (0,1,17,128,257):
            s=R.generate_snapshot(n);t,final=R.generate_trace(s)
            self.assertEqual(R.decode_trace(R.encode_trace(t)),t)
            self.assertEqual(R.encode_canonical(s),R.encode_canonical(R.generate_snapshot(n)))
            self.assertEqual(len(t.queries),3*(n+n//8)+3)
            self.assertEqual(len(final.records),n);self.assertEqual(final.epoch,2)
            self.assertEqual([x.mutation_count for x in t.stages],[n//4]*6+[n//8,n//8,1])
            current=copy.deepcopy(s)
            for stage in t.stages:
                for m in t.mutations:
                    if m.stage==stage.stage:self.assertEqual(R.apply_mutation(current,m),m.expected)
                self.assertEqual(hashlib.sha256(R.encode_canonical(current)).digest(),stage.canonical_digest)
            self.assertEqual(current,final)

    def test_trace_structural_rejections(self):
        s=R.generate_snapshot(17);t,_=R.generate_trace(s)
        for mutate in (lambda x:setattr(x.queries[0],"sequence",9),lambda x:setattr(x.mutations[0],"stage",2),
                       lambda x:setattr(x.stages[0],"mutation_count",99),lambda x:setattr(x.before[0],"status",2)):
            bad=copy.deepcopy(t);mutate(bad)
            with self.assertRaises(ValueError):R.encode_trace(bad)
        raw=R.encode_trace(t)
        for bad in (raw[:52],raw+b"x",raw[:44]+struct.pack("<Q",R.U64)+raw[52:]):
            with self.assertRaises(ValueError):R.decode_trace(bad)
        with self.assertRaises(ValueError):R.decode_trace(raw,R.Limits(max_queries=0))

    def test_fixture_no_overwrite_and_manifest(self):
        manifest=R.generate_fixture(self.out/"fixture",17)
        for name,identity in manifest["files"].items():
            raw=(self.out/"fixture"/name).read_bytes()
            self.assertEqual(len(raw),identity["bytes"]);self.assertEqual(hashlib.sha256(raw).hexdigest(),identity["sha256"])
        self.assertEqual(json.loads((self.out/"fixture"/"fixture.json").read_text()),manifest)
        with self.assertRaises(ValueError):R.generate_fixture(self.out/"fixture",17)

    def test_committed_goldens(self):
        for label in ("0","1","17"):
            original=ROOT/"fixtures"/f"golden-{label}"
            manifest=json.loads((original/"fixture.json").read_text())
            regenerated=R.generate_fixture(self.out/f"repeat-{label}",int(label))
            self.assertEqual(manifest,regenerated)
            for name in list(manifest["files"])+["fixture.json"]:
                self.assertEqual((original/name).read_bytes(),(self.out/f"repeat-{label}"/name).read_bytes())

    def test_scale_and_expand_contract(self):
        self.assertEqual(R.SCALES,{"S":128,"M":16384,"L":131072})
        for n in (0,1,31,32,33,65):
            data=R.expand("initial-v1",R.SEED,7,"payload",n)
            self.assertEqual(len(data),n);self.assertEqual(data,R.expand("initial-v1",R.SEED,7,"payload",128)[:n])
        self.assertNotEqual(R.expand("initial-v1",R.SEED,7,"payload",32),R.expand("append-v1",R.SEED,7,"payload",32))
        for n in (-1,True,1.5):
            with self.assertRaises(ValueError):R.expand("x",R.SEED,1,"f",n)


if __name__=="__main__":unittest.main()
