"""Frozen range-index candidate schedule and statistics. No native execution."""
from __future__ import annotations

import hashlib
import math
from statistics import median

try:
    from . import core as C
except ImportError:
    import core as C

LIBRARIES = ("xoffset", "cista", "flatbuffers", "capnproto", "cereal")
IDENTITIES = {
    "xoffset":("xoffset-receive-completion-v1","7539433ab96d69c4a07b1db5c0a13786da85cdcd"),
    "cista":("cista-v0.16","015b94e66c4c0d7c1ef29742383a024c84fbbba1"),
    "flatbuffers":("flatbuffers-v25.12.19","7e163021e59cca4f8e1e35a7c828b5c6b7915953"),
    "capnproto":("capnproto-v1.5.0","373e61ec89e2359f1c362e9b2eadc552f4779306"),
    "cereal":("cereal-v1.3.2","ebef1e929807629befafbb2918ea1a08c7194554")}
CANDIDATE_CONTENT_SHA256 = "26299f796e2ba289ab15b38b000a785ad1877fbc65ce29cf3071d2118804c3b4"
# The commit above is the base commit, not a claim that the candidate was committed.
# Singleton regular-file closure records are independently enforced at capture
# and in relocated offline audits, even after an ordinary inventory is re-sealed.
CANDIDATE_CLOSURE_IDS = {'candidate-source-identity': {'bytes': '2969', 'sha256': '48f313f3481f516e49d6fd46a5a60362d1680e4736a9b493a94cf72548483de6'}, 'candidate-receive-future-hpp': {'bytes': '3052', 'sha256': '42b2f9aac5286a3c56d62d706bf8d4528bb94eb5c3f27687464a0bcb2e306651'}, 'candidate-receive-raw-hpp': {'bytes': '50646', 'sha256': 'a6a5f4176b6c94acaa54999c01a64ad8f4570855005eef4b506d17647a1ba3a0'}, 'candidate-receive-raw-fwd-hpp': {'bytes': '4997', 'sha256': '1933345bb64eb363a435aff86745318dcb97d4c72d50754674c64db8bbdcd9ca'}, 'candidate-snapshot-index-runtime-cpp': {'bytes': '525', 'sha256': '1208ff468efddeacc86d4e7f5880f106307a4dd348ff4e7ff2883806d509a79b'}, 'candidate-validation-scratch-cpp': {'bytes': '408', 'sha256': 'b16223714ed972aa17c7f8243216930969bfbaab58398b3de5e85ec08f5b6fcd'}, 'candidate-validation-scratch-hpp': {'bytes': '15756', 'sha256': 'edaf40d8cb472acfaddca5938bbb3dbf4c31a54d4c8fa03cea6b25dc0084592c'}, 'candidate-xoffset-core-lifetime-hpp': {'bytes': '7269', 'sha256': '1fed92b34c842b14905502db4f35ff93e66b4d102e59b7e37df3969db8e8f268'}, 'candidate-xoffset-snapshot-index-hpp': {'bytes': '11831', 'sha256': '6286b20288bd43cff22006c5169eab3585960276004a526e32974bf3dcbf63f9'}, 'candidate-xoffset-snapshot-index-budget-hpp': {'bytes': '3042', 'sha256': '724dba8fee57faa6f19d596e2726cc5198a3d5f1054867f5014d9db8f7f02b4d'}, 'candidate-xoffset-validation-scratch-hpp': {'bytes': '1094', 'sha256': '63e673b17d9ba38bda4ade0effe9ec15cf3ac367fa8c5f18c189277211ebddb3'}, 'candidate-xoffset-verified-paths-hpp': {'bytes': '10209', 'sha256': 'a3e69dc13d595bbe94955e0a940b1def5f07b76300e2d5d84242d4d1383e3512'}, 'candidate-xoffset-verified-receive-hpp': {'bytes': '15907', 'sha256': 'f92b2bff7b812ef4bb53687c093ed0e73d13bf1b0defaf282447a5ad02abab4a'}, 'candidate-os-header': {'bytes': '20900', 'sha256': '90105e4c172da26a5b23cbb10021a18613b458e92b795beee8ec9d7faed72991'}, 'candidate-main-header': {'bytes': '278262', 'sha256': '66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3'}}
CELL = "V1-M-mixed-growth-full-validation-four-v1"
ANCHORS = ("pre", "mid", "post")
POLICY = dict(maximum_children=285, maximum_window_seconds=2700,
              maximum_child_seconds=20, maximum_new_wire_bytes=str(16*1024**3),
              bootstrap_resamples=10000, target_ratio=1.05, anchor_ratio=1.02,
              initial_records=16384, no_retry=True, whole_cell_qualification=True)


