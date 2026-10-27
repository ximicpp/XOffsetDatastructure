# Cross-version receive and send: instruction counts (record only)

`run_ir.sh` builds `evolution_ir.cpp` with the P2996 toolchain (-O3, libc++) and
counts instructions per message under callgrind, with the glibc setting of
`tools/perf_gate`. The messages are the SLG prototype's, at the Ir gate's scale S
(seed 20261021, cycle 4): A = BattleMsg request, B = PlayerState (season
migration), D = Report. The old version of each lacks one or two fields
(`Hero::star`, `Event::target`, `PlayerState::items`); see the source. Not a gate:
numbers only show the order of the conversion cost.

| Operation | What it counts |
| --- | --- |
| recv_same | `load_verified` of the working version (baseline) |
| load_old | `load_verified` of the old version only |
| recv_old | `XPeerSchemas::load_verified`: load_verified as old + upgrade |
| send_same | `save_verified` of the working version (baseline) |
| send_old | `XPeerSchemas::save` to an old peer: downgrade + `save_compact` |

## 2026-10-27, cloud x86_64, P2996 d77eff1, valgrind 3.22 (commit of this file)

| Scenario | wire / old wire | recv_same | load_old | recv_old | send_same | send_old |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| A BattleMsg | 1032 / 1032 B | 3,642 | 3,643 | 11,683 (x3.2) | 1,003 | 11,657 (x11.6) |
| B PlayerState | 14912 / 9584 B | 28,478 | 14,383 | 76,568 (x2.7) | 2,517 | 86,362 (x34) |
| D Report | 192 / 160 B | 2,416 | 2,408 | 5,172 (x2.1) | 552 | 3,360 (x6.1) |

Ir per message; the factors are against the same-version baseline. Receiving an
old version costs two to three same-version receives; sending to an old peer is
a full rebuild of the message instead of a copy of its bytes, so on PlayerState
it costs about three same-version receives. `XBroadcast` pays the send once per
version, not per peer. Field-by-field conversion has not been optimized
(records with the same shape and names are already one memcpy).
