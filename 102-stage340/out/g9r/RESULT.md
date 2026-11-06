# G9R3 result (2026-11-06T01:55:45Z)

t0 ee506d3f54a979ededdd71cdc5cca5857ff38f11 (v3.3.1 ee506d3f (previous release tag)); t1 1162e37bf40fad6074b9e4cc9c7ff109a8b35288 (STAGE340 head (v3.4.0 1162e37b)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 ee506d3f54a979ededdd71cdc5cca5857ff38f11 tree becbab81e91384527bc313392fb407f61f690d36 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.3.1 ee506d3f (previous release tag))
t1 1162e37bf40fad6074b9e4cc9c7ff109a8b35288 tree 28785dd4ff283d8aa549739172844f43c60366a9 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (STAGE340 head (v3.4.0 1162e37b))
t1: diff over t0 sha256 346c56ba34e7def10ae087becdab8151c95c8e0494faebabc97515f7dc207514 ( 59 files changed, 3666 insertions(+), 486 deletions(-))
t1: library header diff over t0:  2 files changed, 296 insertions(+), 146 deletions(-)
t1: first-parent commits over t0:
  1162e37b Merge pull request #245 from ximicpp/claude/release-3.4.0
  e71ee31f Merge pull request #244 from ximicpp/claude/delta-trust-p2-20261105
  8003a5d5 Merge pull request #243 from ximicpp/claude/up-send-small-msg-20261105 (class S): UP.send small-message fixed cost
  fd0dc58c Merge pull request #241 from ximicpp/claude/mem-pressure-20261105 (class L): VM host header in the payload's first page
  d10ede94 Merge pull request #242 from ximicpp/claude/shared-delta-d3-20261104 (class L): D3 shared deltas, XS 0.1 (experimental)
  56aa849e Merge pull request #240 from ximicpp/claude/revert-223-20261105 (class S): revert #223 root<T>() cache refill
  4d1ac732 Merge pull request #237 from ximicpp/claude/sorted-insert-split-20261105 (class S): sorted inserts, random back below 3.2.1
  59899aec Merge pull request #239 from ximicpp/claude/anchor-3.3.1 (class 0): anchor at v3.3.1
  93124535 Merge pull request #236 from ximicpp/claude/slg-trust-keys-20261105
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 70ef4974d92872cec20fcdc0207e6af7aca7ed4abc17338d8e001729a41b3762
reloc_bench t1 sha256 f71169a7897c9ff17ecbdb4b0ee74b1cc73680b876442304409f4ff6e9a8dd4d
reloc_bench t0a sha256 d8e183035a70417d0de3f5b4430ad52d986d54b8761d9eff018bee33f8ca2159
reloc_bench t1a sha256 2aea6f1c2364924b6b81f369b8ee6b909918013f00eaa1b8fc4df45ba7576a7a
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.725 | 0.997 | 0.995–0.999 | 0.998, 0.996 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.761 | 0.757 | 0.995 | 0.992–0.999 | 0.993, 0.998 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.370 | 0.368 | 0.996 | 0.994–0.998 | 0.988, 1.005 |  |
| map.erase.fresh | std::flat_map | G9 | 0.941 | 0.941 | 1.000 | 0.997–1.002 | 0.999, 1.000 |  |
| map.find.fresh | std::flat_map | G9 | 0.807 | 0.807 | 1.000 | 0.998–1.002 | 1.000, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.861 | 0.859 | 0.998 | 0.996–1.000 | 1.000, 0.996 |  |
| nested.build.ord | std | G9 | 0.325 | 0.322 | 0.991 | 0.988–0.994 | 0.995, 0.987 |  |
| reset | std | G9 | 0.225 | 0.189 | 0.839 | 0.826–0.853 | 0.859, 0.820 |  |
| set.erase.fresh | std::flat_set | G9 | 1.084 | 1.086 | 1.001 | 0.999–1.004 | 1.001, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.757 | 0.756 | 0.999 | 0.997–1.001 | 0.998, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 0.997 | 0.985 | 0.988 | 0.987–0.990 | 0.988, 0.989 |  |
| str.append | std::string | G9 | 0.431 | 0.433 | 1.006 | 1.001–1.010 | 0.992, 1.020 |  |
| str.assign_long | std::string | G9 | 0.865 | 0.800 | 0.924 | 0.923–0.926 | 0.924, 0.925 |  |
| str.assign_short.view | std::string | G9 | 0.343 | 0.343 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| vec.iterate | std::vector | G9 | 1.002 | 1.001 | 0.998 | 0.995–1.001 | 0.997, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.019 | 1.011 | 0.991 | 0.990–0.993 | 0.989, 0.994 |  |
| vec.mid_insert | std::vector | G9 | 0.964 | 0.957 | 0.993 | 0.991–0.995 | 0.992, 0.994 |  |
| vec.push_back | std::vector | G9 | 1.120 | 1.106 | 0.987 | 0.985–0.990 | 0.990, 0.984 |  |
| vec.push_back_grow | std::vector | G9 | 0.765 | 0.766 | 1.001 | 0.998–1.004 | 1.014, 0.988 |  |
| hero.mid_erase.plain | std::vector | report | 4.034 | 4.049 | 1.004 | 1.001–1.007 | 1.003, 1.004 | worse |
| hero.mid_insert.plain | std::vector | report | 4.223 | 4.203 | 0.995 | 0.991–0.999 | 0.996, 0.994 |  |
| hero.push_back_grow.plain | std::vector | report | 0.410 | 0.408 | 0.994 | 0.992–0.996 | 0.985, 1.003 |  |
| map.erase.fresh | std::map | report | 0.744 | 0.734 | 0.986 | 0.983–0.990 | 0.990, 0.983 |  |
| map.erase.reused | std::flat_map | report | 1.073 | 1.081 | 1.008 | 1.005–1.011 | 1.002, 1.013 | worse |
| map.erase.reused | std::map | report | 1.674 | 1.607 | 0.960 | 0.955–0.966 | 0.966, 0.954 |  |
| map.find.fresh | std::map | report | 0.324 | 0.325 | 1.004 | 1.000–1.008 | 1.002, 1.006 | worse |
| map.find.reused | std::flat_map | report | 0.809 | 0.808 | 0.999 | 0.997–1.002 | 0.999, 0.999 |  |
| map.find.reused | std::map | report | 0.726 | 0.730 | 1.005 | 1.001–1.010 | 1.005, 1.006 | worse |
| map.insert.fresh | std::map | report | 0.750 | 0.739 | 0.985 | 0.982–0.988 | 0.981, 0.989 |  |
| map.insert.reused | std::flat_map | report | 1.027 | 1.016 | 0.990 | 0.988–0.992 | 0.995, 0.985 |  |
| map.insert.reused | std::map | report | 1.850 | 1.834 | 0.992 | 0.986–0.998 | 0.972, 1.012 |  |
| nested.build.plain | std | report | 0.336 | 0.334 | 0.992 | 0.989–0.995 | 1.001, 0.984 |  |
| set.erase.fresh | std::set | report | 0.678 | 0.664 | 0.979 | 0.976–0.983 | 0.986, 0.973 |  |
| set.erase.reused | std::flat_set | report | 1.125 | 1.128 | 1.003 | 1.000–1.006 | 1.000, 1.006 |  |
| set.erase.reused | std::set | report | 1.356 | 1.318 | 0.972 | 0.966–0.978 | 0.988, 0.956 |  |
| set.find.fresh | std::set | report | 0.301 | 0.294 | 0.975 | 0.970–0.980 | 0.980, 0.970 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.000 | 0.998–1.002 | 1.001, 0.998 |  |
| set.find.reused | std::set | report | 0.669 | 0.691 | 1.032 | 1.027–1.037 | 1.052, 1.013 | worse |
| set.insert.fresh | std::set | report | 0.653 | 0.649 | 0.994 | 0.992–0.996 | 0.991, 0.996 |  |
| set.insert.reused | std::flat_set | report | 1.112 | 1.112 | 1.001 | 0.999–1.002 | 1.002, 1.000 |  |
| set.insert.reused | std::set | report | 1.446 | 1.467 | 1.014 | 1.005–1.024 | 1.016, 1.013 | worse |
| str.assign_short | std::string | report | 1.764 | 1.765 | 1.000 | 0.999–1.002 | 0.997, 1.003 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.147 | 0.996 | 0.993–0.999 | 0.998, 0.995 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.plain / std::vector (report) 1.004; map.erase.reused / std::flat_map (report) 1.008; map.find.fresh / std::map (report) 1.004; map.find.reused / std::map (report) 1.005; set.find.reused / std::set (report) 1.032; set.insert.reused / std::set (report) 1.014

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.1 | 164 | 1.006 | 1.004–1.007 | 1.006, 1.005 | worse |
| hero.mid_erase.plain | 904.9 | 907.6 | 1.003 | 1.001–1.005 | 1.003, 1.003 | worse |
| hero.mid_insert.ord | 227.9 | 226.9 | 0.996 | 0.994–0.997 | 0.995, 0.997 |  |
| hero.mid_insert.plain | 1257 | 1252 | 0.996 | 0.995–0.998 | 0.995, 0.997 |  |
| hero.push_back_grow.ord | 31.92 | 31.86 | 0.998 | 0.997–1.000 | 0.997, 0.999 |  |
| hero.push_back_grow.plain | 35.4 | 35.1 | 0.991 | 0.990–0.993 | 0.990, 0.993 |  |
| map.erase.fresh | 32.91 | 33.05 | 1.004 | 1.002–1.006 | 1.003, 1.006 | worse |
| map.erase.reused | 29.47 | 29.66 | 1.006 | 1.005–1.008 | 1.003, 1.009 | worse |
| map.find.fresh | 5.283 | 5.282 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| map.find.reused | 5.306 | 5.301 | 0.999 | 0.997–1.001 | 1.000, 0.998 |  |
| map.insert.fresh | 31.9 | 31.71 | 0.994 | 0.993–0.995 | 0.993, 0.995 |  |
| map.insert.reused | 30.28 | 29.89 | 0.987 | 0.986–0.989 | 0.988, 0.987 |  |
| nested.build.ord | 271.1 | 266 | 0.981 | 0.979–0.983 | 0.981, 0.981 |  |
| nested.build.plain | 279.1 | 273 | 0.978 | 0.976–0.980 | 0.983, 0.974 |  |
| reset | 28.15 | 25.56 | 0.908 | 0.905–0.912 | 0.912, 0.905 |  |
| set.erase.fresh | 29.81 | 29.81 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| set.erase.reused | 23.02 | 23.08 | 1.002 | 1.000–1.005 | 1.001, 1.004 | worse |
| set.find.fresh | 4.886 | 4.88 | 0.999 | 0.997–1.000 | 0.997, 1.000 |  |
| set.find.reused | 4.881 | 4.878 | 0.999 | 0.998–1.001 | 1.000, 0.999 |  |
| set.insert.fresh | 28.78 | 28.48 | 0.990 | 0.989–0.991 | 0.989, 0.991 |  |
| set.insert.reused | 23.43 | 23.5 | 1.003 | 1.002–1.004 | 1.002, 1.004 | worse |
| str.append | 1.559 | 1.529 | 0.980 | 0.976–0.984 | 0.964, 0.997 |  |
| str.assign_long | 2.937 | 2.712 | 0.924 | 0.923–0.925 | 0.923, 0.925 |  |
| str.assign_short | 0.9279 | 0.924 | 0.996 | 0.993–0.999 | 0.989, 1.003 |  |
| str.assign_short.copy | 0.6209 | 0.6122 | 0.986 | 0.981–0.991 | 0.979, 0.992 |  |
| str.assign_short.view | 0.9362 | 0.9336 | 0.997 | 0.995–1.000 | 0.992, 1.002 |  |
| vec.iterate | 0.03319 | 0.0332 | 1.000 | 0.998–1.003 | 0.999, 1.001 |  |
| vec.mid_erase | 40.26 | 40.12 | 0.997 | 0.995–0.998 | 0.997, 0.996 |  |
| vec.mid_insert | 40.63 | 40.31 | 0.992 | 0.991–0.993 | 0.991, 0.993 |  |
| vec.push_back | 0.2928 | 0.293 | 1.001 | 0.988–1.014 | 1.007, 0.995 |  |
| vec.push_back_grow | 0.2758 | 0.2759 | 1.000 | 0.994–1.006 | 1.003, 0.997 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.ord 1.006; hero.mid_erase.plain 1.003; map.erase.fresh 1.004; map.erase.reused 1.006; set.erase.reused 1.002; set.insert.reused 1.003

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.724 | 0.720 | 0.995 | 0.992–0.998 | 1.003, 0.987 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.760 | 0.756 | 0.995 | 0.991–0.999 | 1.001, 0.988 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.368 | 0.349 | 0.948 | 0.943–0.953 | 0.987, 0.910 |  |
| map.erase.fresh | std::flat_map | G9 | 0.941 | 0.944 | 1.004 | 1.002–1.006 | 1.005, 1.003 | worse |
| map.find.fresh | std::flat_map | G9 | 0.809 | 0.809 | 1.000 | 0.998–1.002 | 1.001, 1.000 |  |
| map.insert.fresh | std::flat_map | G9 | 0.864 | 0.851 | 0.985 | 0.983–0.988 | 0.990, 0.980 |  |
| nested.build.ord | std | G9 | 0.321 | 0.313 | 0.976 | 0.971–0.982 | 1.024, 0.931 |  |
| reset | std | G9 | 0.217 | 0.207 | 0.956 | 0.943–0.969 | 1.107, 0.826 |  |
| set.erase.fresh | std::flat_set | G9 | 1.090 | 1.089 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.756 | 1.001 | 0.998–1.005 | 1.001, 1.002 |  |
| set.insert.fresh | std::flat_set | G9 | 0.995 | 0.987 | 0.992 | 0.989–0.995 | 0.988, 0.996 |  |
| str.append | std::string | G9 | 0.429 | 0.432 | 1.008 | 1.003–1.014 | 1.004, 1.013 | worse |
| str.assign_long | std::string | G9 | 0.866 | 0.866 | 1.000 | 0.998–1.002 | 1.000, 0.999 |  |
| str.assign_short.view | std::string | G9 | 0.341 | 0.343 | 1.006 | 1.003–1.008 | 1.011, 1.001 | worse |
| vec.iterate | std::vector | G9 | 1.002 | 1.001 | 1.000 | 0.996–1.003 | 0.999, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.018 | 1.020 | 1.001 | 1.000–1.003 | 1.003, 1.000 |  |
| vec.mid_insert | std::vector | G9 | 0.957 | 0.959 | 1.002 | 0.999–1.005 | 0.997, 1.006 |  |
| vec.push_back | std::vector | G9 | 1.119 | 1.112 | 0.993 | 0.989–0.997 | 0.997, 0.989 |  |
| vec.push_back_grow | std::vector | G9 | 0.772 | 0.752 | 0.974 | 0.970–0.977 | 0.973, 0.974 |  |
| hero.mid_erase.plain | std::vector | report | 4.033 | 4.011 | 0.995 | 0.992–0.997 | 0.997, 0.992 |  |
| hero.mid_insert.plain | std::vector | report | 4.208 | 4.143 | 0.985 | 0.981–0.988 | 0.991, 0.979 |  |
| hero.push_back_grow.plain | std::vector | report | 0.408 | 0.387 | 0.949 | 0.944–0.954 | 0.987, 0.913 |  |
| map.erase.fresh | std::map | report | 0.733 | 0.733 | 1.000 | 0.997–1.003 | 1.006, 0.994 |  |
| map.erase.reused | std::flat_map | report | 1.080 | 1.076 | 0.996 | 0.993–0.999 | 0.993, 1.000 |  |
| map.erase.reused | std::map | report | 1.610 | 1.591 | 0.989 | 0.981–0.997 | 1.003, 0.975 |  |
| map.find.fresh | std::map | report | 0.324 | 0.324 | 0.999 | 0.995–1.003 | 0.999, 0.999 |  |
| map.find.reused | std::flat_map | report | 0.809 | 0.810 | 1.001 | 0.997–1.004 | 1.002, 0.999 |  |
| map.find.reused | std::map | report | 0.741 | 0.724 | 0.977 | 0.971–0.982 | 0.958, 0.995 |  |
| map.insert.fresh | std::map | report | 0.723 | 0.731 | 1.012 | 1.008–1.015 | 1.009, 1.014 | worse |
| map.insert.reused | std::flat_map | report | 1.028 | 1.019 | 0.992 | 0.989–0.995 | 0.992, 0.992 |  |
| map.insert.reused | std::map | report | 1.898 | 1.857 | 0.978 | 0.974–0.982 | 0.990, 0.966 |  |
| nested.build.plain | std | report | 0.333 | 0.322 | 0.968 | 0.962–0.974 | 1.017, 0.922 |  |
| set.erase.fresh | std::set | report | 0.666 | 0.663 | 0.996 | 0.992–0.999 | 1.005, 0.987 |  |
| set.erase.reused | std::flat_set | report | 1.135 | 1.129 | 0.994 | 0.990–0.999 | 0.983, 1.005 |  |
| set.erase.reused | std::set | report | 1.304 | 1.327 | 1.018 | 1.009–1.026 | 1.045, 0.991 | worse? |
| set.find.fresh | std::set | report | 0.300 | 0.299 | 0.999 | 0.994–1.005 | 1.002, 0.996 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.757 | 1.001 | 0.998–1.003 | 1.002, 0.999 |  |
| set.find.reused | std::set | report | 0.674 | 0.680 | 1.009 | 1.004–1.014 | 1.002, 1.016 | worse |
| set.insert.fresh | std::set | report | 0.652 | 0.643 | 0.987 | 0.984–0.990 | 0.990, 0.984 |  |
| set.insert.reused | std::flat_set | report | 1.108 | 1.113 | 1.004 | 1.002–1.007 | 1.006, 1.003 | worse |
| set.insert.reused | std::set | report | 1.477 | 1.486 | 1.007 | 0.998–1.015 | 1.045, 0.970 |  |
| str.assign_short | std::string | report | 1.763 | 1.767 | 1.002 | 1.000–1.004 | 1.001, 1.003 |  |
| str.assign_short.copy | std::string | report | 1.150 | 1.149 | 0.999 | 0.997–1.001 | 1.002, 0.996 |  |

