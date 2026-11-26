# Realistic RPG & iso-byte structural complexity summary (median of 9 processes)

- `byte_serializable: flat_item=1 flat_item_pair=0 flat_pos=1 flat_skill=1 | rich_item=0 rich_equip=0 rich_quest=0`
- `containers/strings count: Flat-3KB=14, Rich-3KB=107, Flat-12KB=38, Rich-12KB=373`

## 1A. L1d-resident iso-byte (3 KB): Flat-3KB vs Rich-3KB (`trusted_reuse`, ns)

| Library | Flat-3KB | Rich-3KB | Rich/Flat | Flat ratio | Rich ratio | Rich varied | varied ratio | Rich fresh | fresh ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| XOffset | 123.8 | 165.3 | 1.34x | 1.0x | 1.0x | 195.0 | 1.0x | 288.4 | 1.0x |
| zpp::bits | 720.2 | 1,986.1 | 2.76x | 5.8x | 12.0x | 3,289.5 | 16.9x | 11,342.5 | 39.3x |
| FlatBuffers | 1,393.5 | 7,550.8 | 5.42x | 11.3x | 45.7x | 9,849.4 | 50.5x | 16,434.3 | 57.0x |
| cista raw | 1,333.8 | 3,045.3 | 2.28x | 10.8x | 18.4x | 3,444.9 | 17.7x | 3,641.2 | 12.6x |
| cista offset | 1,346.2 | 3,009.3 | 2.24x | 10.9x | 18.2x | 3,264.6 | 16.7x | 3,624.3 | 12.6x |
| Cap'n Proto | 4,928.6 | 8,094.3 | 1.64x | 39.8x | 49.0x | 9,588.7 | 49.2x | 16,155.2 | 56.0x |
| MessagePack | 13,461.7 | 13,372.2 | 0.99x | 108.7x | 80.9x | 15,611.4 | 80.1x | 21,824.6 | 75.7x |
| Protocol Buffers | 19,131.7 | 21,374.4 | 1.12x | 154.5x | 129.3x | 25,691.6 | 131.8x | 38,456.9 | 133.3x |

## 1B. Commercial character (12 KB): Flat-12KB vs Rich-12KB (`trusted_reuse`, ns)

| Library | Flat-12KB | Rich-12KB | Rich/Flat | Flat ratio | Rich ratio | Rich varied | varied ratio | Rich fresh | fresh ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| XOffset | 386.1 | 951.0 | 2.46x | 1.0x | 1.0x | 941.9 | 1.0x | 1,157.2 | 1.0x |
| zpp::bits | 2,477.8 | 6,329.0 | 2.55x | 6.4x | 6.7x | 14,142.9 | 15.0x | 36,970.7 | 31.9x |
| FlatBuffers | 4,984.4 | 27,943.4 | 5.61x | 12.9x | 29.4x | 36,134.9 | 38.4x | 57,779.8 | 49.9x |
| cista raw | 4,766.5 | 11,052.8 | 2.32x | 12.3x | 11.6x | 12,044.6 | 12.8x | 11,610.3 | 10.0x |
| cista offset | 4,723.6 | 10,531.0 | 2.23x | 12.2x | 11.1x | 11,249.6 | 11.9x | 11,222.6 | 9.7x |
| Cap'n Proto | 18,223.6 | 29,490.6 | 1.62x | 47.2x | 31.0x | 33,190.6 | 35.2x | 62,309.8 | 53.8x |
| MessagePack | 49,659.0 | 48,919.9 | 0.99x | 128.6x | 51.4x | 55,890.7 | 59.3x | 76,421.1 | 66.0x |
| Protocol Buffers | 95,856.2 | 89,183.7 | 0.93x | 248.3x | 93.8x | 108,440.7 | 115.1x | 150,721.3 | 130.2x |

## 2. Server-to-server handoff (`Receive -> Mutate -> Send`, 16 varied players) & In-place `rw_rich` (ns)

| Library | Rich-3KB hop (trusted) | ratio | Rich-3KB hop (checked) | Rich-12KB hop (trusted) | ratio | Rich-12KB hop (checked) | ratio | Rich-12KB rw_rich |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| XOffset | 2,000.0 | 1.0x | 3,170.3 | 6,324.9 | 1.0x | 10,384.7 | 1.0x | 508.8 |
| zpp::bits | 4,300.7 | 2.2x | 4,300.7 | 15,429.2 | 2.4x | 15,429.2 | 1.5x | 657.4 |
| FlatBuffers | 10,350.6 | 5.2x | 13,514.1 | 37,633.0 | 5.9x | 46,594.1 | 4.5x | - |
| cista raw | 4,158.8 | 2.1x | 12,648.8 | 13,790.2 | 2.2x | 43,044.6 | 4.1x | 720.8 |
| cista offset | 4,075.8 | 2.0x | 14,163.7 | 13,451.2 | 2.1x | 49,371.6 | 4.8x | 836.3 |
| Cap'n Proto | 10,482.0 | 5.2x | 10,482.0 | 35,113.4 | 5.6x | 35,113.4 | 3.4x | - |
| MessagePack | 16,239.6 | 8.1x | 16,239.6 | 60,635.8 | 9.6x | 60,635.8 | 5.8x | - |
| Protocol Buffers | 26,302.4 | 13.2x | 26,302.4 | 103,785.1 | 16.4x | 103,785.1 | 10.0x | 606.2 |

## 3. Equal-check tier: `checked_reuse`, `varied_checked`, and `fresh_checked` (ns)

