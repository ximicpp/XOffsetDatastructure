# SLG scorecard

- Batches: xoffset 8, xoffset_d3 8, zpp_bits 8, zpp_bits shared 8, zpp_bits_dirty shared 8
- Ratio = xoffset_trusted ÷ the strongest rival (the one XOffset is slowest against), geometric mean over batches; brackets are the batch min–max. Rivals of the D cells include their shared-encoding approach (the "shared" columns, 4.0 W1). Win < 0.95, tie 0.95–1.05, loss > 1.05.

**Main cells 19: win 19, tie 0, loss 0.** UL.send is report only and not counted.

| Group | Cell | Scenario | ÷ strongest rival | Strongest rival | Verdict | ÷ zpp_bits | ÷ zpp_bits shared | ÷ zpp_bits_dirty shared |
| --- | --- | --- | ---: | --- | --- | ---: | ---: | ---: |
| server to server | A | battle request and result | 0.381 | zpp_bits | win | 0.381 [0.38–0.39] |  |  |
| server to server | B | server migration | 0.238 | zpp_bits | win | 0.238 [0.23–0.24] |  |  |
| server to server | S | save and load | 0.164 | zpp_bits | win | 0.164 [0.16–0.17] |  |  |
| server to server | SD | delta save | 0.194 | zpp_bits | win | 0.194 [0.19–0.20] |  |  |
| server send | L.send | login full state | 0.339 | zpp_bits | win | 0.339 [0.34–0.34] |  |  |
| server send | EB.send | browse full state | 0.385 | zpp_bits | win | 0.385 [0.38–0.39] |  |  |
| server send | LD.send | own delta | 0.479 | zpp_bits | win | 0.479 [0.47–0.48] |  |  |
| server send | UP.send | view delta, peace | 0.714 | zpp_bits | win | 0.714 [0.71–0.73] |  |  |
| server send | UC.send | view delta, conflict | 0.385 | zpp_bits | win | 0.385 [0.38–0.39] |  |  |
| server send | DP | shared delta, peace: publish + send ÷ rival UP.send or its shared encoding DP | 0.794 | zpp_bits_dirty shared | win | 0.458 [0.45–0.46] | 0.765 [0.76–0.78] | 0.794 [0.79–0.81] |
| server send | DC | shared delta, conflict: publish + send ÷ rival UC.send or its shared encoding DC | 0.833 | zpp_bits_dirty shared | win | 0.109 [0.11–0.11] | 0.546 [0.54–0.55] | 0.833 [0.82–0.84] |
| client receive | L.recv | login full state | 0.270 | zpp_bits | win | 0.270 [0.26–0.27] |  |  |
| client receive | EBW.recv | browse full state (warm view) | 0.272 | zpp_bits | win | 0.272 [0.27–0.27] |  |  |
| client receive | LDW.recv | own delta (warm view) | 0.369 | zpp_bits | win | 0.369 [0.35–0.38] |  |  |
| client receive | UPW.recv | view delta, peace (warm view) | 0.796 | zpp_bits | win | 0.796 [0.72–0.85] |  |  |
| client receive | UCW.recv | view delta, conflict (warm view) | 0.406 | zpp_bits | win | 0.406 [0.40–0.41] |  |  |
| client receive | DPTW.recv | shared delta, peace ÷ rival UPW.recv or its shared encoding DPTW | 0.481 | zpp_bits_dirty shared | win | 0.287 [0.26–0.30] | 0.442 [0.43–0.45] | 0.481 [0.46–0.51] |
| client receive | DCTW.recv | shared delta, conflict ÷ rival UCW.recv or its shared encoding DCTW | 0.558 | zpp_bits_dirty shared | win | 0.146 [0.14–0.15] | 0.393 [0.38–0.42] | 0.558 [0.54–0.59] |
| uplink | UL.recv | server receives orders (bounds check) | 0.832 | zpp_bits | win | 0.832 [0.81–0.85] |  |  |
| uplink | UL.send | client sends orders (report only) | 0.765 | zpp_bits | report only (win) | 0.765 [0.75–0.78] |  |  |
| report only | US.send | view delta, siege | 0.369 | zpp_bits | report only (win) | 0.369 [0.36–0.37] |  |  |
| report only | DS | shared delta, siege: publish + send ÷ rival US.send or its shared encoding DS | 0.750 | zpp_bits_dirty shared | report only (win) | 0.065 [0.06–0.07] | 0.376 [0.37–0.38] | 0.750 [0.75–0.76] |
| report only | USW.recv | view delta, siege (warm view) | 0.465 | zpp_bits | report only (win) | 0.465 [0.46–0.47] |  |  |
| report only | DSTW.recv | shared delta, siege ÷ rival USW.recv or its shared encoding DSTW | 0.570 | zpp_bits_dirty shared | report only (win) | 0.115 [0.11–0.12] | 0.291 [0.29–0.30] | 0.570 [0.56–0.58] |
| report only | EB.recv | browse full state (cold) | 0.339 | zpp_bits | report only (win) | 0.339 [0.34–0.34] |  |  |
| report only | LD recv+apply | own delta (cold) | 0.520 | zpp_bits | report only (win) | 0.520 [0.52–0.53] |  |  |
| report only | UP recv+apply | view delta, peace (cold) | 0.961 | zpp_bits | report only (tie) | 0.961 [0.95–0.98] |  |  |
| report only | UC recv+apply | view delta, conflict (cold) | 0.366 | zpp_bits | report only (win) | 0.366 [0.36–0.37] |  |  |

## Bytes (report only, per-cycle median, uncompressed (no compression build in this run); A/B/S/SD not compressed)

| Message | Key | XOffset | zpp_bits | zpp_bits shared | zpp_bits_dirty shared | ÷ zpp_bits | ÷ zpp_bits shared | ÷ zpp_bits_dirty shared |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| A request | `A.req` | 586340 | 437166 |  |  | 1.34 |  |  |
| A result | `A.res` | 744936 | 510254 |  |  | 1.46 |  |  |
| B migration | `B.out` | 3357584 | 1625870 |  |  | 2.07 |  |  |
| S save | `S.player` | 3345596 | 1628906 |  |  | 2.05 |  |  |
| SD delta save | `SD.modules` | 1077448 | 697570 |  |  | 1.54 |  |  |
| L login | `L.player` | 599116 | 434355 |  |  | 1.38 |  |  |
| EB browse | `EB.window` | 322974196 | 276928372 |  |  | 1.17 |  |  |
| LD own delta | `LD.patch` | 752272 | 517870 |  |  | 1.45 |  |  |
| UP view peace | `UP.delta` | 1841160 | 1248644 |  |  | 1.47 |  |  |
| UC view conflict | `UC.delta` | 18528172 | 15321512 |  |  | 1.21 |  |  |
| DP shared peace ÷ rival UP | `DP.delta` | 739779 | 1248644 | 1155628 | 786246 | 0.59 | 0.64 | 0.94 |
| DC shared conflict ÷ rival UC | `DC.delta` | 6221568 | 15321512 | 15225712 | 5570815 | 0.41 | 0.41 | 1.12 |
| UL uplink orders | `UL.order` | 139328 | 77872 |  |  | 1.79 |  |  |
