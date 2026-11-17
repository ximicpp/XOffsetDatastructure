# Archived evidence

Historical records were removed from the working tree to keep it small. Nothing was rewritten: each
batch below last existed, unchanged, at the named commit or tag. To restore files into the working tree
without switching branches, run `git checkout <commit> -- <paths>`; to only read one, run
`git show <commit>:<path>`.

| Removed | Last present at | Paths |
| --- | --- | --- |
| 2026-11-17: superseded docs and run records | `30ca3500651c19d144abc89f06a1cbbb543bc17a` (main) | `docs/STABLE_VM_STORAGE_DESIGN.md`, `docs/STABLE_VM_STORAGE_MIGRATION.md`, `docs/STABLE_VM_STORAGE_VALIDATION.md`, `docs/perf/`, `docs/validation/vm-observability/`, `tools/schema_guard/VALIDATION_*_20261028.md`, `bench/cppcon2024/results/2026-11-16/`, `bench/cppcon2024/results/2026-11-16-pr292/`, `bench/cppcon2024/results/2026-11-17-fair/`; per-release performance tables of 3.0.0–3.6.0 and the long CHANGELOG are in the history of `docs/PERFORMANCE.md` and `CHANGELOG.md` |
| 2026-11-17: Chinese-language documents (the repository is English only) | `837c9d4febb5e50c1cc0df4257ae224608ec7f33` (main) | `README.zh-CN.md`, `docs/validation/vm-final/`, `docs/handoff-history/`, `docs/perf/round2-20261021/`, `docs/perf/round3-20261021/`, `docs/perf/vm-owner-scaling/`, `docs/perf/gcc16-20261026/` |
| 2026-11-17: LLM/agent files and openspec | `18a37f39` (main) | `CLAUDE.md`, `AGENTS.md`, `.claude/`, `openspec/changes/` |
| 2026-10-27: V1/VM-era documents, experiments and root handoff files | `87a0514b6fd76bdd130d1be1067232211ce78b24` (`next_cpp26`) | `docs/VM_*.md` (except `VM_PACKAGING.md` and `VM_VALIDATION_RUNNER.md`, renamed to `PACKAGING.md` and `VALIDATION_RUNNER.md`), `docs/V1_*`, `docs/BOOST_*`, `experiments/`, `HANDOFF.md`, `HANDOFF_GITHUB_20261019.md`, `HANDOFF_MAC_20261020.md`, `PROMPT_MAC_20261020.md` |
| W9a switch: handoff and C14 iteration evidence (about 254 MB) | Tag `archive/codex-mac-continuation-20261020` (also `archive/claude-mac-candidates-20261020`) | `handoff/`, `docs/coordination/`, `tools/c14_handoff_20261020/`, `tools/c14_iteration_20261019/`, `tools/c14_iteration_20261020_v2/`, `accepted-executed/`, `accepted-generated/` |

For the tag, fetch it first (`git fetch origin --tags`). These scripts in `tools/` read the handoff
paths of the last row and fail until they are restored: `verify_mac_handoff_20261020.py`,
`verify_c14_evidence_bundle_20261020.py`, `restore_handoff_20261019.py`, `verify_vm_handoff.py`.

Kept in the tree because tests read them: `docs/validation/2026-10-15-vm-followup/` and
`docs/validation/2026-10-15-vm-stage2/`. Files under `docs/validation/` are frozen records and may still
name removed paths.