| Library | Rich-3KB checked | Rich-3KB varied | Rich-3KB fresh | Rich-12KB checked | Rich-12KB varied | ratio | Rich-12KB fresh | ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| XOffset | 1,215.7 | 1,267.2 | 1,379.4 | 4,532.8 | 4,668.1 | 1.0x | 4,714.7 | 1.0x |
| zpp::bits | 1,986.1 | 3,289.5 | 11,342.5 | 6,329.0 | 14,142.9 | 3.0x | 36,970.7 | 7.8x |
| FlatBuffers | 10,437.3 | 12,815.9 | 20,286.6 | 36,581.3 | 45,277.0 | 9.7x | 66,847.4 | 14.2x |
| cista raw | 10,830.2 | 11,662.5 | 12,251.8 | 38,775.2 | 41,808.3 | 9.0x | 38,626.6 | 8.2x |
| cista offset | 12,437.1 | 13,228.4 | 13,056.6 | 42,841.0 | 46,404.4 | 9.9x | 44,202.3 | 9.4x |
| Cap'n Proto | 8,094.3 | 9,588.7 | 16,155.2 | 29,490.6 | 33,190.6 | 7.1x | 62,309.8 | 13.2x |
| MessagePack | 13,372.2 | 15,611.4 | 21,824.6 | 48,919.9 | 55,890.7 | 12.0x | 76,421.1 | 16.2x |
| Protocol Buffers | 21,374.4 | 25,691.6 | 38,456.9 | 89,183.7 | 108,440.7 | 23.2x | 150,721.3 | 32.0x |

## 4. XOffset arena layout variants (reserved vs compacted vs unreserved)

| Workload | Variant | wire_raw / zstd3 | trusted_reuse | varied_reuse | fresh_trusted | fresh_checked | hop_trusted | hop_checked | varied_checked |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rich-3KB | XOffset | 4,384 / 3,079 | 165.3 | 195.0 | 288.4 | 1,379.4 | 2,000.0 | 3,170.3 | 1,267.2 |
| Rich-3KB | XOffset (compacted) | 4,560 / 3,099 | 178.2 | 201.6 | 301.7 | 972.3 | 722.1 | 1,374.8 | 839.5 |
| Rich-3KB | XOffset (unreserved) | 5,944 / 3,215 | 217.1 | 294.6 | 358.8 | 1,507.5 | 942.5 | 2,168.7 | 1,439.6 |
| Rich-12KB | XOffset | 16,256 / 10,850 | 951.0 | 941.9 | 1,157.2 | 4,714.7 | 6,324.9 | 10,384.7 | 4,668.1 |
| Rich-12KB | XOffset (compacted) | 17,224 / 10,969 | 1,084.2 | 1,027.1 | 1,327.8 | 3,417.6 | 1,818.4 | 3,922.9 | 3,201.1 |
| Rich-12KB | XOffset (unreserved) | 20,872 / 11,407 | 1,479.7 | 1,558.9 | 1,642.4 | 5,418.8 | 2,227.4 | 6,040.8 | 5,369.3 |

## 5. Wire sizes (raw / zstd-3 bytes) and send/recv split (ns)

| Library | Flat-3KB raw/z3 | Rich-3KB raw/z3 | Flat-12KB raw/z3 | Rich-12KB raw/z3 | Rich-3KB send/recv_t/recv_c | Rich-12KB send/recv_t/recv_c |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| XOffset | 3,160/3,023 | 4,384/3,079 | 11,896/11,563 | 16,256/10,850 | 86.0/88.4/1,132.1 | 222.4/232.3/3,996.4 |
| zpp::bits | 2,916/2,926 | 3,141/2,714 | 11,460/11,470 | 11,860/9,834 | 862.1/-/980.8 | 3,127.1/-/3,258.7 |
| FlatBuffers | 3,680/3,128 | 4,584/3,283 | 14,240/11,922 | 16,416/11,544 | 6,114.1/1,198.4/3,962.0 | 22,452.4/4,383.5/13,800.4 |
| cista raw | 3,216/2,992 | 5,027/3,101 | 12,336/11,550 | 18,450/11,190 | 2,566.0/372.4/8,358.1 | 9,218.4/1,335.1/30,204.1 |
| cista offset | 3,216/2,992 | 5,027/3,106 | 12,336/11,529 | 18,450/11,218 | 2,876.3/97.6/9,303.3 | 10,139.7/496.3/32,972.6 |
| Cap'n Proto | 3,656/3,131 | 4,432/3,038 | 14,120/11,922 | 16,384/10,777 | 2,734.1/-/4,834.9 | 10,099.6/-/18,984.2 |
| MessagePack | 3,542/3,391 | 3,258/2,977 | 13,974/13,195 | 12,515/11,036 | 2,700.5/-/10,490.0 | 9,498.8/-/40,120.8 |
| Protocol Buffers | 4,095/3,800 | 3,587/3,341 | 16,160/14,636 | 13,889/12,247 | 4,661.2/-/16,650.3 | 19,489.3/-/68,412.4 |

## 6. Read-only zero-copy view (send + view without unpack, ns)

| Workload | Tier | XOffset | FlatBuffers | cista raw | cista offset | Cap'n Proto |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Rich-3KB | `view_admit` | 91.2 | 6,303.8 | 3,002.7 | 2,845.4 | - |
| Rich-3KB | `view_bounds` | 483.4 | 9,349.7 | 10,711.6 | 12,217.4 | 2,994.7 |
| Rich-12KB | `view_admit` | 249.3 | 22,654.2 | 10,017.4 | 10,143.3 | - |
| Rich-12KB | `view_bounds` | 1,590.8 | 33,639.5 | 38,280.3 | 43,394.8 | 10,177.6 |
