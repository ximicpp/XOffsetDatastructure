---
name: sync-typelayout
description: Sync XOffset codebase after TypeLayout submodule update. Use when user says "sync typelayout", "update typelayout", "升级typelayout", "同步typelayout", or mentions TypeLayout upstream changes.
disable-model-invocation: true
argument-hint: "[typelayout-commit-or-branch]"
---

# Sync XOffset after TypeLayout Submodule Update

When the TypeLayout submodule (`external/typelayout`) is updated, XOffset may need corresponding changes. Follow this procedure.

## Phase 1: Analyze upstream diff

1. Record the current TypeLayout submodule commit:
   ```
   cd external/typelayout && git rev-parse HEAD
   ```

2. If `$ARGUMENTS` is provided, fetch and diff against that commit/branch. Otherwise, diff the submodule's staged change (`git diff --submodule=diff external/typelayout`), or ask the user which TypeLayout commit/branch to sync to.

3. Identify **all API-visible changes** in TypeLayout by diffing the old vs new commit. Focus on:
   - `include/boost/typelayout/` — renamed/removed/added headers
   - Public API changes: function renames, signature changes, new/removed type traits
   - Macro renames in `opaque.hpp` (e.g., `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`)
   - `tools/` headers: `sig_export.hpp`, `compat_auto.hpp`, `sig_types.hpp`
   - Namespace changes

4. Summarize the upstream diff to the user as a change list before proceeding.

## Phase 2: Update submodule pointer

```bash
cd external/typelayout
git fetch origin
git checkout <target-commit>
cd ../..
git add external/typelayout
```

## Phase 3: Update XOffset integration points

Check and update **each** of the following files. Only modify what the upstream diff actually affects — do not speculatively change unrelated code.

### 3.1 Main header: `xoffsetdatastructure.hpp`

The header has these TypeLayout integration zones (search to locate):

- **Includes** (lines ~40-43): `#include <boost/typelayout/...>` — add/remove/rename includes to match new TypeLayout header structure
- **Header comments** (lines ~33-39): API summary block starting with `// TypeLayout library —` — update function names, descriptions
- **Domain admission comment block** (lines ~46-60): starting with `// Target Architecture & Domain Admission` — update if admission API changed
- **using declarations** (search `using boost::typelayout::`): update renamed symbols
- **Registration macros** (search `TYPELAYOUT_OPAQUE_`): update if macro names changed in TypeLayout
- **Any direct API calls**: `is_byte_copy_safe_v`, `SafetyLevel`, `get_layout_signature`, `is_transfer_safe`, `detail::classify_signature()`

### 3.2 Tools: `tools/export_signatures.cpp`

- Uses `<boost/typelayout/tools/sig_export.hpp>` and `SigExporter` API
- Check if `add_relocatable<T>()`, `write()`, `write_stdout()`, `platform_name()` are renamed

### 3.3 Tools: `tools/check_compat.cpp`

- Uses `<boost/typelayout/tools/compat_auto.hpp>` and `compat::layout_match`, `compat::definition_match`
- Uses `TYPELAYOUT_CHECK_COMPAT` macro
- Check if any of these are renamed or have signature changes

### 3.4 Signature files: `tools/sigs/*.sig.hpp`

- These are generated files — if the signature format changed in TypeLayout, they need regeneration
- Regeneration happens via Docker build (`bash ./build.sh`) — note for Phase 5

### 3.5 Specs: `openspec/specs/`

Search for old API names in these spec files and update:
- `openspec/specs/type-safety-delegation/spec.md` — references TypeLayout predicates
- `openspec/specs/type-signature/spec.md` — references signature API
- `openspec/specs/relocatable-opaque/spec.md` — references opaque macros
- `openspec/specs/opaque-relocatable-types/spec.md`

### 3.6 Docs: `docs/`

Search for old API names in docs and update:
- `docs/technical_overview.md`
- `docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md`
- `docs/MIGRATION_TYPELAYOUT.md`
- Other docs that reference changed API names (use grep to find)

### 3.7 CLAUDE.md

If any API names listed in CLAUDE.md are changed, update them.

## Phase 4: Verify

Present all changes to the user for review. Summarize:
- Which files were modified and why
- Which API names were updated (old → new)
- Any deprecation aliases that TypeLayout added (XOffset can rely on these for backward compat)

## Phase 5: Build & Test

Ask the user if they want to run the Docker build to verify:
```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
  ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```

On Apple Silicon, remind to add `--platform linux/amd64`.

Expected: 23/23 tests pass, signature export succeeds, compat check succeeds.

## Phase 6: OpenSpec change record (optional)

If the changes are non-trivial (API renames, behavior changes), ask the user whether to create an openspec change record:

```
openspec/changes/<date>-<change-name>/
├── proposal.md   — Why the change, what changed upstream, XOffset impact
└── tasks.md      — Checklist of all modifications made
```

Use the existing archived changes as reference for format (e.g., `openspec/changes/archive/2026-03-16-rename-transfer-safe-to-portable/`).

## Key Principles

- **XOffset delegates ALL type safety to TypeLayout** — never duplicate or re-implement TypeLayout logic
- **Minimal changes** — only update what the upstream diff actually affects
- **Grep before editing** — always search for old API names across the entire repo to catch all references
- **Preserve deprecated aliases** — if TypeLayout added deprecated aliases, XOffset should use the NEW names but note the aliases exist