def schedule():
    rows = []

    def add(library, kind, **extra):
        rows.append(dict(id="j%03d" % len(rows), library=library, kind=kind, **extra))

    for lib in LIBRARIES:
        for side in ("A", "B"):
            add(lib, "warmup-aa", anchor="warmup", pair=0, side=side, order="AB")
    for lib in LIBRARIES:
        add(lib, "warmup-comparison", block=-1, position=LIBRARIES.index(lib))

    def anchor(name):
        for pair in range(4):
            order = "AB" if pair % 2 == 0 else "BA"
            # Rotate libraries across pairs and anchors; each pair stays adjacent.
            shift = (pair + ANCHORS.index(name)) % 5
            for lib in LIBRARIES[shift:] + LIBRARIES[:shift]:
                for side in order:
                    add(lib, "aa", anchor=name, pair=pair, side=side, order=order)

    def blocks(start):
        for block in range(start, start+15):
            shift = block % 5
            for position, lib in enumerate(LIBRARIES[shift:] + LIBRARIES[:shift]):
                add(lib, "comparison", block=block, position=position)

    anchor("pre")
    blocks(0)
    anchor("mid")
    blocks(15)
    anchor("post")
    C.require(len(rows) == POLICY["maximum_children"], "internal schedule count mismatch")
    return rows


def mad(values):
    center = median(values)
    return median(abs(x-center) for x in values)


def _pair_stats(pairs):
    logs = [math.log(a/b) for a,b,_order in pairs]
    ratios = [a/b for a,b,_order in pairs]
    strata = {order: median(a/b for a,b,o in pairs if o == order) for order in ("AB", "BA")}
    mid = median(ratios)
    scaled = 1.4826 * mad(logs)
    return dict(paired_ratios=ratios, median_ratio=mid, scaled_log_mad=scaled,
                order_medians=strata,
                qualified=0.98 <= mid <= 1.02 and scaled <= math.log(1.02)
                and all(0.98 <= v <= 1.02 for v in strata.values()))


def anchor_gates(samples, completed):
    C.require(completed in ANCHORS, "unknown completed anchor")
    selected = ANCHORS[:ANCHORS.index(completed)+1]
    result = {}
    for lib in LIBRARIES:
        local, all_pairs, centers = {}, [], {"pooled":[], "A":[], "B":[]}
        for name in selected:
            rows = [r for r in samples if r["job"]["kind"] == "aa" and r["job"]["library"] == lib and r["job"]["anchor"] == name]
            C.require(len(rows) == 8, "incomplete anchor")
            pairs = []
            sides = {"A":[], "B":[]}
            for pair in range(4):
                members = [r for r in rows if r["job"]["pair"] == pair]
                C.require(len(members) == 2 and {r["job"]["side"] for r in members} == {"A","B"}, "invalid AA pair")
                values = {r["job"]["side"]: C.decimal(r["total_ns"], positive=True) for r in members}
                order = "AB" if pair % 2 == 0 else "BA"
                C.require(all(r["job"]["order"] == order for r in members), "AA order mismatch")
                pairs.append((values["A"], values["B"], order))
                for side in sides:
                    sides[side].append(values[side])
            local[name] = _pair_stats(pairs)
            all_pairs.extend(pairs)
            for side in ("A", "B"):
                centers[side].append(median(sides[side]))
            centers["pooled"].append(median(sides["A"] + sides["B"]))
        drifts = {side:max(v)/min(v) for side,v in centers.items()}
        pooled = _pair_stats(all_pairs) if completed == "post" else None
        qualified = all(v["qualified"] for v in local.values()) and all(v <= 1.02 for v in drifts.values())
        if pooled is not None:
            # Post pooled median ratio itself is not an additional plan gate.
            qualified = qualified and pooled["scaled_log_mad"] <= math.log(1.02) and all(0.98 <= v <= 1.02 for v in pooled["order_medians"].values())
        result[lib] = dict(anchors=local, drift_ratios=drifts, pooled=pooled, qualified=qualified)
    return dict(completed_anchor=completed, libraries=result, qualified=all(r["qualified"] for r in result.values()))


