"""Independent canonical-v1 reference. Python 3.10+, stdlib only; no timed code."""
from __future__ import annotations
import argparse
from bisect import bisect_left
from dataclasses import dataclass, field
import copy
import hashlib
import json
from pathlib import Path
import struct

U64 = (1 << 64) - 1
SEED = "xoffset-competitor-v1"
SCALES = {"S": 128, "M": 16384, "L": 131072}


@dataclass(eq=True)
class Attr:
    key: str
    value: bytes


@dataclass(eq=True)
class Record:
    id: int = 0
    revision: int = 0
    score: int = 0
    name: str = ""
    samples: list[int] = field(default_factory=list)
    payload: bytes = b""
    attrs: list[Attr] = field(default_factory=list)


@dataclass(eq=True)
class Snapshot:
    schema_version: int = 1
    epoch: int = 0
    records: list[Record] = field(default_factory=list)


@dataclass(eq=True)
class Query:
    sequence: int
    code: int
    id: int = 0
    key: str = ""


@dataclass(eq=True)
class QueryResult:
    sequence: int
    status: int
    values: tuple[int, ...] = (0,) * 8


@dataclass(eq=True)
class Mutation:
    sequence: int
    stage: int
    code: int
    id: int = 0
    expected: int = 0
    score: int = 0
    revision: int = 0
    text: str = ""
    samples: list[int] = field(default_factory=list)
    bytes: bytes = b""
    record: Record = field(default_factory=Record)
    epoch: int = 0


@dataclass(eq=True)
class StageExpected:
    stage: int
    mutation_count: int
    canonical_digest: bytes


@dataclass(eq=True)
class Trace:
    input_digest: bytes
    queries: list[Query] = field(default_factory=list)
    mutations: list[Mutation] = field(default_factory=list)
    stages: list[StageExpected] = field(default_factory=list)
    before: list[QueryResult] = field(default_factory=list)
    after: list[QueryResult] = field(default_factory=list)


@dataclass(frozen=True)
class Limits:
    max_bytes: int = 512 << 20
    max_records: int = 1048576
    max_sequence: int = 16777216
    max_string: int = 1 << 20
    max_blob: int = 64 << 20
    max_attrs: int = 4096
    max_queries: int = 4194304
    max_mutations: int = 2097152
    max_total_elements: int = 67108864


def _int(x, bits=64, signed=False):
    low = -(1 << (bits - 1)) if signed else 0
    high = (1 << (bits - (1 if signed else 0))) - 1
    if type(x) is not int or not low <= x <= high:
        raise ValueError(f"integer outside {'i' if signed else 'u'}{bits}")
    return x


def _text(x):
    if not isinstance(x, str) or "\0" in x:
        raise ValueError("text must be UTF-8 without NUL")
    return x.encode("utf-8", "strict")


def validate_snapshot(s: Snapshot, limits=Limits()):
    if type(s.schema_version) is not int or s.schema_version != 1:
        raise ValueError("unsupported schema version")
    _int(s.epoch)
    if len(s.records) > limits.max_records:
        raise ValueError("record limit")
    total = len(s.records)
    previous = 0
    for r in s.records:
        _int(r.id); _int(r.revision, 32); _int(r.score, 64, True)
        if r.id <= previous:
            raise ValueError("record ids must be positive and strictly ordered")
        previous = r.id
        if len(_text(r.name)) > limits.max_string or len(r.payload) > limits.max_blob:
            raise ValueError("field byte limit")
        if len(r.samples) > limits.max_sequence or len(r.attrs) > limits.max_attrs:
            raise ValueError("field count limit")
        for x in r.samples:
            _int(x, 32)
        last = None
        for a in r.attrs:
            key = _text(a.key)
            if last is not None and key <= last:
                raise ValueError("attribute keys must be strictly byte ordered")
            if len(key) > limits.max_string or len(a.value) > limits.max_blob:
                raise ValueError("attribute byte limit")
            last = key
        total += len(r.samples) + len(r.attrs)
        if total > limits.max_total_elements:
            raise ValueError("aggregate element limit")


class Writer:
    def __init__(self):
        self.b = bytearray()
    def raw(self, b):
        if len(b) > Limits().max_bytes-len(self.b): raise ValueError("encoded byte limit")
        self.b.extend(b)
    def u8(self, x): self.raw(struct.pack("<B", _int(x, 8)))
    def u32(self, x): self.raw(struct.pack("<I", _int(x, 32)))
    def u64(self, x): self.raw(struct.pack("<Q", _int(x)))
    def i64(self, x): self.raw(struct.pack("<q", _int(x, 64, True)))
    def blob(self, b): self.u64(len(b)); self.raw(b)
    def text(self, s): self.blob(_text(s))
    def finish(self):
        return bytes(self.b)


