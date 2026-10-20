# macOS-native business functional suite (normal / SAN, 80 children each)

Scope: run the restore-20261019 `serialization_comparison` functional campaign
(5 libraries x (describe + S/M/L x {encode, inspect-input, validate, run,
inspect-final}) = 80 native children, 15 completed cells, then bundle audit)
natively on macOS arm64 with the P2996 clang `d77eff1`, for two xoffset
sources: **C14** and **R1+M2**. Nothing in this directory has been executed
yet except static checks (py_compile, patch-anchor load via `--mac-patches`).

## Files

| path | role |
|---|---|
| `meta-c14/`, `meta-r1m2/` | copies of restore `experiments/serialization_comparison` + `tools/serialization_comparison`; only the xoffset label literal changed |
| `meta.patch` | every change vs restore (4 hunks per copy) |
| `identities/{c14,r1m2}-source-identity.json` | expected 14 production files + `source_content_sha256` |
| `mac_campaign.py` | importlib loader + documented monkeypatches (M1..M5, additive A1/A2) |
| `runner/experiments/vm_coordination/run_job.py` | native stand-in for the absent WSL queue supervisor (same request/lock/result/manifest protocol) |
| `build_competitors.sh` | flatc, capnp/capnpc-c++ + libcapnp/libkj (normal and ASan) -> `../install/` |
| `build_bench.sh` | sc_* drivers + contract/resource/capability tests -> `../bench/<id>-<mode>/` |
| `make_profiles.py` | `read_profile`-schema profiles for macOS |

Identities (verified from files):

| id | label (adapter `library_id`, `IDENTITIES["xoffset"][0]`) | source_content_sha256 | main header |
|---|---|---|---|
| c14 | `xoffset-receive-completion-v1` | `26299f796e2ba289ab15b38b000a785ad1877fbc65ce29cf3071d2118804c3b4` | `66818de4…` |
| r1m2 | `xoffset-c14-r1-m2-combined-v1` | `8cd90345f109308d150caa141e49fa65e2e7b1fac957957e8e45bc8ee8eb5518` | `155f0847…` |

The C14 snapshot matches `candidate-source-identity-v2.json` (content
`26299f79…`), **not** v1 (`3630d03f…`, main header `a1dfec7b…`). Commit label
stays `7539433ab96d69c4a07b1db5c0a13786da85cdcd` (legacy base metadata).
The digest algorithm is the one in `combined_full/business/prepare.py`:
`sha256(json.dumps({name:{bytes:int,sha256}}, sort_keys=True, separators=(",",":")))`.

## 0. Environment

```bash
B=/Volumes/XOffsetCS/mac-validation/business
V=/Volumes/XOffsetCS/mac-validation
# campaign.active_queue compares argv[0] with sys.executable exactly, so use the
# path the interpreter reports for itself (Homebrew rewrites it to opt/...).
PY=$(/opt/homebrew/bin/python3.13 -I -c 'import sys;print(sys.executable)')
test "$($PY -I -c 'import sys;print(sys.executable)')" = "$PY"   # fixed point
cd "$B"          # runner records cwd; the campaign child must see the same cwd
```

Python >= 3.10 is mandatory (`reference.py` uses `bisect_left(key=)`);
`/usr/bin/python3` (3.9.6) is rejected by `mac_campaign.py`. Never open the
bundle/meta directories in Finder (`.DS_Store` breaks inventories/tree ids).

## 1. Competitor tools/runtimes (once)

```bash
JOBS=8 $B/build_competitors.sh all      # flatbuffers, capnproto, capnproto-asan, verify
```
Expect `$V/install/{flatbuffers,capnproto,capnproto-asan}` and `$V/install/IDENTITIES.sha256`.

## 2. Bench binaries (4 fresh build dirs)

```bash
JOBS=8 $B/build_bench.sh c14  normal --ctest
JOBS=8 $B/build_bench.sh c14  san    --ctest
JOBS=8 $B/build_bench.sh r1m2 normal --ctest
JOBS=8 $B/build_bench.sh r1m2 san    --ctest
```
Each produces `$V/bench/<id>-<mode>/bin/sc_{xoffset,cista,flatbuffers,capnproto,cereal}`
plus `sc_*_contract`, `sc_*_resource`, capability tests, and logs in
`$V/bench/<id>-<mode>-logs/` (configure.argv, ninja-deps, xoffset-header-deps,
otool-L, binaries.sha256, ctest.xml). ctest is informational (not part of the 80).

## 3. Prepared fixtures (once, shared by both ids; pure Python)

```bash
SC_MAC_META=c14 $PY -B $B/mac_campaign.py prepare \
  --fixture-module $B/meta-c14/experiments/serialization_comparison/model/reference.py \
  --out $V/prepared-v1
# optional determinism cross-check against the shipped golden S fixture
for f in input.canonical.bin trace.bin final.canonical.bin query-before.bin query-after.bin; do
  cmp $V/prepared-v1/S/$f $B/meta-c14/experiments/serialization_comparison/fixtures/golden-S/$f; done
```
Expect `{"status":"audit-passed","kind":"sc-prepared",...}`. (reference.py is
byte-identical in both meta copies, so one prepared bundle serves both.)

## 4. Profiles

