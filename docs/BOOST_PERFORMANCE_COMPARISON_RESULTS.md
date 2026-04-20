# Boost Baseline Performance Comparison Results

## Run

- date: `2026-04-20`
- baseline commit: `6d033a77`
- compiler: the same local Clang toolchain used by the current build
- artifacts:
  - `build/perf_compare/20260420-181355/baseline.tsv`
  - `build/perf_compare/20260420-181355/current.tsv`
  - `build/perf_compare/20260420-181355/summary.tsv`

## Benchmark reasonableness

The benchmark was adjusted to make the comparison more defensible:

- both trees are built from the same benchmark source with the same compiler/toolchain
- the default matrix includes only stable workloads that both runtimes complete
- footprint reporting no longer mixes `sizeof(T) * count` estimates with real serialized sizes
- results now distinguish:
  - `used_bytes`: actual in-buffer footprint
  - `wire_bytes`: actual transport bytes when serialization is part of the workload

The remaining interpretation boundary is deliberate:

- `build_and_save_snapshot` and `load_snapshot_batch` are end-to-end measurements, so they include
  both runtime cost and the effect of different wire sizes
- `load_snapshot_batch` is a raw load/reopen comparison because the historical Boost baseline does
  not have the current verified-load path

## Stable comparison matrix

| benchmark | baseline median (ms) | current median (ms) | speedup vs baseline | baseline used (B) | current used (B) | baseline wire (B) | current wire (B) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `vector_int_append_prealloc` | 1.290 | 1.162 | 1.110x | 424104 | 1049000 | 0 | 0 |
| `vector_item_append_prealloc` | 12.177 | 9.762 | 1.247x | 1937680 | 2752232 | 0 | 0 |
| `map_insert_find_prealloc` | 12.537 | 10.546 | 1.189x | 1180032 | 1572632 | 0 | 0 |
| `string_assign_churn` | 2.388 | 2.040 | 1.171x | 376 | 592 | 0 | 0 |
| `build_and_save_snapshot` | 20.373 | 12.622 | 1.614x | 2600680 | 2947744 | 2614456 | 2947744 |
| `load_snapshot_batch` | 12.421 | 21.691 | 0.573x | 2600680 | 2947744 | 2614456 | 2947744 |
| `fragment_and_compact_snapshot` | 46.642 | 43.734 | 1.066x | 4133968 | 2915432 | 2110216 | 2915432 |

## Takeaways

- Current runtime is faster on all stable workloads except `load_snapshot_batch`.
- Largest win is `build_and_save_snapshot`: about `1.61x`.
- Flat container workloads also improved:
  - `vector_item_append_prealloc`: about `1.25x`
  - `map_insert_find_prealloc`: about `1.19x`
  - `string_assign_churn`: about `1.17x`
  - `vector_int_append_prealloc`: about `1.11x`
- `fragment_and_compact_snapshot` improved modestly: about `1.07x`.
- `load_snapshot_batch` regressed: current runtime is about `1.74x` slower on raw reopen/load.

## Footprint observations

- In-memory `used_bytes` are mostly larger on the current runtime for the preallocated mutation cases.
- End-to-end compaction changed the picture:
  - fragmented+compacted `used_bytes` are smaller on the current runtime
  - but `wire_bytes` are larger on the current runtime for both snapshot cases

## Exploratory note

- `vector_int_append_growth` remains exploratory and is excluded from the stable matrix.
- Under the aggressive low-reserve setting used for growth stress, both runtimes fail allocation
  instead of producing stable timing data.