class Reader:
    def __init__(self, b, limits):
        self.b = memoryview(b); self.p = 0; self.limits = limits; self.elements = 0
        if len(b) > limits.max_bytes:
            raise ValueError("input byte limit")
    def raw(self, n):
        if n < 0 or n > len(self.b) - self.p:
            raise ValueError("truncated input")
        out = self.b[self.p:self.p+n]; self.p += n
        return out
    def u8(self): return struct.unpack("<B", self.raw(1))[0]
    def u32(self): return struct.unpack("<I", self.raw(4))[0]
    def u64(self): return struct.unpack("<Q", self.raw(8))[0]
    def i64(self): return struct.unpack("<q", self.raw(8))[0]
    def count(self, maximum, minimum=1):
        n = self.u64()
        if n > maximum or n > (len(self.b)-self.p)//minimum:
            raise ValueError("count limit or truncated collection")
        if n > self.limits.max_total_elements - self.elements:
            raise ValueError("aggregate element limit")
        self.elements += n
        return n
    def blob(self, maximum=None):
        n = self.u64()
        if n > (self.limits.max_blob if maximum is None else maximum):
            raise ValueError("blob limit")
        return bytes(self.raw(n))
    def text(self):
        s = self.blob(self.limits.max_string).decode("utf-8", "strict")
        _text(s)
        return s
    def end(self):
        if self.p != len(self.b):
            raise ValueError("trailing bytes")
    def magic(self, expected):
        if self.raw(8) != expected:
            raise ValueError("wrong magic")


def _write_record(w, r):
    w.u64(r.id); w.u32(r.revision); w.i64(r.score); w.text(r.name)
    w.u64(len(r.samples))
    w.raw(struct.pack(f"<{len(r.samples)}I",*r.samples))
    w.blob(r.payload); w.u64(len(r.attrs))
    for a in r.attrs: w.text(a.key); w.blob(a.value)


def _read_record(r):
    result = Record(r.u64(), r.u32(), r.i64(), r.text())
    result.samples = [r.u32() for _ in range(r.count(r.limits.max_sequence, 4))]
    result.payload = r.blob()
    result.attrs = [Attr(r.text(), r.blob()) for _ in range(r.count(r.limits.max_attrs, 16))]
    return result


def encode_canonical(s):
    validate_snapshot(s)
    w = Writer(); w.raw(b"SCCAN001"); w.u32(s.schema_version); w.u64(s.epoch); w.u64(len(s.records))
    for record in s.records: _write_record(w, record)
    return w.finish()


def decode_canonical(data, limits=Limits()):
    r = Reader(data, limits); r.magic(b"SCCAN001")
    s = Snapshot(r.u32(), r.u64())
    s.records = [_read_record(r) for _ in range(r.count(limits.max_records, 52))]
    r.end(); validate_snapshot(s, limits)
    return s


def _validate_results(results):
    if len(results) > Limits().max_queries:
        raise ValueError("query result limit")
    for i, q in enumerate(results):
        if q.sequence != i or q.status not in (0, 1) or len(q.values) != 8:
            raise ValueError("invalid query result")
        if q.status == 1 and any(q.values):
            raise ValueError("not_found result must be zero")


def _write_results(w, results):
    _validate_results(results)
    w.u64(len(results))
    for q in results:
        w.u64(q.sequence); w.u8(q.status)
        for value in q.values: w.u64(value)


def _read_results(r):
    out = [QueryResult(r.u64(), r.u8(), tuple(r.u64() for _ in range(8)))
           for _ in range(r.count(r.limits.max_queries, 73))]
    _validate_results(out)
    return out


def encode_query_results(results):
    w = Writer(); w.raw(b"SCQUERY1"); w.u32(1); _write_results(w, results)
    return w.finish()


def decode_query_results(data, limits=Limits()):
    r = Reader(data, limits); r.magic(b"SCQUERY1")
    if r.u32() != 1: raise ValueError("query version")
    result = _read_results(r); r.end()
    return result


def encode_query_pair(before, after):
    w = Writer(); w.raw(b"SCQPAIR1"); w.u32(1)
    w.blob(encode_query_results(before)); w.blob(encode_query_results(after))
    return w.finish()


