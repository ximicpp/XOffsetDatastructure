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

2. If `$ARGUMENTS` is provided, fetch and diff against that commit/branch. Otherwise, fetch origin first (`git fetch origin`), then compare HEAD against `origin/main`. If there are no new commits, ask the user which TypeLayout commit/branch to sync to.

3. Identify **all API-visible changes** in TypeLayout by diffing the old vs new commit. Focus on:
   - `include/boost/typelayout/` — renamed/removed/added headers
   - Public API changes: function renames, signature changes, new/removed type traits
   - Macro renames in `opaque.hpp` (e.g., `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`)
   - `tools/` headers: `sig_export.hpp`, `compat_check.hpp`, `sig_types.hpp`, `safety_level.hpp`
   - Namespace changes (e.g., `detail::` → `compat::`)
   - Signature format changes (e.g., type tag renames like `fld` → `fld64/fld80/fld128`)
   - Struct field additions (e.g., `TypeEntry`, `ExportEntry` gaining new fields)

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

- **Includes** (lines ~38-39): `#include <boost/typelayout.hpp>` and `#include <boost/typelayout/tools/sig_types.hpp>` — add/remove/rename includes to match new TypeLayout header structure
- **Header comments** (lines ~33-37): API summary block starting with `// TypeLayout library —` — update function names, descriptions
- **Domain admission comment block** (lines ~42-43): starting with `// Platform:` and `// Type safety:` — update if admission API changed
- **using declarations** (search `using boost::typelayout::`): update renamed symbols
- **Registration macros** (search `TYPELAYOUT_OPAQUE_`): update if macro names changed in TypeLayout
- **Any direct API calls**: `is_byte_copy_safe_v`, `get_layout_signature`, `is_transfer_safe`

### 3.2 Tools: `tools/export_signatures.cpp`

- Uses `<boost/typelayout/tools/sig_export.hpp>` and `SigExporter` API
- Check if `add_relocatable<T>()`, `write()`, `write_stdout()`, `platform_name()` are renamed

### 3.3 Tools: `tools/check_compat.cpp`

- Uses `<boost/typelayout/tools/compat_check.hpp>` and `compat::layout_match`, `compat::CompatReporter`
- Inlines `TYPELAYOUT_CHECK_COMPAT` logic (creates CompatReporter, adds platforms, prints report)
- Check if `compat::layout_match`, `compat::CompatReporter`, `add_platform()`, `print_report()` are renamed

### 3.4 Signature files: `tools/sigs/*.sig.hpp`

- These are generated files — if the signature format or `TypeEntry` struct changed in TypeLayout, they need manual update or regeneration
- `TypeEntry` struct fields (as of 44b6e0f): `name`, `layout_sig`, `byte_copy_safe`
- Each type needs: `<Name>_layout[]` (signature string), `<Name>_byte_copy_safe` (bool), and the registry entry `{"<Name>", <Name>_layout, <Name>_byte_copy_safe}`
- Full regeneration happens via Docker build (`bash ./build.sh`) — note for Phase 5

### 3.5 Test files: `tests/*.cpp`

Search for TypeLayout API usage in test files. Key patterns:
- `using boost::typelayout::compat::SafetyLevel` (was `detail::SafetyLevel` before 44b6e0f)
- `using boost::typelayout::compat::classify_signature` (was `detail::classify_signature`)
- `using boost::typelayout::compat::safety_level_name` (was `detail::safety_level_name`)
- Include comments mentioning old namespace (e.g., `// detail::classify_signature`)

Files known to use these:
- `tests/test_type_signatures.cpp`
- `tests/test_policy_trait.cpp`

### 3.6 Specs: `openspec/specs/`

Search for old API names in these spec files and update:
- `openspec/specs/type-safety-delegation/spec.md` — references `opaque_copy_safe` (was `opaque_elements_safe`), TypeLayout predicates
- `openspec/specs/type-signature/spec.md` — references `compat::classify_signature` (was `detail::classify_signature`), signature API

### 3.7 Docs: `docs/`

Search for old API names in docs and update:
- `docs/technical_overview.md` — references `compat::classify_signature` (was `detail::classify_signature`)
- Other docs that reference changed API names (use grep to find)

### 3.8 CLAUDE.md

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

## Current TypeLayout API Reference (as of 44b6e0f)

| API | Header | Namespace | Notes |
|-----|--------|-----------|-------|
| `is_byte_copy_safe_v<T>` | `admission.hpp` | `boost::typelayout` | Recursive domain admission |
| `opaque_copy_safe<T>` | `fwd.hpp` | `boost::typelayout` | Was `opaque_elements_safe` |
| `get_layout_signature<T>()` | `signature.hpp` | `boost::typelayout` | Binary layout signature |
| `is_transfer_safe<T>(sig)` | `transfer.hpp` (core) | `boost::typelayout` | Moved from `tools/transfer.hpp` |
| `has_pointer_v<T>` | `layout_traits.hpp` | `boost::typelayout` | New public predicate |
| `has_padding_v<T>` | `layout_traits.hpp` | `boost::typelayout` | New public predicate |
| `has_opaque_v<T>` | `layout_traits.hpp` | `boost::typelayout` | New public predicate |
| `SafetyLevel` | `tools/safety_level.hpp` | `boost::typelayout::compat` | Was `detail::` |
| `classify_signature()` | `tools/safety_level.hpp` | `boost::typelayout::compat` | Was `detail::` |
| `safety_level_name()` | `tools/safety_level.hpp` | `boost::typelayout::compat` | Was `detail::` |
| `layout_match()` | `tools/compat_check.hpp` | `boost::typelayout::compat` | Unchanged |
| `CompatReporter` | `tools/compat_check.hpp` | `boost::typelayout::compat` | `TypeResult` now has `byte_copy_safe` field |
| `SigExporter` | `tools/sig_export.hpp` | `boost::typelayout` | `ExportEntry` now has `byte_copy_safe` field |
| `TypeEntry` | `tools/sig_types.hpp` | `boost::typelayout` | Now has `byte_copy_safe` field |
| `long double` sig tag | `config.hpp` / `type_map.hpp` | — | `fld64`/`fld80`/`fld106`/`fld128` (was `fld`) |
