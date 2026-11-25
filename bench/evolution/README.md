# Loading stored data of an older generation: instruction counts (record only)

`run_ir.sh` builds `evolution_ir.cpp` with the P2996 toolchain (-O3, libc++) and
counts instructions per stored image under callgrind, with the glibc setting of
`tools/perf_gate`. The messages are the SLG prototype's, at the Ir gate's scale S
(seed 20261021, cycle 4): A = BattleMsg request, B = PlayerState (season
migration), D = Report. The older generation of each lacks one or two fields
(`Hero::star`, `Event::target`, `PlayerState::items`); see the source. Not a gate:
numbers only show the order of the cost of a one-time upgrade at load.

Peers of different generations do not exchange messages (the handshake refuses
them), so there is no per-message conversion to measure; versions of one add-only
family exchange the same bytes, measured by `family_ir.cpp` (`family.*` rows of the
Ir gate, docs/PERF_GATE.md).

| Operation | What it counts |
| --- | --- |
| load_same | `load_verified` of the working version's image (baseline) |
| load_old | `load_verified` of the old generation's image as that generation |
| load_upgrade | `evolve_load_verified`: load_verified as the old generation + upgrade |

## 2026-10-27, cloud x86_64, P2996 d77eff1, valgrind 3.22

Measured before the per-peer layer was removed; `load_upgrade` ran the same code
then (`XPeerSchemas::load_verified` called what `evolve_load_verified` calls), under
the names recv_same and recv_old.

| Scenario | image / old image | load_same | load_old | load_upgrade |
| --- | --- | ---: | ---: | ---: |
| A BattleMsg | 1032 / 1032 B | 3,642 | 3,643 | 11,683 (x3.2) |
| B PlayerState | 14912 / 9584 B | 28,478 | 14,383 | 76,568 (x2.7) |
| D Report | 192 / 160 B | 2,416 | 2,408 | 5,172 (x2.1) |

Ir per image; the factors are against the same-version baseline. Loading an older
generation's image costs two to three same-version loads, once per image: after it
is saved again it is an image of the working generation. Field-by-field conversion
has not been optimized (records with the same shape and names are already one
memcpy).
