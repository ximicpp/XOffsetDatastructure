# G9R3 result (2026-11-04T22:32:16Z)

t0 fe8c07437224d9462f407cd115eae18a3972a829 (next_cpp26 fe8c0743 (#191 merged; base of PR #222)); t1 6578f4039c5fe746430c2f0c3889bca59cf77e25 (CKPT19b head (PR #222 6578f403)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 fe8c07437224d9462f407cd115eae18a3972a829 tree d4f4865f4f4c657f8dd768c95d25fcf983175471 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (next_cpp26 fe8c0743 (#191 merged; base of PR #222))
t1 6578f4039c5fe746430c2f0c3889bca59cf77e25 tree 33c241701572bb55163fd7d37e79e85cf7fb1123 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT19b head (PR #222 6578f403))
t1: diff over t0 sha256 6e3c2d04fa61b2a1d79acf411b748a6402355f2ec1e162442ba0c7b124945e28 ( 6 files changed, 262 insertions(+), 91 deletions(-))
t1: library header diff over t0:  1 file changed, 52 insertions(+), 27 deletions(-)
t1: first-parent commits over t0:
  6578f403 VM color does not raise the region alignment
  209fe35f test_owner_map: expect min(8, colors) VM colors on 16 KiB pages
  60288a20 Color payload starts so many buffers do not share TLB sets
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 5d3a5f4b2d326c7cbaa036b8c53d31a776b1a4fd674c8ecd20794add4813f531
reloc_bench t1 sha256 b62e19b7fb3817eb6de04ea09c5a8d2167777e683d02de54194f877fcc73adfe
reloc_bench t0a sha256 6fa9ff1605a6e8edffb26f903f8afb0b6da9714d55e303986704d466811888f6
reloc_bench t1a sha256 ec1a0cdcfbae123425a24c23572daea72318efd16f0d4af5b84290f1a9d2cb60
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.723 | 0.727 | 1.005 | 1.002–1.008 | 1.005, 1.005 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.757 | 0.997 | 0.993–1.001 | 1.001, 0.994 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.366 | 0.363 | 0.991 | 0.989–0.993 | 0.998, 0.984 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.941 | 1.003 | 1.000–1.005 | 1.005, 1.001 | worse |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.808 | 1.001 | 0.999–1.004 | 1.002, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.873 | 0.870 | 0.997 | 0.994–0.999 | 0.998, 0.996 |  |
| nested.build.ord | std | G9 | 0.329 | 0.328 | 0.995 | 0.991–0.998 | 0.992, 0.997 |  |
| reset | std | G9 | 0.223 | 0.205 | 0.920 | 0.904–0.937 | 1.087, 0.779 |  |
| set.erase.fresh | std::flat_set | G9 | 1.087 | 1.090 | 1.003 | 0.999–1.006 | 1.004, 1.002 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 0.999, 1.000 |  |
| set.insert.fresh | std::flat_set | G9 | 0.999 | 0.996 | 0.997 | 0.992–1.003 | 0.996, 0.999 |  |
| str.append | std::string | G9 | 0.426 | 0.418 | 0.981 | 0.976–0.987 | 0.982, 0.981 |  |
| str.assign_long | std::string | G9 | 0.802 | 0.866 | 1.080 | 1.078–1.083 | 1.080, 1.080 | worse |
| str.assign_short.view | std::string | G9 | 0.342 | 0.342 | 1.000 | 0.998–1.002 | 0.998, 1.002 |  |
| vec.iterate | std::vector | G9 | 0.982 | 1.002 | 1.020 | 1.017–1.022 | 1.019, 1.021 | worse |
| vec.mid_erase | std::vector | G9 | 1.022 | 1.018 | 0.996 | 0.995–0.997 | 0.997, 0.995 |  |
| vec.mid_insert | std::vector | G9 | 0.976 | 0.971 | 0.995 | 0.994–0.997 | 1.002, 0.989 |  |
| vec.push_back | std::vector | G9 | 1.138 | 1.111 | 0.976 | 0.973–0.979 | 0.974, 0.978 |  |
| vec.push_back_grow | std::vector | G9 | 0.766 | 0.776 | 1.013 | 1.010–1.016 | 1.012, 1.014 | worse |
| hero.mid_erase.plain | std::vector | report | 4.000 | 4.033 | 1.008 | 1.005–1.012 | 1.011, 1.005 | worse |
| hero.mid_insert.plain | std::vector | report | 4.206 | 4.222 | 1.004 | 0.999–1.008 | 1.004, 1.004 |  |
| hero.push_back_grow.plain | std::vector | report | 0.409 | 0.404 | 0.988 | 0.985–0.990 | 0.992, 0.984 |  |
| map.erase.fresh | std::map | report | 0.738 | 0.734 | 0.995 | 0.992–0.998 | 1.006, 0.984 |  |
| map.erase.reused | std::flat_map | report | 1.082 | 1.081 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| map.erase.reused | std::map | report | 1.635 | 1.623 | 0.993 | 0.986–0.999 | 1.018, 0.967 |  |
| map.find.fresh | std::map | report | 0.323 | 0.321 | 0.995 | 0.990–0.999 | 0.997, 0.992 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.808 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| map.find.reused | std::map | report | 0.718 | 0.702 | 0.978 | 0.973–0.982 | 0.938, 1.019 |  |
| map.insert.fresh | std::map | report | 0.748 | 0.743 | 0.993 | 0.991–0.996 | 0.995, 0.991 |  |
| map.insert.reused | std::flat_map | report | 1.004 | 1.001 | 0.997 | 0.994–1.000 | 1.002, 0.992 |  |
| map.insert.reused | std::map | report | 1.808 | 1.780 | 0.984 | 0.980–0.988 | 0.982, 0.986 |  |
| nested.build.plain | std | report | 0.342 | 0.338 | 0.987 | 0.984–0.990 | 0.981, 0.992 |  |
| set.erase.fresh | std::set | report | 0.666 | 0.663 | 0.996 | 0.993–0.999 | 1.011, 0.981 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.136 | 0.998 | 0.995–1.001 | 0.999, 0.997 |  |
| set.erase.reused | std::set | report | 1.344 | 1.334 | 0.992 | 0.985–0.999 | 1.034, 0.952 |  |
| set.find.fresh | std::set | report | 0.301 | 0.302 | 1.003 | 0.998–1.007 | 1.001, 1.004 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 0.999 | 0.997–1.002 | 1.000, 0.999 |  |
| set.find.reused | std::set | report | 0.657 | 0.667 | 1.015 | 1.012–1.018 | 1.006, 1.024 | worse |
| set.insert.fresh | std::set | report | 0.654 | 0.646 | 0.989 | 0.983–0.994 | 0.993, 0.985 |  |
| set.insert.reused | std::flat_set | report | 1.082 | 1.081 | 0.999 | 0.997–1.002 | 0.999, 1.000 |  |
| set.insert.reused | std::set | report | 1.451 | 1.412 | 0.973 | 0.965–0.982 | 0.963, 0.984 |  |
| str.assign_short | std::string | report | 1.772 | 1.768 | 0.998 | 0.996–0.999 | 0.997, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.151 | 1.001 | 0.998–1.003 | 1.001, 1.001 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.005; map.erase.fresh / std::flat_map (G9) 1.003; str.assign_long / std::string (G9) 1.080; vec.iterate / std::vector (G9) 1.020; vec.push_back_grow / std::vector (G9) 1.013; hero.mid_erase.plain / std::vector (report) 1.008; set.find.reused / std::set (report) 1.015

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.3 | 163.9 | 1.004 | 1.002–1.006 | 1.004, 1.004 | worse |
| hero.mid_erase.plain | 907.4 | 909.2 | 1.002 | 0.999–1.005 | 1.002, 1.002 |  |
| hero.mid_insert.ord | 228.5 | 227.1 | 0.994 | 0.992–0.996 | 0.993, 0.995 |  |
| hero.mid_insert.plain | 1265 | 1268 | 1.002 | 1.000–1.005 | 0.994, 1.011 |  |
| hero.push_back_grow.ord | 32.1 | 31.74 | 0.989 | 0.987–0.991 | 0.984, 0.993 |  |
| hero.push_back_grow.plain | 35.75 | 35.18 | 0.984 | 0.982–0.986 | 0.973, 0.995 |  |
| map.erase.fresh | 33.02 | 33.01 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |
| map.erase.reused | 29.61 | 29.67 | 1.002 | 1.000–1.004 | 1.001, 1.003 | worse |
| map.find.fresh | 5.29 | 5.294 | 1.001 | 0.999–1.002 | 1.002, 1.000 |  |
| map.find.reused | 5.307 | 5.305 | 1.000 | 0.998–1.002 | 1.000, 0.999 |  |
| map.insert.fresh | 32.19 | 32.2 | 1.000 | 0.999–1.002 | 1.000, 1.001 |  |
| map.insert.reused | 29.5 | 29.53 | 1.001 | 0.999–1.002 | 1.001, 1.000 |  |
| nested.build.ord | 273.4 | 272.6 | 0.997 | 0.995–1.000 | 0.996, 0.998 |  |
| nested.build.plain | 282 | 280.2 | 0.993 | 0.991–0.996 | 0.996, 0.991 |  |
| reset | 28.8 | 28.84 | 1.001 | 0.994–1.008 | 0.994, 1.008 |  |
| set.erase.fresh | 29.79 | 29.8 | 1.000 | 0.999–1.002 | 1.000, 1.000 |  |
| set.erase.reused | 23.21 | 23.17 | 0.999 | 0.996–1.001 | 0.999, 0.998 |  |
| set.find.fresh | 4.881 | 4.889 | 1.002 | 1.000–1.004 | 1.002, 1.001 |  |
| set.find.reused | 4.883 | 4.884 | 1.000 | 0.998–1.002 | 0.999, 1.001 |  |
| set.insert.fresh | 28.87 | 29.06 | 1.007 | 1.002–1.012 | 1.012, 1.001 | worse |
| set.insert.reused | 22.8 | 22.85 | 1.002 | 1.000–1.005 | 1.004, 1.001 |  |
| str.append | 1.586 | 1.567 | 0.988 | 0.983–0.993 | 0.985, 0.992 |  |
| str.assign_long | 2.748 | 2.955 | 1.075 | 1.072–1.079 | 1.071, 1.079 | worse |
| str.assign_short | 0.9513 | 0.9395 | 0.988 | 0.983–0.992 | 0.980, 0.995 |  |
| str.assign_short.copy | 0.63 | 0.6277 | 0.996 | 0.991–1.002 | 0.990, 1.003 |  |
| str.assign_short.view | 0.9519 | 0.9461 | 0.994 | 0.990–0.998 | 0.990, 0.998 |  |
| vec.iterate | 0.03253 | 0.03318 | 1.020 | 1.017–1.022 | 1.019, 1.021 | worse |
| vec.mid_erase | 40.28 | 40.13 | 0.996 | 0.995–0.998 | 1.001, 0.991 |  |
| vec.mid_insert | 40.61 | 40.52 | 0.998 | 0.996–0.999 | 0.997, 0.999 |  |
| vec.push_back | 0.2984 | 0.2922 | 0.979 | 0.967–0.991 | 0.980, 0.978 |  |
| vec.push_back_grow | 0.2753 | 0.2789 | 1.013 | 1.007–1.019 | 1.004, 1.022 | worse |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.004; map.erase.reused 1.002; set.insert.fresh 1.007; str.assign_long 1.075; vec.iterate 1.020; vec.push_back_grow 1.013

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.721 | 0.993 | 0.990–0.995 | 0.993, 0.992 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.763 | 0.761 | 0.997 | 0.994–1.001 | 1.001, 0.993 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.370 | 0.348 | 0.941 | 0.936–0.945 | 0.948, 0.934 |  |
| map.erase.fresh | std::flat_map | G9 | 0.944 | 0.946 | 1.001 | 1.000–1.003 | 1.003, 1.000 |  |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.809 | 1.001 | 0.998–1.004 | 0.999, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.865 | 0.996 | 0.994–0.999 | 1.001, 0.992 |  |
| nested.build.ord | std | G9 | 0.316 | 0.326 | 1.032 | 1.026–1.037 | 1.018, 1.046 | worse |
| reset | std | G9 | 0.239 | 0.248 | 1.036 | 1.026–1.046 | 0.989, 1.084 | worse? |
| set.erase.fresh | std::flat_set | G9 | 1.095 | 1.094 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 1.001, 0.999 |  |
| set.insert.fresh | std::flat_set | G9 | 1.003 | 1.003 | 0.999 | 0.998–1.001 | 1.002, 0.997 |  |
| str.append | std::string | G9 | 0.424 | 0.428 | 1.008 | 1.002–1.013 | 0.997, 1.018 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.866 | 1.000 | 0.997–1.002 | 0.999, 1.000 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.000 | 0.997–1.003 | 0.999, 1.001 |  |
| vec.iterate | std::vector | G9 | 1.001 | 1.002 | 1.001 | 0.998–1.003 | 1.002, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.026 | 1.005 | 1.003–1.007 | 1.000, 1.011 |  |
| vec.mid_insert | std::vector | G9 | 0.956 | 0.958 | 1.002 | 1.001–1.004 | 1.004, 1.000 | worse |
| vec.push_back | std::vector | G9 | 1.122 | 1.119 | 0.997 | 0.995–0.999 | 1.004, 0.990 |  |
| vec.push_back_grow | std::vector | G9 | 0.766 | 0.762 | 0.995 | 0.991–0.999 | 1.001, 0.989 |  |
| hero.mid_erase.plain | std::vector | report | 4.039 | 3.997 | 0.990 | 0.982–0.997 | 0.996, 0.984 |  |
| hero.mid_insert.plain | std::vector | report | 4.194 | 4.157 | 0.991 | 0.986–0.997 | 0.981, 1.001 |  |
| hero.push_back_grow.plain | std::vector | report | 0.410 | 0.391 | 0.955 | 0.950–0.960 | 0.959, 0.951 |  |
| map.erase.fresh | std::map | report | 0.744 | 0.744 | 0.999 | 0.996–1.002 | 1.013, 0.985 |  |
| map.erase.reused | std::flat_map | report | 1.087 | 1.080 | 0.993 | 0.990–0.996 | 0.998, 0.989 |  |
| map.erase.reused | std::map | report | 1.615 | 1.619 | 1.003 | 0.995–1.011 | 0.996, 1.010 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 0.999 | 0.995–1.004 | 1.000, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.808 | 0.810 | 1.003 | 1.000–1.005 | 1.002, 1.003 | worse |
| map.find.reused | std::map | report | 0.739 | 0.731 | 0.989 | 0.986–0.992 | 0.993, 0.985 |  |
| map.insert.fresh | std::map | report | 0.741 | 0.730 | 0.985 | 0.982–0.989 | 0.986, 0.984 |  |
| map.insert.reused | std::flat_map | report | 0.999 | 1.002 | 1.003 | 1.000–1.006 | 1.006, 1.000 | worse |
| map.insert.reused | std::map | report | 1.850 | 1.857 | 1.004 | 0.998–1.010 | 0.998, 1.010 |  |
| nested.build.plain | std | report | 0.325 | 0.340 | 1.047 | 1.041–1.053 | 0.999, 1.097 | worse? |
| set.erase.fresh | std::set | report | 0.674 | 0.673 | 0.998 | 0.996–1.001 | 1.001, 0.996 |  |
| set.erase.reused | std::flat_set | report | 1.136 | 1.137 | 1.001 | 0.998–1.004 | 1.001, 1.001 |  |
| set.erase.reused | std::set | report | 1.311 | 1.330 | 1.015 | 1.006–1.023 | 1.021, 1.009 | worse |
| set.find.fresh | std::set | report | 0.300 | 0.300 | 0.999 | 0.995–1.004 | 0.999, 1.000 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.756 | 1.000 | 0.998–1.002 | 1.000, 0.999 |  |
| set.find.reused | std::set | report | 0.685 | 0.687 | 1.003 | 1.000–1.006 | 0.996, 1.010 |  |
| set.insert.fresh | std::set | report | 0.653 | 0.649 | 0.993 | 0.990–0.996 | 0.999, 0.987 |  |
| set.insert.reused | std::flat_set | report | 1.085 | 1.082 | 0.997 | 0.996–0.999 | 1.000, 0.994 |  |
| set.insert.reused | std::set | report | 1.435 | 1.429 | 0.996 | 0.986–1.005 | 0.973, 1.019 |  |
| str.assign_short | std::string | report | 1.767 | 1.766 | 1.000 | 0.997–1.002 | 1.001, 0.998 |  |
| str.assign_short.copy | std::string | report | 1.149 | 1.150 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |

worse at t1a than t0a (by the rule): nested.build.ord / std (G9) 1.032; vec.mid_insert / std::vector (G9) 1.002; map.find.reused / std::flat_map (report) 1.003; map.insert.reused / std::flat_map (report) 1.003; set.erase.reused / std::set (report) 1.015

above 1.01 but not worse by the rule: reset / std (G9) 1.036; nested.build.plain / std (report) 1.047

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.6 | 164.3 | 0.998 | 0.997–0.999 | 0.998, 0.998 |  |
| hero.mid_erase.plain | 916.3 | 913.5 | 0.997 | 0.990–1.004 | 0.993, 1.001 |  |
| hero.mid_insert.ord | 229.3 | 229.6 | 1.001 | 1.000–1.003 | 1.003, 1.000 |  |
| hero.mid_insert.plain | 1262 | 1288 | 1.021 | 1.008–1.033 | 0.990, 1.052 | worse? |
| hero.push_back_grow.ord | 31.89 | 31.68 | 0.993 | 0.992–0.995 | 0.994, 0.992 |  |
| hero.push_back_grow.plain | 35.4 | 35.35 | 0.999 | 0.996–1.002 | 0.992, 1.005 |  |
| map.erase.fresh | 33.05 | 32.98 | 0.998 | 0.997–0.999 | 1.000, 0.997 |  |
| map.erase.reused | 29.78 | 29.68 | 0.997 | 0.994–0.999 | 1.003, 0.990 |  |
| map.find.fresh | 5.29 | 5.298 | 1.002 | 0.999–1.004 | 0.999, 1.004 |  |
| map.find.reused | 5.305 | 5.312 | 1.001 | 0.999–1.003 | 1.002, 1.001 |  |
| map.insert.fresh | 32.34 | 32.27 | 0.998 | 0.996–0.999 | 1.000, 0.996 |  |
| map.insert.reused | 29.59 | 29.55 | 0.998 | 0.996–1.001 | 1.003, 0.994 |  |
| nested.build.ord | 268.3 | 270.1 | 1.007 | 1.004–1.009 | 1.010, 1.003 | worse |
| nested.build.plain | 278.6 | 279.9 | 1.004 | 1.002–1.007 | 0.995, 1.014 |  |
| reset | 29.42 | 29.37 | 0.998 | 0.993–1.004 | 0.998, 0.999 |  |
| set.erase.fresh | 29.82 | 29.78 | 0.999 | 0.998–1.000 | 0.999, 0.998 |  |
| set.erase.reused | 23.22 | 23.21 | 0.999 | 0.996–1.003 | 1.000, 0.999 |  |
| set.find.fresh | 4.882 | 4.882 | 1.000 | 0.999–1.001 | 1.001, 0.999 |  |
| set.find.reused | 4.881 | 4.883 | 1.001 | 0.999–1.002 | 1.002, 0.999 |  |
| set.insert.fresh | 28.94 | 28.91 | 0.999 | 0.998–1.000 | 1.001, 0.997 |  |
| set.insert.reused | 22.82 | 22.79 | 0.999 | 0.998–1.000 | 1.003, 0.994 |  |
| str.append | 1.574 | 1.57 | 0.998 | 0.993–1.002 | 0.995, 1.001 |  |
| str.assign_long | 2.965 | 2.955 | 0.996 | 0.994–0.999 | 1.001, 0.992 |  |
| str.assign_short | 0.9572 | 0.9434 | 0.986 | 0.981–0.990 | 0.998, 0.973 |  |
| str.assign_short.copy | 0.6325 | 0.6271 | 0.991 | 0.986–0.996 | 1.006, 0.977 |  |
| str.assign_short.view | 0.9557 | 0.9522 | 0.996 | 0.992–1.001 | 1.008, 0.985 |  |
| vec.iterate | 0.03323 | 0.03328 | 1.002 | 0.999–1.004 | 1.003, 1.001 |  |
| vec.mid_erase | 40.39 | 40.69 | 1.007 | 1.005–1.009 | 1.004, 1.011 | worse |
| vec.mid_insert | 40.47 | 40.54 | 1.002 | 1.001–1.003 | 1.003, 1.000 | worse |
| vec.push_back | 0.2717 | 0.2722 | 1.002 | 1.001–1.003 | 1.000, 1.004 | worse |
| vec.push_back_grow | 0.2717 | 0.2716 | 1.000 | 0.997–1.002 | 1.003, 0.996 |  |

XOffset slower at t1a than t0a (worse by the rule): nested.build.ord 1.007; vec.mid_erase 1.007; vec.mid_insert 1.002; vec.push_back 1.002

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.004 | 0.998 | 0.997–0.999 | not worse |
| map.erase.reused (XOffset ns/op) | 1.002 | 0.997 | 0.994–0.999 | not worse |
| set.insert.fresh (XOffset ns/op) | 1.007 | 0.999 | 0.998–1.000 | not worse |
| str.assign_long (XOffset ns/op) | 1.075 | 0.996 | 0.994–0.999 | not worse |
| vec.iterate (XOffset ns/op) | 1.020 | 1.002 | 0.999–1.004 | not worse |
| vec.push_back_grow (XOffset ns/op) | 1.013 | 1.000 | 0.997–1.002 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.005 | 0.993 | 0.990–0.995 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.008 | 0.990 | 0.982–0.997 | not worse |
| map.erase.fresh / std::flat_map (G9) | 1.003 | 1.001 | 1.000–1.003 | not worse |
| set.find.reused / std::set (report) | 1.015 | 1.003 | 1.000–1.006 | not worse |
| str.assign_long / std::string (G9) | 1.080 | 1.000 | 0.997–1.002 | not worse |
| vec.iterate / std::vector (G9) | 1.020 | 1.001 | 0.998–1.003 | not worse |
| vec.push_back_grow / std::vector (G9) | 1.013 | 0.995 | 0.991–0.999 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.08 | 163.66 | 1.004 | 224.76 | 0.999 |
| hero.mid_erase.plain | 902.70 | 905.35 | 1.003 | 225.45 | 0.990 |
| hero.mid_insert.ord | 227.78 | 226.73 | 0.995 | 298.65 | 1.002 |
| hero.mid_insert.plain | 1257.57 | 1262.05 | 1.004 | 301.76 | 0.992 |
| hero.push_back_grow.ord | 31.97 | 31.66 | 0.990 | 87.40 | 0.993 |
| hero.push_back_grow.plain | 35.65 | 35.06 | 0.983 | 87.18 | 0.998 |
| map.erase.fresh | 32.97 | 32.99 | 1.001 | 35.12 | 0.998 |
| map.erase.reused | 29.51 | 29.59 | 1.003 | 27.27 | 0.997 |
| map.find.fresh | 5.29 | 5.29 | 1.000 | 6.55 | 0.999 |
| map.find.reused | 5.30 | 5.29 | 1.000 | 6.56 | 1.002 |
| map.insert.fresh | 32.18 | 32.19 | 1.000 | 36.80 | 0.998 |
| map.insert.reused | 29.47 | 29.47 | 1.000 | 29.31 | 0.997 |
| nested.build.ord | 273.25 | 272.37 | 0.997 | 829.80 | 1.008 |
| nested.build.plain | 281.80 | 279.45 | 0.992 | 822.39 | 1.004 |
| reset | 28.86 | 28.78 | 0.997 | 128.45 | 0.996 |
| set.erase.fresh | 29.76 | 29.76 | 1.000 | 27.35 | 0.999 |
| set.erase.reused | 22.98 | 22.96 | 0.999 | 20.34 | 0.997 |
| set.find.fresh | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.find.reused | 4.87 | 4.87 | 1.000 | 6.45 | 1.000 |
| set.insert.fresh | 28.85 | 28.84 | 1.000 | 28.87 | 0.999 |
| set.insert.reused | 22.78 | 22.80 | 1.001 | 21.06 | 1.000 |
| str.append | 1.58 | 1.55 | 0.982 | 3.69 | 1.004 |
| str.assign_long | 2.71 | 2.93 | 1.082 | 3.39 | 1.000 |
| str.assign_short | 0.95 | 0.92 | 0.972 | 0.54 | 0.977 |
| str.assign_short.copy | 0.62 | 0.62 | 1.000 | 0.54 | 0.976 |
| str.assign_short.view | 0.94 | 0.93 | 0.990 | 2.73 | 0.991 |
| vec.iterate | 0.03 | 0.03 | 1.014 | 0.03 | 1.003 |
| vec.mid_erase | 40.21 | 39.99 | 0.995 | 39.37 | 1.007 |
| vec.mid_insert | 40.54 | 40.49 | 0.999 | 41.59 | 1.002 |
| vec.push_back | 0.30 | 0.29 | 0.981 | 0.26 | 1.001 |
| vec.push_back_grow | 0.28 | 0.28 | 1.011 | 0.36 | 0.998 |

## analyze t0 (G9 at the anchor fe8c0743)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.138 |  1.142 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.766 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.982 |  0.983 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.34 |  0.976 |  0.978 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.39 |  1.022 |  1.023 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     224.93 |  0.723 |  0.726 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     298.17 |  0.760 |  0.763 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.98 |      87.52 |  0.366 |  0.367 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    902.34 |     225.27 |  4.000 |  4.014 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.17 |     305.01 |  4.206 |  4.224 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.68 |      87.52 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.93 |       0.52 |  1.772 |  1.776 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.802 |  0.804 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.72 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.62 |       0.53 |  1.150 |  1.152 | report  |
| str.append                   | std::string    | G9     |  808 |      1.59 |       3.69 |  0.426 |  0.428 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.15 |      36.78 |  0.873 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.15 |      42.97 |  0.748 |  0.750 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.809 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.43 |  0.323 |  0.324 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.938 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.43 |  0.738 |  0.740 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.999 |  1.001 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.11 |  0.654 |  0.655 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.26 |  0.301 |  0.302 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.087 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.43 |  0.666 |  0.668 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.004 |  1.007 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.28 |  1.808 |  1.813 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.56 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.50 |  0.718 |  0.721 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.082 |  1.084 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.635 |  1.646 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.082 |  1.085 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.46 |  1.451 |  1.463 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.41 |  0.657 |  0.659 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.138 |  1.141 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.68 |  1.344 |  1.354 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.16 |     828.12 |  0.329 |  0.330 | pass    |
| reset                        | std            | G9     |  808 |     28.66 |     118.50 |  0.223 |  0.226 | pass    |
| nested.build.plain           | std            | report |  808 |    281.25 |     822.91 |  0.342 |  0.343 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.111 |  1.114 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.776 |  0.779 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      41.83 |  0.971 |  0.972 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.39 |  1.018 |  1.019 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.27 |  0.727 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.757 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.62 |      87.56 |  0.363 |  0.364 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    906.25 |     224.61 |  4.033 |  4.046 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1263.03 |     304.69 |  4.222 |  4.242 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.10 |      87.00 |  0.404 |  0.405 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.868 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.61 |       0.52 |  1.151 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.72 |  0.418 |  0.420 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.95 |  0.870 |  0.872 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.21 |  0.743 |  0.745 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.29 |       6.55 |  0.808 |  0.810 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.29 |      16.54 |  0.321 |  0.322 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      34.99 |  0.941 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.60 |  0.734 |  0.736 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.97 |  0.996 |  1.004 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.43 |  0.646 |  0.652 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.24 |  0.302 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.26 |  1.090 |  1.095 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.60 |  0.663 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.001 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.28 |  1.780 |  1.789 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.811 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       7.69 |  0.702 |  0.706 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.081 |  1.083 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.74 |  1.623 |  1.634 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.081 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.62 |  1.412 |  1.425 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.28 |  0.667 |  0.670 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.95 |      20.35 |  1.136 |  1.139 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.95 |      17.09 |  1.334 |  1.343 | report  |
| nested.build.ord             | std            | G9     |  808 |    272.12 |     829.44 |  0.328 |  0.329 | pass    |
| reset                        | std            | G9     |  808 |     28.64 |     157.88 |  0.205 |  0.209 | pass    |
| nested.build.plain           | std            | report |  808 |    279.94 |     826.81 |  0.338 |  0.339 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T22:22:59Z block 1 start: waited 180s load 1.17 WindowServer 1.2% remote 4
2026-11-04T22:26:03Z block 2 start: waited 180s load 1.50 WindowServer 1.1% remote 4
2026-11-04T22:29:08Z block 3 start: waited 180s load 1.23 WindowServer 1% remote 4
2026-11-04T22:32:12Z block 4 start: waited 180s load 1.26 WindowServer 1.3% remote 4
```