def decode_query_pair(data, limits=Limits()):
    r = Reader(data, limits); r.magic(b"SCQPAIR1")
    if r.u32() != 1: raise ValueError("query pair version")
    before = decode_query_results(r.blob(limits.max_bytes), limits)
    after = decode_query_results(r.blob(limits.max_bytes), limits)
    r.end(); return before, after


def _validate_trace(t):
    if len(t.input_digest) != 32 or len(t.before) != len(t.queries) or len(t.after) != len(t.queries):
        raise ValueError("trace digest/result length")
    if len(t.queries) > Limits().max_queries or len(t.mutations) > Limits().max_mutations:
        raise ValueError("trace operation limit")
    for i, q in enumerate(t.queries):
        if q.sequence != i or q.code not in (1, 2, 3): raise ValueError("query order/code")
        if q.code != 2 and q.key: raise ValueError("query key unused")
        if q.code == 3 and q.id: raise ValueError("scan id unused")
        _text(q.key); _int(q.id)
    counts = [0]*9; last = 1
    for i, m in enumerate(t.mutations):
        if m.sequence != i or not 1 <= m.code <= 9 or m.stage != m.code or m.stage < last:
            raise ValueError("mutation order/code/stage")
        if m.expected not in (0, 1) or (m.id == 0) != (m.code == 9):
            raise ValueError("mutation status/id")
        if len(m.samples)>Limits().max_sequence or len(m.bytes)>Limits().max_blob:
            raise ValueError("mutation field limit")
        if m.code in (2,5,6) and len(_text(m.text))>Limits().max_string:
            raise ValueError("mutation text limit")
        if m.code == 8:
            if m.id != m.record.id: raise ValueError("append id mismatch")
            validate_snapshot(Snapshot(records=[m.record]))
        counts[m.stage-1] += 1; last = m.stage
    if len(t.stages) != 9 or [x.stage for x in t.stages] != list(range(1, 10)):
        raise ValueError("nine stage contracts required")
    for s, count in zip(t.stages, counts):
        if s.mutation_count != count or len(s.canonical_digest) != 32:
            raise ValueError("stage contract count/digest")


def encode_trace(t):
    _validate_trace(t)
    w = Writer(); w.raw(b"SCTRAC01"); w.u32(1); w.raw(t.input_digest); w.u64(len(t.queries))
    for q in t.queries: w.u64(q.sequence); w.u8(q.code); w.u64(q.id); w.text(q.key)
    w.u64(len(t.mutations))
    for m in t.mutations:
        w.u64(m.sequence); w.u32(m.stage); w.u8(m.code); w.u64(m.id); w.u8(m.expected)
        if m.code == 1: w.i64(m.score); w.u32(m.revision)
        elif m.code == 2: w.text(m.text)
        elif m.code == 3:
            w.u64(len(m.samples))
            for x in m.samples: w.u32(x)
        elif m.code == 4: w.blob(m.bytes)
        elif m.code in (5, 6): w.text(m.text); w.blob(m.bytes)
        elif m.code == 8: _write_record(w, m.record)
        elif m.code == 9: w.u64(m.epoch)
    w.u64(len(t.stages))
    for s in t.stages: w.u32(s.stage); w.u64(s.mutation_count); w.raw(s.canonical_digest)
    _write_results(w, t.before); _write_results(w, t.after)
    return w.finish()


def decode_trace(data, limits=Limits()):
    r = Reader(data, limits); r.magic(b"SCTRAC01")
    if r.u32() != 1: raise ValueError("trace version")
    t = Trace(bytes(r.raw(32)))
    t.queries = [Query(r.u64(), r.u8(), r.u64(), r.text()) for _ in range(r.count(limits.max_queries, 25))]
    for _ in range(r.count(limits.max_mutations, 22)):
        m = Mutation(r.u64(), r.u32(), r.u8(), r.u64(), r.u8())
        if m.code == 1: m.score = r.i64(); m.revision = r.u32()
        elif m.code == 2: m.text = r.text()
        elif m.code == 3: m.samples = [r.u32() for _ in range(r.count(limits.max_sequence, 4))]
        elif m.code == 4: m.bytes = r.blob()
        elif m.code in (5, 6): m.text = r.text(); m.bytes = r.blob()
        elif m.code == 8: m.record = _read_record(r)
        elif m.code == 9: m.epoch = r.u64()
        elif m.code != 7: raise ValueError("unknown mutation")
        t.mutations.append(m)
    t.stages = [StageExpected(r.u32(), r.u64(), bytes(r.raw(32))) for _ in range(r.count(9, 44))]
    t.before = _read_results(r); t.after = _read_results(r); r.end(); _validate_trace(t)
    return t


