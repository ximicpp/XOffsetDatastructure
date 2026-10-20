# M2 gate review map contract

This request describes source-only binding of existing M2 facts. The binder has not been executed. It does not dispatch, compile, run native/checker, freeze, admit a screen or modify an existing receipt.

Tool: `tools/c14_iteration_20261020_v2/validation/screen/bind_gates_m2.py`.

The exact candidate identity ref, source154 and production14 maps come from existing `core/M2_SCREEN_GATE_BINDING.json` SHA `1d24709eaaa8384e6ee672575e1758dbf7f65382824b64d3547302239acc068c`. Actual functional closure is `validation/m2_full/M2_FUNCTIONAL_GATES.json` SHA `6af220ba48246bacf65e58b5c776aaa59be691a4fa37bda7cf55261d7b77cceb`, including actual full source154 receipts. No R1 extra binding, cold-body equality or historical53-field template is required or imported.

## Prepare: eight reviewed actual gates, three pending

Root supplies a new JSON map with this exact shape:

```json
{
  "kind": "M2-screen-independent-gate-review-map",
  "candidate": "m2-exact-owns-v1",
  "candidate_identity": {"path": "EXACT_CORE_IDENTITY_PATH", "identity": {"bytes": 44233, "sha256": "4a62016c53b103964d4a0b14ce56e3b717b59b54b011efbd52c021b7114b944c"}},
  "source154": "COPY_THE_ACTUAL_154_MEMBER_OBJECT_FROM_CORE_BINDING",
  "production14": "COPY_THE_ACTUAL_14_MEMBER_OBJECT_FROM_CORE_BINDING",
  "gates": {
    "mechanism": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_MECHANISM_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "targeted": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "full_normal": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "full_san": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "business_SML": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "v1_bidirectional": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "consumers": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}},
    "lifecycle": {"independent_review": {"status": "passed", "record": {"path": "ACTUAL_M2_FULL_ACCEPTANCE_WSL_PATH", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}}
  }
}
```

Placeholders are documentation, not a runnable map. Both member fields must be actual JSON objects, not strings. Each record requires its actual byte count and SHA. The seven functional envelopes must reference root's actual `reviews/M2_CAPTURE_FUNCTIONAL_ACCEPTANCE.json` SHA `dc43c3e27c650d241d1dbb4ef002635fe99280277a9866431eb07d2d9eec5dca`; the mechanism envelope references `reviews/M2_MECHANISM_EXECUTION_ACCEPTANCE.json` SHA `4d8deaf53cfbaf22c244a7ec3f1d738b1191e5b34dfa96f81f46166bc4866e70`. The binder then loads each bridge's pinned `independent_underlying_review`, respectively the original `M2_FULL_ACCEPTANCE.json` and `M2_MECHANISM_ACCEPTANCE.json`, and checks actual passed status, candidate/source closure, counts and mechanism evidence pins. Root's map status cannot replace that underlying independent review. No guessed alternate schema or missing-evidence fallback is used.

With a real map, the source-only command is:

```text
/usr/bin/python3 -B /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261020_v2/validation/screen/bind_gates_m2.py prepare --review-map ABSOLUTE_EIGHT_GATE_MAP.json
```

It exclusively creates `validation/screen/M2/gate-receipts-v1/{gate}.json`, `GATES.json`, and `PARENT_GATE_REVIEW_REQUEST.json`. Every passed gate has precisely `independent_review: {status: 'passed', record: {path, identity}}`. The three pending gates have `independent_review: {status: 'pending', record: null}` and never count as passed. Missing evidence fails; a partial publication is retained and cannot silently be overwritten.

## Complete: three independent typed decisions

After parent actually reviews codegen, safety and engineering, root supplies a separate map with the same kind/candidate/identity/source154/production14 fields and exactly three gates:

```json
{
  "codegen": {
    "assertions": {"candidate_compiles": 1},
    "typed_gate": {"path": "ACTUAL_ROOT_CODEGEN_TYPED_GATE", "identity": {"bytes": 0, "sha256": "ACTUAL"}},
    "independent_review": {"status": "passed", "record": {"path": "ACTUAL_PARENT_TYPED_REVIEW", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}
  },
  "safety": {
    "assertions": {"representation_and_lifetime_preserved": true},
    "typed_gate": {"path": "ACTUAL_ROOT_SAFETY_TYPED_GATE", "identity": {"bytes": 0, "sha256": "ACTUAL"}},
    "independent_review": {"status": "passed", "record": {"path": "ACTUAL_PARENT_TYPED_REVIEW", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}
  },
  "engineering": {
    "assertions": {"predeclared_cost_policy_satisfied": true},
    "typed_gate": {"path": "ACTUAL_ROOT_ENGINEERING_TYPED_GATE", "identity": {"bytes": 0, "sha256": "ACTUAL"}},
    "independent_review": {"status": "passed", "record": {"path": "ACTUAL_PARENT_TYPED_REVIEW", "identity": {"bytes": 0, "sha256": "ACTUAL"}}}
  }
}
```

This example is the `gates` object only. `typed_gate` references the actual root `reviews/M2_SCREEN_GATES_v1/{codegen,safety,engineering}.json` (engineering remains pending its actual review). Its kind, gate, passed status, exact candidate identity/source154/production14 and assertions must match. The entry's `independent_review` must equal that actual typed receipt's existing envelope, and its underlying review file must exist, hash correctly, have `status` equal `pass` or `passed` and bind the exact candidate identity. Typed receipt evidence refs are checked too. This consumes root's actual typed records without asking the original independent reviews to acquire a new schema or repeating review. One combined parent record may cover multiple gates; the map cannot review itself. Existing core partial proof is evidence, not an accepted independent engineering or safety decision. Bare profile/cost/resource facts belong to parent engineering review; diagnostic timing does not imply speed.

```text
/usr/bin/python3 -B /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261020_v2/validation/screen/bind_gates_m2.py complete --review-map ABSOLUTE_THREE_GATE_MAP.json
```

This rechecks M2 facts, keeps the original eight receipts, creates only three new `gate-receipts-v2` receipts and a new `GATES_v2.json`. All eleven are then review-bound, while `screen_admitted` and `performance_admitted` remain false. No shared/R1/v3 frozen file is changed. Root remains responsible for any later original-lease source-only invocation and separate screen admission.
