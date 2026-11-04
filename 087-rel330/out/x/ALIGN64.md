# align64 pair: fe8c0743 (as head) vs 1c528d67 (as anc), L2 SLG command (report only): format xoffset, processes head 12 anchor 12

| metric | head median | anchor median | head/anc | 95% interval |
|---|---|---|---|---|
| A.req_decode | 58292 | 57986 | 1.0053 | [0.9982, 1.0131] |
| A.res_decode | 82788 | 84180 | 0.9835 | [0.9730, 0.9976] |
| A.apply | 101752 | 104844 | 0.9705 | [0.9642, 0.9902] |
| A.req_encode | 198790 | 198956 | 0.9992 | [0.9966, 1.0181] |
| A.res_encode | 22234 | 22167 | 1.0030 | [0.9981, 1.0117] |
| A.cleanup | 6521 | 6535 | 0.9979 | [0.9830, 1.0135] |
| A.simulate | 109314 | 108477 | 1.0077 | [1.0009, 1.0131] |
| B.back_decode | 352722 | 356648 | 0.9890 | [0.9801, 1.0064] |
| B.back_encode | 57262 | 57542 | 0.9951 | [0.9886, 1.0050] |
| B.cleanup | 3612 | 3521 | 1.0258 | [1.0032, 1.0433] |
| B.in_decode | 234480 | 237059 | 0.9891 | [0.9704, 1.0049] |
| B.out_encode | 179988 | 179826 | 1.0009 | [0.9961, 1.0067] |
| B.season_edits | 515790 | 517751 | 0.9962 | [0.9931, 0.9981] |
| B.writeback | 35489 | 36306 | 0.9775 | [0.9679, 0.9841] |
| C.replace | 208 | 208 | 1.0000 | [0.8029, 1.0000] |
| C.restore | 43813 | 43823 | 0.9998 | [0.9924, 1.0084] |
| C.snapshot | 39510 | 39969 | 0.9885 | [0.9805, 1.0010] |
| C.snapshot2 | 32042 | 31844 | 1.0062 | [0.9941, 1.0125] |
| C.ticks | 124229 | 123094 | 1.0092 | [1.0035, 1.0120] |
| D.read | 71022 | 70179 | 1.0120 | [0.9970, 1.0362] |
| S.ticks_middle | 1218406 | 1218479 | 0.9999 | [0.9950, 1.0076] |

xoffset byte metrics (every cycle, every metric) head vs anchor: identical (287 / 287 distinct entries)

align64 pair: fe8c0743 (as head) vs 1c528d67 (as anc), L2 SLG command head/anc: A.req_decode 1.005, A.res_decode 0.983, A.apply 0.971, A.req_encode 0.999, A.res_encode 1.003