worse at t1a than t0a (by the rule): map.erase.fresh / std::flat_map (G9) 1.004; str.append / std::string (G9) 1.008; str.assign_short.view / std::string (G9) 1.006; map.insert.fresh / std::map (report) 1.012; set.find.reused / std::set (report) 1.009; set.insert.reused / std::flat_set (report) 1.004

above 1.01 but not worse by the rule: set.erase.reused / std::set (report) 1.018

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.5 | 163.6 | 0.994 | 0.993–0.996 | 0.998, 0.991 |  |
| hero.mid_erase.plain | 907.2 | 902.2 | 0.994 | 0.992–0.997 | 0.994, 0.995 |  |
| hero.mid_insert.ord | 228.3 | 228.4 | 1.000 | 0.998–1.002 | 0.997, 1.004 |  |
| hero.mid_insert.plain | 1253 | 1239 | 0.989 | 0.988–0.991 | 0.988, 0.990 |  |
| hero.push_back_grow.ord | 31.86 | 31.54 | 0.990 | 0.988–0.992 | 0.994, 0.986 |  |
| hero.push_back_grow.plain | 35.33 | 35.02 | 0.991 | 0.990–0.993 | 0.988, 0.995 |  |
| map.erase.fresh | 33.03 | 32.98 | 0.998 | 0.997–1.000 | 0.999, 0.998 |  |
| map.erase.reused | 29.6 | 29.68 | 1.003 | 1.001–1.004 | 1.004, 1.001 | worse |
| map.find.fresh | 5.29 | 5.295 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| map.find.reused | 5.317 | 5.334 | 1.003 | 1.000–1.007 | 1.010, 0.996 |  |
| map.insert.fresh | 31.94 | 31.7 | 0.993 | 0.990–0.995 | 0.994, 0.991 |  |
| map.insert.reused | 30.32 | 30.01 | 0.990 | 0.988–0.991 | 0.990, 0.990 |  |
| nested.build.ord | 270.5 | 264.2 | 0.977 | 0.975–0.979 | 0.979, 0.975 |  |
| nested.build.plain | 279.8 | 271 | 0.968 | 0.966–0.971 | 0.968, 0.968 |  |
| reset | 29.49 | 25.52 | 0.866 | 0.862–0.869 | 0.899, 0.834 |  |
| set.erase.fresh | 29.76 | 29.75 | 1.000 | 0.998–1.001 | 0.999, 1.001 |  |
| set.erase.reused | 23.24 | 23.07 | 0.992 | 0.989–0.996 | 0.985, 1.000 |  |
| set.find.fresh | 4.908 | 4.881 | 0.995 | 0.991–0.998 | 1.000, 0.989 |  |
| set.find.reused | 4.881 | 4.885 | 1.001 | 0.999–1.003 | 1.002, 1.000 |  |
| set.insert.fresh | 28.79 | 28.59 | 0.993 | 0.991–0.995 | 0.991, 0.995 |  |
| set.insert.reused | 23.41 | 23.5 | 1.004 | 1.002–1.005 | 1.005, 1.002 | worse |
| str.append | 1.533 | 1.554 | 1.014 | 1.009–1.018 | 1.008, 1.020 | worse |
| str.assign_long | 2.942 | 2.938 | 0.999 | 0.997–1.000 | 0.999, 0.998 |  |
| str.assign_short | 0.9311 | 0.9243 | 0.993 | 0.990–0.995 | 0.986, 0.999 |  |
| str.assign_short.copy | 0.6227 | 0.6168 | 0.990 | 0.986–0.995 | 0.984, 0.996 |  |
| str.assign_short.view | 0.9376 | 0.9324 | 0.994 | 0.992–0.997 | 0.989, 1.000 |  |
| vec.iterate | 0.03327 | 0.03328 | 1.000 | 0.998–1.003 | 0.999, 1.002 |  |
| vec.mid_erase | 40.44 | 40.52 | 1.002 | 1.001–1.003 | 1.002, 1.001 | worse |
| vec.mid_insert | 40.49 | 40.78 | 1.007 | 1.005–1.009 | 1.010, 1.005 | worse |
| vec.push_back | 0.2719 | 0.272 | 1.000 | 0.998–1.003 | 1.000, 1.001 |  |
| vec.push_back_grow | 0.2719 | 0.2683 | 0.987 | 0.985–0.989 | 0.983, 0.991 |  |