```bash
for id in c14 r1m2; do for mode in normal san; do
  $PY -I -B $B/make_profiles.py --meta $id --mode $mode --out $V/profiles/$id/profile-$mode.json
done; done
```
Profiles: `platform_class=macos-arm64-native`, `prefix=[]`,
`lock_dir=$B/runner/build/vm-coordination`, env `LANG/LC_ALL=C TZ=UTC PATH=/usr/bin:/bin`;
SAN adds `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=0:allocator_may_return_null=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `MallocNanoZone=0`.
A sidecar `profile-<mode>.json.otool.json` lists rpaths/dylibs (system dylibs unhashed).

## 5. Functional (4 runs, strictly one at a time; the runner holds a flock lease)

```bash
run_functional() {  # $1=id $2=mode
  local out=$V/runs/$1-functional-$2
  SC_MAC_META=$1 $PY -B $B/runner/experiments/vm_coordination/run_job.py run \
    --owner W3 --kind functional --out $out/queue --timeout-seconds 3600 --wait-seconds 0 -- \
    $PY -B $B/mac_campaign.py functional \
      --prepared $V/prepared-v1 --profile $V/profiles/$1/profile-$2.json \
      --out $out --queue $out/queue
}
run_functional c14 normal
run_functional c14 san
run_functional r1m2 normal
run_functional r1m2 san
```
All paths must be absolute and symlink-free (do not use /tmp or /var). Child
stdout (campaign JSON) is in `$out/queue/stdout.log`; expected
`{"performance_accepted": false, "status": "functional-native-completed-awaiting-queue"}`.
`$out.mac-identity.json` (outside the bundle) records the live content digest.

## 6. Pin + audit (pure, no native child)

```bash
audit_run() {  # $1=id $2=mode
  local out=$V/runs/$1-functional-$2
  local pin=$(SC_MAC_META=$1 $PY -B $B/mac_campaign.py pin --bundle $out | $PY -I -c 'import json,sys;print(json.load(sys.stdin)["pin"])')
  echo "$pin" > $out.postrun-pin.txt
  SC_MAC_META=$1 $PY -B $B/mac_campaign.py audit --bundle $out --postrun-pin $pin
  $PY -I -c 'import json,sys;s=json.load(open(sys.argv[1]+"/status.json"));print(s["status"],len(s["attempted"]),len(s["completed"]),len(s["unexecuted"]),s["error"])' $out
}
for id in c14 r1m2; do for mode in normal san; do audit_run $id $mode; done; done
```

Expected per run:
* audit: `{"status":"audit-passed","kind":"sc-functional","performance_accepted":false,"final_manifest_externally_pinned":true,...}`
* status line: `complete 80 15 0 None`
* `queue/result.json`: `status=complete`, `command_returncode=0`, cleanup all empty
* `runtime-before.json == runtime-after.json`; xoffset describe `library` equals the id's label
* mac A2 check: captured `candidate-file/*` records reproduce the expected content digest

## Known macOS deviations (all intentional, documented)

1. **Queue/lease**: the historical global WSL `run_job.py` lease is replaced by a
   single-host native supervisor with the same on-disk protocol; it is not the
   original runner (its bytes are bound as `protocol/run_job.py`).
2. **Platform gate / class**: `Darwin+arm64`, `macos-arm64-native` (M4/M5).
3. **No CPU pinning** (`taskset -c 2` historically); **no loader prefix** (dyld + LC_RPATH to
   the P2996 libc++); closure via `otool` instead of `ldd/readelf`; dyld-shared-cache system
   libraries cannot be hashed.
4. **ru_maxrss is bytes on Darwin**: `peak_rss_*_kib` fields hold bytes. The campaign only
   checks they are decimals and monotonic, so it passes, but values are not KiB and are not
   comparable to Linux evidence.
5. **No LeakSanitizer** on Darwin: `detect_leaks=0`; **no leak claim** is made.
   Darwin ASan interposes `operator new` for libc++-dylib callers; this does not affect the
   80 children but can make the SAN `sc_*_resource` ctest (custom `operator new`) misbehave.
6. `MallocNanoZone=0` in SAN env to keep libmalloc's ASan warning off stderr (campaign
   requires empty child stderr).
7. Cap'n Proto built with `WITH_OPENSSL/ZLIB/FIBERS=OFF` (historical values unknown;
   the adapter uses only message builder/reader APIs).
8. Interpreter is Homebrew Python 3.13 (historical: `/usr/bin/python3` on Ubuntu).

## What this does NOT reproduce

* **Not the historical final-v4 metadata.** The 20261020-v2 business runs used
  `/root/xoffset-receive-completion/20261019/v1/final-v4/source` metadata, which differs from
  restore in 9 files: xoffset adapter (9619 B vs 9418 B; final-v4 adds
  `source_content_sha256` to `describe` and a different trust_mode
  `...;stable-owner-V8-plus-explicit-production-paths-four-v1;...`), cista adapter, driver.cpp,
  contract-v1.json, native-result.schema.json, campaign.py (54171 B, adds candidate closure /
  content checks), protocol.py (11167 B, adds `CANDIDATE_CONTENT_SHA256`,
  `CANDIDATE_CLOSURE_IDS`), README, tests/test_core.py. Only final-v4 campaign.py/protocol.py
  exist on this Mac (`tools/c14_iteration_20261020_v2/final_formal_v1/native_helpers/campaign.py`,
  `tools/c14_iteration_20261019/validation/screen/protocol.py`); the final-v4 C++ side does not.
  This suite therefore runs the **restore (alpha1-era) adapters/driver** against the new
  headers; content identity is enforced by mac_campaign A1/A2 instead of describe.
* No reuse of sealed V5/V8 competitor binaries or profiles: all five drivers are rebuilt.
* Historical `prepared-v1` bytes are not available for comparison (only golden-S/0/1/17/257).
* No timing/freeze/run (285-child performance cell), no P4/P6, no compatibility worker
  (V1 84 + 8 extras, AB4, C4), no full73 ctest, no package checks.
* No x86_64/WSL2 or Linux-sanitizer equivalence claim; results are a macOS arm64 functional
  correctness signal only (`performance_accepted=false` throughout).
