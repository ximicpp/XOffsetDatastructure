# CK12S sizes (arm64, P2996 d77eff1 + libc++, the slg_bench compile flags)

| point | XBufferCore | XBuffer | optional<XBuffer> | XOffsetRunner | players_ (512 x XBuffer) |
| --- | ---: | ---: | ---: | ---: | ---: |
| new | 216 | 232 | 240 | 9560 | 118784 |
| anc | 224 | 240 | 248 | 9096 | 122880 |
| old | 224 | 248 | 256 | 9944 | 126976 |
| nc | 216 | 232 | 240 | 9560 | 118784 |

malloc_good_size (FreeTable: 12472 at anc, 12480 at old/new, from the cloud layout dump):

```
232 -> 256
240 -> 256
248 -> 256
256 -> 256
9096 -> 10240
9560 -> 10240
9944 -> 10240
12472 -> 14336
12480 -> 14336
118784 -> 131072
122880 -> 131072
126976 -> 131072
```
