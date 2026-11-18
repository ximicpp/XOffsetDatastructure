# SLG scorecard

- Batches: xoffset 8, xoffset_d3 8, zpp_bits 8, zpp_bits shared 8, zpp_bits_dirty shared 8
- Ratio = xoffset_trusted ÷ the strongest rival (the one XOffset is slowest against), geometric mean over batches; brackets are the batch min–max. Rivals of the D cells include their shared-encoding approach (the "shared" columns, 4.0 W1). Win < 0.95, tie 0.95–1.05, loss > 1.05.

**Main cells 19: win 19, tie 0, loss 0.** UL.send is report only and not counted.

| Group | Cell | Scenario | ÷ strongest rival | Strongest rival | Verdict | ÷ zpp_bits | ÷ zpp_bits shared | ÷ zpp_bits_dirty shared |
| --- | --- | --- | ---: | --- | --- | ---: | ---: | ---: |
| server to server | A | battle request and result | 0.382 | zpp_bits | win | 0.382 [0.38–0.39] |  |  |
| server to server | B | server migration | 0.238 | zpp_bits | win | 0.238 [0.23–0.24] |  |  |
| server to server | S | save and load | 0.164 | zpp_bits | win | 0.164 [0.16–0.17] |  |  |
| server to server | SD | delta save | 0.196 | zpp_bits | win | 0.196 [0.19–0.20] |  |  |
| server send | L.send | login full state | 0.340 | zpp_bits | win | 0.340 [0.34–0.34] |  |  |
| server send | EB.send | browse full state | 0.388 | zpp_bits | win | 0.388 [0.38–0.39] |  |  |
| server send | LD.send | own delta | 0.486 | zpp_bits | win | 0.486 [0.48–0.49] |  |  |
| server send | UP.send | view delta, peace | 0.709 | zpp_bits | win | 0.709 [0.69–0.73] |  |  |
| server send | UC.send | view delta, conflict | 0.384 | zpp_bits | win | 0.384 [0.38–0.39] |  |  |
| server send | DP | shared delta, peace: publish + send ÷ rival UP.send or its shared encoding DP | 0.793 | zpp_bits_dirty shared | win | 0.453 [0.44–0.47] | 0.765 [0.76–0.79] | 0.793 [0.78–0.82] |
| server send | DC | shared delta, conflict: publish + send ÷ rival UC.send or its shared encoding DC | 0.840 | zpp_bits_dirty shared | win | 0.109 [0.11–0.11] | 0.548 [0.54–0.55] | 0.840 [0.83–0.85] |
| client receive | L.recv | login full state | 0.270 | zpp_bits | win | 0.270 [0.27–0.27] |  |  |
| client receive | EBW.recv | browse full state (warm view) | 0.269 | zpp_bits | win | 0.269 [0.27–0.27] |  |  |
| client receive | LDW.recv | own delta (warm view) | 0.379 | zpp_bits | win | 0.379 [0.37–0.40] |  |  |
| client receive | UPW.recv | view delta, peace (warm view) | 0.795 | zpp_bits | win | 0.795 [0.76–0.81] |  |  |
| client receive | UCW.recv | view delta, conflict (warm view) | 0.416 | zpp_bits | win | 0.416 [0.41–0.42] |  |  |
| client receive | DPTW.recv | shared delta, peace ÷ rival UPW.recv or its shared encoding DPTW | 0.476 | zpp_bits_dirty shared | win | 0.287 [0.27–0.31] | 0.431 [0.42–0.47] | 0.476 [0.44–0.54] |
| client receive | DCTW.recv | shared delta, conflict ÷ rival UCW.recv or its shared encoding DCTW | 0.551 | zpp_bits_dirty shared | win | 0.144 [0.14–0.15] | 0.388 [0.38–0.40] | 0.551 [0.54–0.56] |
| uplink | UL.recv | server receives orders (bounds check) | 0.814 | zpp_bits | win | 0.814 [0.79–0.88] |  |  |
| uplink | UL.send | client sends orders (report only) | 0.769 | zpp_bits | report only (win) | 0.769 [0.74–0.79] |  |  |
| report only | US.send | view delta, siege | 0.369 | zpp_bits | report only (win) | 0.369 [0.37–0.37] |  |  |
| report only | DS | shared delta, siege: publish + send ÷ rival US.send or its shared encoding DS | 0.760 | zpp_bits_dirty shared | report only (win) | 0.065 [0.06–0.07] | 0.378 [0.37–0.38] | 0.760 [0.75–0.77] |
| report only | USW.recv | view delta, siege (warm view) | 0.488 | zpp_bits | report only (win) | 0.488 [0.48–0.49] |  |  |
| report only | DSTW.recv | shared delta, siege ÷ rival USW.recv or its shared encoding DSTW | 0.575 | zpp_bits_dirty shared | report only (win) | 0.116 [0.11–0.12] | 0.293 [0.29–0.30] | 0.575 [0.56–0.59] |
| report only | EB.recv | browse full state (cold) | 0.338 | zpp_bits | report only (win) | 0.338 [0.33–0.34] |  |  |
| report only | LD recv+apply | own delta (cold) | 0.526 | zpp_bits | report only (win) | 0.526 [0.52–0.53] |  |  |
| report only | UP recv+apply | view delta, peace (cold) | 0.958 | zpp_bits | report only (tie) | 0.958 [0.94–0.99] |  |  |
| report only | UC recv+apply | view delta, conflict (cold) | 0.371 | zpp_bits | report only (win) | 0.371 [0.37–0.38] |  |  |

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
