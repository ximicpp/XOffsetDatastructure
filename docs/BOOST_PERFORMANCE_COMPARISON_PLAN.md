# Boost Baseline Performance Comparison Plan

## Goal

Measure the runtime impact of replacing the old Boost-based container/backend path
with the current frozen-container + custom arena runtime.

## Baseline

- baseline commit: `6d033a77`
- baseline runtime characteristics:
  - `boost::container` for `XString`, `XVector`, `XSet`, `XMap`
  - Boost-style managed-memory backend / segment-manager allocation path

## Current target

- current branch / working tree
- frozen-layout main containers
- custom arena backend

## Method

Run the same benchmark source against both trees:

1. export the baseline commit into a temporary tree
2. export the exact submodule revisions recorded by that commit
3. compile `tools/bench_compare.cpp` twice with the same compiler and flags
4. run both binaries on the same machine
5. join the TSV outputs into a summary table

The benchmark output records:

- `used_bytes`
  - actual buffer bytes in use after the workload
- `wire_bytes`
  - serialized transport bytes when the workload produces or consumes a wire image

This avoids mixing real runtime footprint with `sizeof(T) * count` style estimates.

## Workloads

- `vector_int_append_prealloc`
  - isolates append cost for trivial vector elements with sufficient arena headroom
- `vector_item_append_prealloc`
  - append non-trivial records with nested `XString`
- `map_insert_find_prealloc`
  - insert and lookup `int -> XString`
- `string_assign_churn`
  - repeated reassignment of one `XString` with varied sizes
- `build_and_save_snapshot`
  - end-to-end build + save of a medium object graph
- `load_snapshot_batch`
  - repeated reopen/load cost from the same serialized payload
- `fragment_and_compact_snapshot`
  - fragmentation + compaction path

## Exploratory workload

- `vector_int_append_growth`
  - low-reserve growth stress
  - not part of the default matrix because both runtimes can hit allocation
    failure under aggressive small-buffer settings; run it explicitly with
    `bench_compare vector_int_append_growth` when investigating growth limits

## Output

`tools/run_perf_compare.sh` produces:

- `baseline.tsv`
- `current.tsv`
- `summary.tsv`

under `build/perf_compare/<timestamp>/`.