def _record_pos(s, ident): return bisect_left(s.records, ident, key=lambda x: x.id)


def evaluate_queries(s, queries):
    out = []
    for q in queries:
        v = [0]*8; status = 0
        if q.code == 3:
            v = [len(s.records), sum(x.id for x in s.records), sum(x.revision for x in s.records),
                 sum(x.score for x in s.records), sum(len(x.name.encode("utf-8")) for x in s.records),
                 sum(sum(x.samples) for x in s.records), sum(len(x.payload) for x in s.records),
                 sum(len(x.attrs) for x in s.records)]
        else:
            at = _record_pos(s, q.id)
            if at == len(s.records) or s.records[at].id != q.id: status = 1
            elif q.code == 1:
                x = s.records[at]; name = x.name.encode("utf-8")
                v = [x.revision, x.score, len(name), sum(name), len(x.samples), sum(x.samples),len(x.payload),sum(x.payload)]
            elif q.code == 2:
                attrs = s.records[at].attrs; key = q.key.encode("utf-8")
                j = bisect_left(attrs, key, key=lambda a: a.key.encode("utf-8"))
                if j == len(attrs) or attrs[j].key != q.key: status = 1
                else:
                    b = attrs[j].value; v = [len(b),sum(b),b[0] if b else 0,b[-1] if b else 0,0,0,0,0]
            else: raise ValueError("query code")
        out.append(QueryResult(q.sequence, status, tuple(x & U64 for x in v)))
    return out


query_reference = evaluate_queries


def apply_mutation(s, m):
    if m.code == 9:
        s.epoch = _int(m.epoch); return 0
    p = _record_pos(s, m.id)
    found = p != len(s.records) and s.records[p].id == m.id
    if m.code == 8:
        if found: raise ValueError("append existing record")
        s.records.insert(p, copy.deepcopy(m.record)); return 0
    if not found: return 1
    r = s.records[p]
    if m.code == 1: r.score = _int(m.score, 64, True); r.revision = _int(m.revision, 32)
    elif m.code == 2: _text(m.text); r.name = m.text
    elif m.code == 3: r.samples.extend(m.samples)
    elif m.code == 4: r.payload = m.bytes
    elif m.code in (5, 6):
        key = _text(m.text); at = bisect_left(r.attrs, key, key=lambda a: a.key.encode("utf-8"))
        exists = at != len(r.attrs) and r.attrs[at].key == m.text
        if m.code == 5:
            if not exists: return 1
            r.attrs[at].value = m.bytes
        else:
            if exists: raise ValueError("insert existing attribute")
            r.attrs.insert(at, Attr(m.text, m.bytes))
    elif m.code == 7: del s.records[p]
    else: raise ValueError("mutation code")
    return 0


apply_reference = apply_mutation


