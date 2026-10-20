# M2 actual assembly mechanism review

Verdict: **pass for the static mechanism opportunity**. On the valid nonempty
`XFixedVector<sc::xo_detail::Attr>` capacity-containment-miss path, the emitted
parent-vector code performs one data/canonical recovery where the frozen control
performs four. This is neither a measured speedup nor dynamic-hit evidence.
No compiler, C++ native executable or timing process was executed by this reviewer.

## Exact artifact binding and one tooling limitation

| Artifact | SHA256 |
|---|---|
| Actual candidate assembly | `5551c11a4e7b4ae379e61bb499047795021f2bf9f3f7e12dc7e6b0a33b69da0f` |
| Frozen control assembly | `110bc1bc494a1d731c563660d5760a630d5bb0da39531eb6e43e83bd04479640` |
| Actual codegen request146703B | `16cf5688e44d752602509547095ed2401f277b9e84a3baf709aba8cd5590851b` |
| Compile admission212881B | `37716e47a9058e047b2c4bfbc0d7e3bac51aa5d765289f2b8dbe97987904ab27` |
| Compile worker receipt53040B | `f9a57799f89da9b70de044176fce1f7dd81e3e9da452d5315b8e534948125d0b` |
| Candidate identity154 | `4a62016c53b103964d4a0b14ce56e3b717b59b54b011efbd52c021b7114b944c` |
| Actual main278527B | `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f` |

Candidate assembly is
`/root/xoffset-c14-iteration/20261020/v2/validation/m2-codegen-v1/candidate-driver.s`.
Control is the retained
`/root/xoffset-c14-iteration/20261019/v1/validation/m1-v2/codegen/control-driver.s`.
No control was recompiled. The actual request/receipt identifies one candidate
bare `-O3 -DNDEBUG -S` compile, not the correctness `-UNDEBUG` configuration.
Original queue run `bdee66f95ee7431da5c43a628164868e` completed with return0,
`command_exit` and empty live/process-group/zombie cleanup.

All154 current staged source file IDs equal the admission map and the compile
worker's before/after maps. Canonical sorted compact JSON SHA256 of that complete
file-ID map is
`c0ba88fd821f9e96f4ced4b3b4fb4acb9235988d35527bcbc7be7e15870a968e`.
The production14 map digest is
`2810fca77c737987a0d48af518a214c04dba57468744d171824c6496afd5f128`.
The JSON review includes every production ID, actual compiler/wrapper/libc++
identities, full request/argv and exact receipt references.

The actual `/opt/p2996-toolchain/bin/clang++` identity is
`c7fa55437d34c43201da37960251d7d3dab1e06fc92273efe6f6d88f6dae7c6e`;
the1462-byte profile `bin/clang++` wrapper identity is
`b970d582c872afbdc6095c56ad235c5f8cf65245ac53b0a51194d6b46fd27ee8`.
These and the recorded libc++/libc++abi IDs still match the actual admission.

The outer61-byte profile `bin/clang++-libcxx` was **not explicitly captured in
M2's before/after input map**. Its current SHA is
`c8ea06a7c58557e3b89d09873dc88457d7b0c5d9af3f08cf0449093f2fbd1b87`,
matching the sealed exact entry in v1 `R1_CODEGEN_JOB_ADMISSION.json` (also the
earlier M1 admission). Its current content only forwards to the neighboring
`clang++` with `-stdlib=libc++`. The JSON review pins that historical receipt and
current wrapper file. This does not establish a missing earlier observation;
**complete tool before/after closure is not claimed**. It does not invalidate
direct inspection of the actual emitted4→1 code. Do not repeat a restricted
compile/native batch merely to fill this metadata gap. Future requests must
explicitly capture the61-byte wrapper before and after execution.

## Actual function and control-flow path

Reviewed symbol in both artifacts:

```text
_ZN20XOffsetDatastructure12fixed_detail28move_assignment_owns_addressINS_12XFixedVectorIN2sc9xo_detail4AttrEEEEEbRKT_PKv
```

