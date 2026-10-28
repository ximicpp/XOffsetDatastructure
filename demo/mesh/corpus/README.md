# Device mesh demo corpus (schema version 1)

Written on Linux x86-64 (g++-13, C++20 field lists) by

    xmesh_selftest --corpus-write demo/mesh/corpus

and checked on every device column by

    xmesh_selftest --corpus-check demo/mesh/corpus

which recomputes everything and fails on any byte difference.

- `index.txt`: for each type x scale (small, medium, large) x save mode
  (verified, compact), one job over the five-device route in the `job`
  line: every hop's content digest, wire digest (FNV-1a 64 of the data
  part) and length, exactly as `replay()` gives them and the hub expects
  them; and, for small and medium, the bad data part of every EvilCase
  (`make_evil()`).
- `<Type>-small-<mode>-h<k>.xmw`: the data part route[k] sends in the small
  jobs, the wires themselves (48 files).

A change to `schema.hpp`, to state 0 or to a mutation (`src/types/`)
changes the corpus: rewrite it in the same commit.