def expand(domain, seed, ident, field_name, size):
    if type(size) is not int or not 0 <= size <= Limits().max_blob:
        raise ValueError("invalid expansion size")
    prefix = [domain.encode("utf-8"), seed.encode("utf-8"), struct.pack("<Q", _int(ident)), field_name.encode("utf-8")]
    out = bytearray()
    for counter in range((size+31)//32):
        components = prefix + [struct.pack("<Q", counter)]
        message = b"".join(struct.pack("<Q", len(x))+x for x in components)
        out.extend(hashlib.sha256(message).digest())
    return bytes(out[:size])


def _letters(domain, seed, ident, name, size):
    return "".join(chr(97+x%26) for x in expand(domain, seed, ident, name, size))


def make_record(ident, seed=SEED, domain="initial-v1"):
    draw = lambda name, n: expand(domain, seed, ident, name, n)
    length = 8 + int.from_bytes(draw("name-length", 8), "little") % 57
    return Record(ident, int.from_bytes(draw("revision", 4),"little") % 1024,
                  int.from_bytes(draw("score",8),"little") % 2000001 - 1000000,
                  _letters(domain,seed,ident,"name",length),
                  list(struct.unpack("<32I",draw("samples",128))),draw("payload",128),
                  [Attr(f"k{i:04}",draw(f"attr/k{i:04}",16)) for i in range(4)])


def generate_snapshot(n, seed=SEED):
    if type(n) is not int or not 0 <= n <= Limits().max_records:
        raise ValueError("invalid fixture size")
    return Snapshot(1,1,[make_record(i,seed) for i in range(1,n+1)])


def generate_trace(snapshot, seed=SEED):
    validate_snapshot(snapshot)
    n = len(snapshot.records)
    if [r.id for r in snapshot.records] != list(range(1,n+1)):
        raise ValueError("generator requires initial contiguous ids")
    perm = sorted(range(1,n+1),key=lambda i:(expand("permutation-v1",seed,i,"rank",32),i))
    ids = perm + list(range(n+1,n+n//8+1))
    queries = []
    for i in ids:
        for code,key in ((1,""),(2,"k0001"),(2,"k9999")):
            queries.append(Query(len(queries),code,i,key))
    for i in (0,n+n//8+1): queries.append(Query(len(queries),1,i))
    queries.append(Query(len(queries),3))
    t = Trace(hashlib.sha256(encode_canonical(snapshot)).digest(),queries)
    t.before = evaluate_queries(snapshot,queries)
    s = copy.deepcopy(snapshot); quarter=n//4
    for stage in range(1,10):
        count_before = len(t.mutations)
        targets = perm[(stage-1)*quarter:stage*quarter] if stage <= 4 else perm[:quarter]
        if stage == 7: targets = perm[n-n//8:] if n//8 else []
        if stage == 8: targets = list(range(n+1,n+n//8+1))
        if stage == 9: targets = [0]
        for ident in targets:
            m=Mutation(len(t.mutations),stage,stage,ident)
            draw=lambda name,length: expand("mutation-v1",seed,ident,name,length)
            if stage == 1:
                old=s.records[_record_pos(s,ident)]; m.score=old.score+1000001; m.revision=old.revision+1
            elif stage == 2: m.text=_letters("mutation-v1",seed,ident,"name",128)
            elif stage == 3: m.samples=list(struct.unpack("<32I",draw("samples",128)))
            elif stage == 4: m.bytes=draw("payload",384)
            elif stage == 5: m.text="k0001"; m.bytes=draw("attr/k0001",32)
            elif stage == 6: m.text="k0004"; m.bytes=draw("attr/k0004",32)
            elif stage == 8: m.record=make_record(ident,seed,"append-v1")
            elif stage == 9: m.epoch=s.epoch+1
            if apply_mutation(s,m) != m.expected: raise ValueError("generated operation precondition")
            t.mutations.append(m)
        t.stages.append(StageExpected(stage,len(t.mutations)-count_before,hashlib.sha256(encode_canonical(s)).digest()))
    t.after=evaluate_queries(s,queries)
    return t,s


def generate_fixture(out: Path, n: int, seed: str = SEED):
    out=Path(out)
    if out.exists(): raise ValueError("fixture output must be a new directory")
    snapshot=generate_snapshot(n,seed); trace,final=generate_trace(snapshot,seed)
    payload={"input.canonical.bin":encode_canonical(snapshot),"trace.bin":encode_trace(trace),
             "final.canonical.bin":encode_canonical(final),"query-before.bin":encode_query_results(trace.before),
             "query-after.bin":encode_query_results(trace.after)}
    manifest={"schema":"sc-fixture-v1","model_version":1,"n":n,"seed":seed,
              "trace_name":"mixed-growth-v1","query_name":"query-v1",
              "input_digest":trace.input_digest.hex(),"final_digest":hashlib.sha256(payload["final.canonical.bin"]).hexdigest(),
              "query_before_digest":hashlib.sha256(payload["query-before.bin"]).hexdigest(),
              "query_after_digest":hashlib.sha256(payload["query-after.bin"]).hexdigest(),
              "query_count":len(trace.queries),"mutation_count":len(trace.mutations),
              "stages":[{"stage":s.stage,"mutation_count":s.mutation_count,"canonical_sha256":s.canonical_digest.hex()} for s in trace.stages],
              "files":{name:{"sha256":hashlib.sha256(b).hexdigest(),"bytes":len(b)} for name,b in payload.items()}}
    out.mkdir(parents=True)
    for name,b in payload.items(): (out/name).write_bytes(b)
    (out/"fixture.json").write_text(json.dumps(manifest,ensure_ascii=False,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument("--out",required=True,type=Path)
    group=p.add_mutually_exclusive_group(required=True); group.add_argument("--n",type=int); group.add_argument("--scale",choices=SCALES)
    p.add_argument("--seed",default=SEED); args=p.parse_args()
    result=generate_fixture(args.out,args.n if args.n is not None else SCALES[args.scale],args.seed)
    print(json.dumps(result,sort_keys=True))


if __name__ == "__main__": main()
