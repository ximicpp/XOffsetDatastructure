# Archived handoff and C14 iteration evidence

After the default implementation switched to W9a (commit `14b57637d6fa57f8521138a0997fb3c8736db4f1`,
merged into `next_cpp26` = `9e3602f977e984b5d4d87bee68c2f369efbf09e8` by PR #2), the following handoff
and C14 iteration evidence was removed from the `next_cpp26` working tree. The content was not modified and
history was not deleted; it can still be retrieved unchanged from Git tags.

## Removed content

| Path | Files | Bytes |
| --- | ---: | ---: |
| `handoff/` | 26 | 176,729,761 |
| `docs/coordination/` | 1,131 | 72,826,504 |
| `tools/c14_handoff_20261020/` | 2 | 13,769 |
| `tools/c14_iteration_20261019/` | 104 | 643,015 |
| `tools/c14_iteration_20261020_v2/` | 379 | 3,243,759 |
| `accepted-executed/` | 4 | 119,619 |
| `accepted-generated/` | 5 | 282,840 |
| **Total** | **1,651** | **253,859,267 (about 254 MB)** |

Left in place at the time: `docs/validation/` (`tools/tests/test_validate_vm.py` reads
`2026-10-15-vm-followup` there), `experiments/` (also moved out on 2026-10-27, see below), `docs/perf/`,
`docs/handoff-history/`, `openspec/`, and `tools/*.py` (including the `verify_*handoff*` scripts).

## Where it is stored

- **`archive/codex-mac-continuation-20261020`** (points to `9e3602f`): contains all 7 paths in the table;
  recommended.
- `archive/claude-mac-candidates-20261020`: also contains all 7 paths.
- `archive/codex-cross-machine-handoff-20261019`, `archive/codex-vm-handoff-20261016`: contain only the
  `handoff/`, `docs/coordination/`, `accepted-executed/` and `accepted-generated/` that existed then (in
  their versions of that time).

## Restoring

Run at the repository root (restores files into the working tree only, without switching branches):

```sh
git fetch origin --tags
git checkout archive/codex-mac-continuation-20261020 -- \
  handoff docs/coordination \
  tools/c14_handoff_20261020 tools/c14_iteration_20261019 tools/c14_iteration_20261020_v2 \
  accepted-executed accepted-generated
```

To only look at it, check it out separately with
`git worktree add <new dir> archive/codex-mac-continuation-20261020`, leaving the current branch untouched.

## Tools that depend on these paths

The following scripts remain in `tools/`, but the matching paths must be restored as above before running
them, otherwise they fail to find their inputs:

- `tools/verify_mac_handoff_20261020.py` (`handoff/mac-20261020/TRANSFER_MANIFEST.json`,
  `handoff/github-20261019/C14_MATERIALIZATION.json`)
- `tools/verify_c14_evidence_bundle_20261020.py` (`handoff/mac-20261020/evidence/BUNDLE.json`)
- `tools/restore_handoff_20261019.py` (`handoff/XOffset-continuation-20261019-v1.zip` and others)
- `tools/verify_vm_handoff.py` (`handoff/manifest.json`)

Links to these paths in the `HANDOFF*.md` / `PROMPT_MAC_20261020.md` files likewise only open after
restoring.

## 2026-10-27: V1/VM-era documents, experiments/ and root-level handoff files

To clean up the trunk for next_cpp26's first formal release (roadmap W5), the following was removed from
the `next_cpp26` working tree. The content was not modified; it last existed in full in commit
`87a0514b6fd76bdd130d1be1067232211ce78b24` (the merge commit of #120 on `next_cpp26`), which is on the
first-parent history of `next_cpp26`, so it can be retrieved without a tag.

| Path | Files | Bytes |
| --- | ---: | ---: |
| `docs/VM_*.md` (except the two renamed below), `docs/V1_*`, `docs/BOOST_*` | 32 | 347,471 |
| `experiments/` (vm_coordination, vm_io_candidates, vm_network, vm_network_measurement, vm_observability, vm_owner_measurement, vm_owner_scaling, vm_platform_validation, vm_windows) | 126 | 1,990,030 |
| `HANDOFF.md`, `HANDOFF_GITHUB_20261019.md`, `HANDOFF_MAC_20261020.md`, `PROMPT_MAC_20261020.md` | 4 | 40,863 |
| **Total** | **162** | **2,378,364** |

The two packaging documents still in use were only renamed, not moved out:
`docs/VM_PACKAGING.md` → `docs/PACKAGING.md`, `docs/VM_VALIDATION_RUNNER.md` →
`docs/VALIDATION_RUNNER.md` (the DOC_FILES of `tools/prepare_release.py` were renamed to match).

Retrieving (restores files into the working tree only, without switching branches):

```sh
git fetch origin next_cpp26
git checkout 87a0514b6fd76bdd130d1be1067232211ce78b24 -- experiments HANDOFF.md HANDOFF_GITHUB_20261019.md HANDOFF_MAC_20261020.md PROMPT_MAC_20261020.md
git checkout 87a0514b6fd76bdd130d1be1067232211ce78b24 -- $(git ls-tree --name-only 87a0514b6fd76bdd130d1be1067232211ce78b24 docs/ | grep -E '^docs/(VM_|V1_|BOOST_)' | grep -vE '^docs/VM_(PACKAGING|VALIDATION_RUNNER)\.md$')
```

Only historical material still points at these paths: designs and tasks under `openspec/changes/`, and
old records under `docs/validation/` and `docs/perf/`, plus a few references marked archived in
`docs/STABLE_VM_STORAGE_*.md`. These files are not changed; retrieve with the commands above when reading
them.

## 2026-11-17: LLM/agent files and openspec removed

`CLAUDE.md`, `AGENTS.md`, `.claude/` (the sync-typelayout skill) and `openspec/changes/` (three archived
changes plus `vm-next-stage-hardening`) were deleted from the repository and added to `.gitignore`. The
build, test and code conventions useful to people moved to [CONTRIBUTING.md](../CONTRIBUTING.md). The
lists and frozen copies under `docs/validation/` still mention these paths and are not changed; to read the
originals, retrieve them from a commit before the merge, for example:

```bash
git show 18a37f39:openspec/changes/vm-next-stage-hardening/tasks.md
```

## 2026-11-17: Chinese-language documents removed (repository is English only)

The repository's documentation and code are English only. Current documents were translated into English;
the Chinese README and large frozen historical evidence written in Chinese were removed instead of
translated. The content was not modified; it last existed in full in commit
`837c9d4febb5e50c1cc0df4257ae224608ec7f33` (on the first-parent history of `main`).

| Path | Contents |
| --- | --- |
| `README.zh-CN.md` | Chinese README (the English `README.md` is the only README) |
| `docs/validation/vm-final/` | Frozen VM-era final validation capsule (260 files) |
| `docs/handoff-history/` | Historical handoff notes |
| `docs/perf/round2-20261021/`, `docs/perf/round3-20261021/` | Performance round notes of 2026-10-21 |
| `docs/perf/vm-owner-scaling/` | VM owner scaling measurements |
| `docs/perf/gcc16-20261026/` | GCC 16 run notes of 2026-10-26 |

Retrieving (restores files into the working tree only, without switching branches):

```sh
git checkout 837c9d4febb5e50c1cc0df4257ae224608ec7f33 -- docs/validation/vm-final docs/handoff-history \
  docs/perf/round2-20261021 docs/perf/round3-20261021 docs/perf/vm-owner-scaling docs/perf/gcc16-20261026
```