def bootstrap_index(b, s, j):
    C.require(C.integer(b, 0, 9999) and C.integer(s, 0, 1) and C.integer(j, 0, 14), "invalid resampling index")
    limit = 2**64 - (2**64 % 15)
    k = 0
    while True:
        raw = ("xoffset-bb-v1|%d|%d|%d|%d" % (b,s,j,k)).encode("ascii")
        value = int.from_bytes(hashlib.sha256(raw).digest()[:8], "little")
        if value < limit:
            return 15*s + value % 15
        k += 1


def quantile(values, q):
    C.require(values and math.isfinite(q) and 0 <= q <= 1, "invalid quantile")
    values = sorted(values)
    h = (len(values)-1)*q
    lo = math.floor(h)
    return values[lo] + (values[min(lo+1,len(values)-1)]-values[lo])*(h-lo)


def compare(samples):
    rows = [r for r in samples if r["job"]["kind"] == "comparison"]
    C.require(len(rows) == 150, "comparison requires 30 complete blocks")
    logs = {lib:[] for lib in LIBRARIES[1:]}
    for block in range(30):
        members = [r for r in rows if r["job"]["block"] == block]
        C.require(len(members) == 5 and {r["job"]["library"] for r in members} == set(LIBRARIES), "incomplete comparison block")
        values = {r["job"]["library"]: C.decimal(r["total_ns"], positive=True) for r in members}
        for lib in logs:
            logs[lib].append(math.log(values[lib]/values["xoffset"]))
    distributions = {lib:[] for lib in logs}
    for b in range(10000):
        indices = [bootstrap_index(b,s,j) for s in (0,1) for j in range(15)]
        for lib, values in logs.items():
            distributions[lib].append(math.exp(median(values[i] for i in indices)))
    results = {}
    for lib, values in logs.items():
        interval = [quantile(distributions[lib],q) for q in (0.00625,0.99375)]
        lo, hi = interval
        decision = "uncertain"
        if lo > 1.05:
            decision = "competitor_slower_at_relevance_threshold"
        elif hi < 1/1.05:
            decision = "competitor_faster_at_relevance_threshold"
        elif lo > 1/1.05 and hi < 1.05:
            decision = "practical_equivalence_estimate"
        results[lib] = dict(median_paired_ratio=math.exp(median(values)), paired_log_ratios=values,
                            confidence_level=0.9875, percentile_interval=interval,
                            decision=decision, independent_units=30)
    return dict(method="stratified-whole-block-percentile-v1", resamples=10000,
                nominal_family_confidence=0.95, exact_coverage_guaranteed=False, comparisons=results)


def summary(samples):
    jobs = schedule()
    C.require(len(samples) <= len(jobs), "extra sample")
    for index, sample in enumerate(samples):
        C.require(C.same(sample["job"],jobs[index]), "samples are not exact executed schedule prefix")
    gates = []
    for anchor in ANCHORS:
        end = max(i for i,j in enumerate(jobs) if j.get("anchor") == anchor) + 1
        if len(samples) >= end:
            gate = anchor_gates(samples[:end], anchor)
            gates.append(gate)
            C.require(len(samples) <= end or gate["qualified"], "execution continued after failed anchor")
            if not gate["qualified"]:
                break
    qualified = len(samples) == 285 and len(gates) == 3 and all(g["qualified"] for g in gates)
    return dict(schema_version=1, cell=CELL, successful_children=len(samples),
                excluded_warmup_children=sum(s["job"]["kind"].startswith("warmup") for s in samples),
                anchor_gates=gates, measurement_qualified=qualified,
                comparison=compare(samples) if qualified else None,
                old_N3_closed=False, performance_accepted=False)