| Evidence | Control lines | Candidate lines |
|---|---:|---:|
| Complete helper, including epilogue |50724–50871|50724–50833|
| Initial data/canonical recovery |50751–50761|50750–50762|
| Capacity containment and early true |50762–50775|50763–50774|
| Extra begin/end recoveries after miss |50777–50804|absent|
| Live-size bound after miss |50814–50818|50776–50781|
| Child recursion and early true |50825–50844|50786–50806|

The control's nonzero descriptor takes the canonical call at50761. If the
capacity containment test misses, its begin/end setup reaches50785,50795,50804
while the descriptor remains nonzero. Those four sites belong to one parent's
setup, before traversing the first Attr; they are not four lookups per element.

The candidate resolves the same anchor+4 and width32 once at50760, selecting
the canonical result when nonnull and retaining the original fallback otherwise
(50756–50762). It keeps that pointer in `%r14`. The unchanged unsigned capacity
test uses capacity at offset12, multiplied by32; a hit returns true at50774
without reading live size or recursing. On a miss `.LBB229_4` reads live size
from offset8 at50776. No second parent canonical call occurs before or inside
the parent traversal loop.

## Null, empty and live-element iteration

A zero descriptor branches at50752 to the false return50808–50809 before
relative-target addition, live-size reading, end arithmetic or child access.
For nonzero allocated storage, capacity containment still precedes the size
check. After its miss, size0 exits at50778; no child is accessed.

For positive size `n`,50780–50781 initialize a byte counter to `32*n-32`.
The loop visits the current `%r14` Attr at50791 and invokes the same reflected
child traversal at50796. A positive result returns immediately at50801.
Otherwise50803 advances the pointer by32,50804 tests the old counter,50805
decrements it with flag-preserving `lea`, and50806 branches using that earlier
test. Thus n=1 executes once; general n executes exactly n children on complete
misses, in ascending address order, and reaches only the existing one-past
pointer after the final child. It neither substitutes capacity for size nor
dereferences a one-past element. Malformed descriptors gain no new validation
guarantee; this argument is for the original live-container invariants.

The source's null-safe `elements ? elements + count : nullptr` has become a
null/zero guard plus counted iteration in optimized assembly. Absence of a
literal end-pointer calculation is not a missing size bound.

## Preserved recursion, lookup implementation and assignment sites

The actual Attr reflected visitor body is byte-for-byte identical as assembly
text: control50902–51007, candidate50864–50969. Its key lookup remains before
the value lookup, with positive-result short circuit. The candidate still has
the child key/value canonical calls at50899 and50938. Those calls are distinct
from the parent's4→1 reduction and are not counted as eliminated.

`XBufferCore::canonical_bytes_at` at19278–19859 is identical in both artifacts.
No new address-recovery shortcut or changed cold implementation is hidden in a
callee. Both public move-assignment owns checks remain at50640 and50646, with
swapped arguments and the original short circuit at50641–50648. In fact, the
assembly prefix before the reviewed helper and suffix after it are exactly
equal across the two complete artifacts. All observed emitted-code changes
are confined to this helper body.

## Costs and limits

Both versions save six registers and allocate the same56-byte local stack frame.
The candidate keeps the target in `%rbx` instead of a stack spill/reload. It
adds a result-address `lea` in each child-loop setup and uses `test` plus
counter-decrement `lea` where the original had an end-pointer comparison. These
instruction/register changes remain real costs for later measurement; no
cycle ratio is inferred. No allocation call, added recursive edge or enlarged
frame appears in the reviewed helper. Canonical/child bodies are unchanged.

This establishes the static reduction of three parent recoveries on the stated
eligible path. It does not establish how often that path occurs in fixed M,
prove dynamic canonical/query truth, finish the candidate's full business/V1/
consumer gates, authorize screening, or prove a timing improvement. A separately
admitted finite mechanism request remains the next step. Assembly byte length
and file size were used only for identity, never as performance evidence.