XOffset slower at t1a than t0a (worse by the rule): map.erase.reused 1.003; set.insert.reused 1.004; str.append 1.014; vec.mid_erase 1.002; vec.mid_insert 1.007

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.ord (XOffset ns/op) | 1.006 | 0.994 | 0.993–0.996 | not worse |
| hero.mid_erase.plain (XOffset ns/op) | 1.003 | 0.994 | 0.992–0.997 | not worse |
| map.erase.fresh (XOffset ns/op) | 1.004 | 0.998 | 0.997–1.000 | not worse |
| map.erase.reused (XOffset ns/op) | 1.006 | 1.003 | 1.001–1.004 | still worse |
| set.erase.reused (XOffset ns/op) | 1.002 | 0.992 | 0.989–0.996 | not worse |
| set.insert.reused (XOffset ns/op) | 1.003 | 1.004 | 1.002–1.005 | still worse |
| hero.mid_erase.plain / std::vector (report) | 1.004 | 0.995 | 0.992–0.997 | not worse |
| map.erase.reused / std::flat_map (report) | 1.008 | 0.996 | 0.993–0.999 | not worse |
| map.find.fresh / std::map (report) | 1.004 | 0.999 | 0.995–1.003 | not worse |
| map.find.reused / std::map (report) | 1.005 | 0.977 | 0.971–0.982 | not worse |
| set.find.reused / std::set (report) | 1.032 | 1.009 | 1.004–1.014 | still worse |
| set.insert.reused / std::set (report) | 1.014 | 1.007 | 0.998–1.015 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 162.96 | 163.90 | 1.006 | 223.46 | 0.994 |
| hero.mid_erase.plain | 904.45 | 908.11 | 1.004 | 224.36 | 0.996 |
| hero.mid_insert.ord | 227.62 | 226.56 | 0.995 | 295.75 | 1.000 |
| hero.mid_insert.plain | 1255.31 | 1250.97 | 0.997 | 295.99 | 0.989 |
| hero.push_back_grow.ord | 31.88 | 31.78 | 0.997 | 86.18 | 0.989 |
| hero.push_back_grow.plain | 35.34 | 35.03 | 0.991 | 86.25 | 0.992 |
| map.erase.fresh | 32.89 | 32.98 | 1.003 | 34.88 | 0.999 |
| map.erase.reused | 29.36 | 29.58 | 1.008 | 27.34 | 1.000 |
| map.find.fresh | 5.28 | 5.28 | 1.000 | 6.55 | 1.001 |
| map.find.reused | 5.29 | 5.29 | 0.999 | 6.55 | 1.001 |
| map.insert.fresh | 31.90 | 31.69 | 0.993 | 36.97 | 0.992 |
| map.insert.reused | 30.24 | 29.86 | 0.987 | 29.44 | 0.991 |
| nested.build.ord | 270.91 | 265.54 | 0.980 | 832.63 | 0.977 |
| nested.build.plain | 278.88 | 272.78 | 0.978 | 828.63 | 0.968 |
| reset | 28.14 | 25.62 | 0.911 | 124.37 | 0.861 |
| set.erase.fresh | 29.80 | 29.78 | 0.999 | 27.47 | 0.999 |
| set.erase.reused | 22.83 | 22.91 | 1.004 | 20.39 | 0.996 |
| set.find.fresh | 4.88 | 4.87 | 1.000 | 6.45 | 0.999 |
| set.find.reused | 4.88 | 4.87 | 0.999 | 6.45 | 1.000 |
| set.insert.fresh | 28.79 | 28.48 | 0.989 | 28.83 | 0.993 |
| set.insert.reused | 23.42 | 23.49 | 1.003 | 21.07 | 1.002 |
| str.append | 1.54 | 1.52 | 0.985 | 3.59 | 1.015 |
| str.assign_long | 2.93 | 2.71 | 0.925 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 1.000 | 0.52 | 1.000 |
| str.assign_short.copy | 0.62 | 0.60 | 0.975 | 0.54 | 0.964 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.71 | 1.002 |
| vec.iterate | 0.03 | 0.03 | 1.000 | 0.03 | 0.999 |
| vec.mid_erase | 40.07 | 40.01 | 0.998 | 39.43 | 1.003 |
| vec.mid_insert | 40.62 | 40.27 | 0.992 | 42.07 | 1.004 |
| vec.push_back | 0.29 | 0.29 | 1.000 | 0.26 | 1.001 |
| vec.push_back_grow | 0.28 | 0.27 | 0.999 | 0.36 | 0.988 |

