# g9r2_probe pairs

## ins (probe_1, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 54.93 |
| raw | 50.29 |
| raw.size_first | 49.07 |
| raw.ldr | 49.97 |
| raw.stdpos | 50.38 |
| std | 50.21 |
| std.xopos | 49.32 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.105 | 1.091–1.118 | +4.64 |
| xo / raw: container code: library vs raw (L10) | 1.094 | 1.077–1.111 | +4.48 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.027 | 1.009–1.045 | +1.30 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.002 | 0.979–1.025 | +0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.003 | 0.984–1.022 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 0.980 | 0.978–0.982 | -0.90 |
| raw / std: raw / std (what is left after container code) | 1.010 | 0.996–1.024 | +0.08 |

## era (probe_1, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 44.27 |
| raw | 42.81 |
| raw.size_first | 42.56 |
| raw.ldr | 42.56 |
| raw.noscrub | 43.21 |
| raw.stdpos | 42.81 |
| std | 42.72 |
| std.xopos | 42.97 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.042 | 1.037–1.046 | +1.63 |
| xo / raw: container code: library vs raw (L10) | 1.035 | 1.028–1.043 | +1.38 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 0.990 | 0.962–1.019 | +0.24 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.012 | 1.008–1.015 | +0.32 |
| raw / raw.noscrub: M4 scrub of the vacated slot (report only) | 0.991 | 0.983–0.998 | -0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.004 | 0.996–1.012 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 1.006 | 0.999–1.014 | +0.16 |
| raw / std: raw / std (what is left after container code) | 1.006 | 1.001–1.012 | +0.16 |

## ins (probe_2, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 54.77 |
| raw | 50.38 |
| raw.size_first | 48.91 |
| raw.ldr | 49.80 |
| raw.stdpos | 50.29 |
| std | 49.97 |
| std.xopos | 49.40 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.097 | 1.073–1.121 | +4.64 |
| xo / raw: container code: library vs raw (L10) | 1.094 | 1.070–1.119 | +4.23 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.025 | 1.022–1.029 | +1.30 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.005 | 1.001–1.010 | +0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.000 | 0.997–1.004 | +0.08 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 0.990 | 0.983–0.996 | -0.65 |
| raw / std: raw / std (what is left after container code) | 1.002 | 0.998–1.007 | +0.24 |

## era (probe_2, 101 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 44.03 |
| raw | 42.89 |
| raw.size_first | 42.72 |
| raw.ldr | 42.56 |
| raw.noscrub | 43.05 |
| raw.stdpos | 42.81 |
| std | 42.89 |
| std.xopos | 43.05 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.024 | 1.018–1.031 | +1.14 |
| xo / raw: container code: library vs raw (L10) | 1.025 | 1.019–1.030 | +1.22 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.005 | 0.998–1.011 | +0.16 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.006 | 0.997–1.015 | +0.32 |
| raw / raw.noscrub: M4 scrub of the vacated slot (report only) | 0.996 | 0.987–1.004 | -0.16 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.003 | 0.993–1.012 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 1.008 | 0.996–1.019 | +0.08 |
| raw / std: raw / std (what is left after container code) | 1.000 | 0.991–1.009 | -0.08 |

## ins (pooled, 202 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 54.93 |
| raw | 50.33 |
| raw.size_first | 48.99 |
| raw.ldr | 49.89 |
| raw.stdpos | 50.37 |
| std | 50.13 |
| std.xopos | 49.32 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.101 | 1.087–1.115 | +4.64 |
| xo / raw: container code: library vs raw (L10) | 1.094 | 1.079–1.109 | +4.39 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 1.026 | 1.017–1.035 | +1.30 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.004 | 0.992–1.015 | +0.33 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.002 | 0.992–1.011 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 0.985 | 0.981–0.988 | -0.81 |
| raw / std: raw / std (what is left after container code) | 1.006 | 0.999–1.013 | +0.16 |

## era (pooled, 202 rounds)

| variant | median ns/op |
| --- | ---: |
| xo | 44.27 |
| raw | 42.89 |
| raw.size_first | 42.72 |
| raw.ldr | 42.56 |
| raw.noscrub | 43.21 |
| raw.stdpos | 42.81 |
| std | 42.89 |
| std.xopos | 42.97 |

| pair | GM ratio | 95% interval | median diff ns/op |
| --- | ---: | --- | ---: |
| xo / std: xo / std (the G9 gap, as bench/reloc) | 1.033 | 1.029–1.037 | +1.22 |
| xo / raw: container code: library vs raw (L10) | 1.030 | 1.025–1.035 | +1.30 |
| raw / raw.size_first: forwarding: size_ after vs before memmove (L11) | 0.997 | 0.983–1.012 | +0.24 |
| raw / raw.ldr: head read: ldp vs two ldr | 1.009 | 1.004–1.013 | +0.32 |
| raw / raw.noscrub: M4 scrub of the vacated slot (report only) | 0.993 | 0.987–0.999 | -0.24 |
| raw.stdpos / raw: placement, raw code: std page offset vs arena block (L12) | 1.003 | 0.997–1.010 | +0.00 |
| std.xopos / std: placement, std code: XOffset page offset vs malloc (L12) | 1.007 | 1.000–1.014 | +0.08 |
| raw / std: raw / std (what is left after container code) | 1.003 | 0.998–1.008 | +0.00 |