## analyze t0 (G9 at the anchor ee506d3f)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.120 |  1.123 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.765 |  0.767 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.964 |  0.966 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.39 |  1.019 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    163.41 |     223.31 |  0.728 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     293.62 |  0.761 |  0.764 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.84 |      86.47 |  0.370 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    904.94 |     224.61 |  4.034 |  4.046 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1255.54 |     291.67 |  4.223 |  4.239 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.24 |      86.22 |  0.410 |  0.411 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.764 |  1.766 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.865 |  0.866 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.51 |       3.54 |  0.431 |  0.433 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      37.03 |  0.861 |  0.862 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.48 |  0.750 |  0.751 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.37 |  0.324 |  0.325 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.88 |      34.91 |  0.941 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.88 |      44.11 |  0.744 |  0.746 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.81 |  0.997 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.03 |  0.653 |  0.654 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.757 |  0.758 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.28 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.086 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      43.87 |  0.678 |  0.679 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.27 |      29.38 |  1.027 |  1.029 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.27 |      16.11 |  1.850 |  1.862 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.29 |       6.55 |  0.809 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.29 |       7.19 |  0.726 |  0.731 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.38 |      27.34 |  1.073 |  1.076 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.38 |      17.42 |  1.674 |  1.681 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.44 |      21.08 |  1.112 |  1.113 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.44 |      15.79 |  1.446 |  1.459 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.35 |  0.669 |  0.672 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.79 |      20.35 |  1.125 |  1.128 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.79 |      16.85 |  1.356 |  1.362 | report  |
| nested.build.ord             | std            | G9     |  808 |    270.84 |     832.03 |  0.325 |  0.326 | pass    |
| reset                        | std            | G9     |  808 |     28.00 |     119.80 |  0.225 |  0.228 | pass    |
| nested.build.plain           | std            | report |  808 |    278.66 |     829.44 |  0.336 |  0.337 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.106 |  1.109 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.766 |  0.768 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.004 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.24 |  0.957 |  0.959 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.04 |      39.71 |  1.011 |  1.012 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     226.56 |  0.725 |  0.727 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.20 |     296.88 |  0.757 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.82 |      86.32 |  0.368 |  0.369 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    907.56 |     224.61 |  4.049 |  4.062 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1250.00 |     295.24 |  4.203 |  4.220 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.03 |      85.90 |  0.408 |  0.409 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.765 |  1.769 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.801 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.147 |  1.150 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.46 |  0.433 |  0.435 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.66 |      36.87 |  0.859 |  0.861 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.66 |      42.64 |  0.739 |  0.741 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.29 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.08 |  0.941 |  0.944 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.76 |  0.734 |  0.737 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.48 |      28.89 |  0.985 |  0.988 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.48 |      43.78 |  0.649 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.57 |  0.294 |  0.295 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.086 |  1.089 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.51 |  0.664 |  0.666 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.87 |      29.38 |  1.016 |  1.018 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.87 |      16.03 |  1.834 |  1.845 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.28 |       6.55 |  0.808 |  0.810 | report  |
| map.find.reused              | std::map       | report |  808 |      5.28 |       7.25 |  0.730 |  0.733 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.26 |  1.081 |  1.085 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      18.47 |  1.607 |  1.618 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.52 |      21.08 |  1.112 |  1.115 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.52 |      15.54 |  1.467 |  1.481 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.756 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.10 |  0.691 |  0.693 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.43 |  1.128 |  1.131 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.58 |  1.318 |  1.329 | report  |
| nested.build.ord             | std            | G9     |  808 |    265.62 |     820.95 |  0.322 |  0.323 | pass    |
| reset                        | std            | G9     |  808 |     26.03 |     119.46 |  0.189 |  0.192 | pass    |
| nested.build.plain           | std            | report |  808 |    273.44 |     816.41 |  0.334 |  0.335 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-06T01:46:27Z block 1 start: waited 180s load 1.46 WindowServer 1.3% remote 4
2026-11-06T01:49:32Z block 2 start: waited 180s load 1.76 WindowServer 1.2% remote 4
2026-11-06T01:52:36Z block 3 start: waited 180s load 1.09 WindowServer 1% remote 4
2026-11-06T01:55:41Z block 4 start: waited 180s load 1.23 WindowServer 1.2% remote 4
```
