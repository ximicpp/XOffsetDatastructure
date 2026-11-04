# G9R3 result (2026-11-04T19:18:25Z)

t0 23c6e2065063bd656e9e04239e7918bf355a0588 (v3.2.1 23c6e206 (3.3.0 release checkpoint anchor)); t1 059b6a5ffdc8da9c25424a0c932f780e97a43d18 (REL330 head (3.3.0 candidate C)); bench/reloc of t0 (a7e8b02792a9cf4258e012fe4881330c6c9d5e59) for both.

```
t0 23c6e2065063bd656e9e04239e7918bf355a0588 tree 2628589ce492224a694435720d71b08dcf742117 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.2.1 23c6e206 (3.3.0 release checkpoint anchor))
t1 059b6a5ffdc8da9c25424a0c932f780e97a43d18 tree 3b7d35752cf8fe63f2cb8484336145d06fc6c461 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (REL330 head (3.3.0 candidate C))
t1: diff over t0 sha256 a89f7006ee6406b6884e0ce6518e5fdcb026d8c9ffa74a6ad3348ba485bc2d7d ( 94 files changed, 14979 insertions(+), 420 deletions(-))
t1: library header diff over t0:  2 files changed, 881 insertions(+), 192 deletions(-)
t1: first-parent commits over t0:
  059b6a5f Merge pull request #225 from ximicpp/claude/mingw-delta-test-20261104 (class 0)
  691b036a Merge pull request #224 from ximicpp/claude/delta-fuzz-20261104
  e7fd8cb0 Merge pull request #214 from ximicpp/claude/flatmap-append-20261104 (class L)
  e13245fa Merge pull request #219 from ximicpp/claude/save-empty-table-20261104 (class L)
  0d962e21 Merge pull request #220 from ximicpp/claude/hft-demo-p1-20261104 (class 0)
  fe8c0743 Merge pull request #191 from ximicpp/claude/delta-sync-20261102 (class L)
  1c528d67 Merge pull request #213 from ximicpp/claude/keep-free-table-20261104
  b817ed36 Merge pull request #217 from ximicpp/claude/fixed-msg-20261104
  2e105330 Merge pull request #216 from ximicpp/claude/prerelease-fuzz-20261104
  a34d5622 Merge pull request #218 from ximicpp/claude/slg-v3-scoring-20261104
  e682e468 Merge pull request #215 from ximicpp/claude/positioning-v3-docs-20261104
  49ce3c68 Merge pull request #212 from ximicpp/claude/np-flat-slots-20261104
  ea67054a Merge pull request #211 from ximicpp/claude/slg-held-root-20261104
  eabb583b Merge pull request #210 from ximicpp/claude/save-header-cache-20261103
  32ee1473 Merge pull request #208 from ximicpp/claude/root-cache-20261103
  5fd869db Merge pull request #207 from ximicpp/claude/save-fast-20261103
  9e14ece1 Merge pull request #209 from ximicpp/claude/slg-a-prefetch-parity-20261104
  c2ec7ee3 Merge pull request #205 from ximicpp/claude/slg-nplan-shared-20261103
  0bf0aa1f Merge pull request #206 from ximicpp/claude/validation-lean-20261103 (S class): lean validation registration
  b6e06788 Merge pull request #204 from ximicpp/claude/slg-smap-nodeps-20261103
  cc5b28ee Merge pull request #202 from ximicpp/claude/slg-fairness-20261103
  d9db04d3 Merge pull request #201 from ximicpp/claude/slg-std-snap-direct-20261103
  910b0223 Merge pull request #200 from ximicpp/claude/slg-native-split-20261103
  b3d3106f Merge pull request #199 from ximicpp/claude/slg-rich-unit-20261103
  7e24016a Merge pull request #198 from ximicpp/claude/view-fill-s-20261103 (class S): short string assign into a null head stays inline, GCC emplace_back() appends at the caller
  c5a801a9 Merge pull request #193 from ximicpp/claude/class1-integration-20261102 (class L): inline free bins, prefetch_for_update, faster hole emission
  87227259 Merge pull request #194 from ximicpp/claude/class1-migration-20261102 (class L): reload zero tail, adaptive D6 step, member swap in demo installs
  670fcc75 Merge pull request #197 from ximicpp/claude/slg-view-newblocks-20261103
  f04d6230 Merge pull request #196 from ximicpp/claude/container-grow-s-20261103 (class S): inline bump when no hole fits, append without the empty memmove
  7c2dc4d3 Merge pull request #195 from ximicpp/claude/slg-a-troops-20261103 (class 0): demo A writes back troops only
  cddcbbb2 Merge pull request #192 from ximicpp/claude/slg-cases-v3-20261102 (class 0): bench/slg game case classes 1-3 v3
  c9fbaea2 Merge pull request #190 from ximicpp/claude/slg-cases-v2-20261102 (class 0): bench/slg game demo case set
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 8d7cf69da8f65111da5837039b75d16cf334d51cd6254d5cef1e9d6033665e20
reloc_bench t1 sha256 25d9006a12d1b47b14d53a9469e60526ebc8412c01b190def92b93c2b983a797
reloc_bench t0a sha256 6e809e648798c44452f1edefc4ea856bd6f3d6c152c8fc5480886d6ff2e74116
reloc_bench t1a sha256 972c1725256dc635098dd57c1f6b270727f438c492d89778db8d403e8a292895
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than the anchor)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.720 | 0.729 | 1.013 | 1.009–1.016 | 1.020, 1.006 | worse |
| hero.mid_insert.ord | std::vector | G9 | 0.753 | 0.758 | 1.006 | 1.002–1.009 | 1.004, 1.008 | worse |
| hero.push_back_grow.ord | std::vector | G9 | 0.377 | 0.369 | 0.981 | 0.979–0.983 | 0.971, 0.990 |  |
| map.erase.fresh | std::flat_map | G9 | 0.937 | 0.936 | 0.999 | 0.996–1.002 | 1.000, 0.997 |  |
| map.find.fresh | std::flat_map | G9 | 0.784 | 0.807 | 1.030 | 1.028–1.032 | 1.032, 1.028 | worse |
| map.insert.fresh | std::flat_map | G9 | 0.867 | 0.862 | 0.994 | 0.991–0.997 | 0.996, 0.992 |  |
| nested.build.ord | std | G9 | 0.347 | 0.330 | 0.950 | 0.945–0.955 | 0.940, 0.960 |  |
| reset | std | G9 | 0.240 | 0.224 | 0.933 | 0.919–0.946 | 0.919, 0.946 |  |
| set.erase.fresh | std::flat_set | G9 | 1.084 | 1.084 | 1.000 | 0.997–1.004 | 0.997, 1.003 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.001 | 0.999, 0.998 |  |
| set.insert.fresh | std::flat_set | G9 | 0.998 | 0.996 | 0.999 | 0.996–1.001 | 0.996, 1.002 |  |
| str.append | std::string | G9 | 0.427 | 0.434 | 1.017 | 1.012–1.022 | 1.027, 1.007 | worse |
| str.assign_long | std::string | G9 | 0.800 | 0.801 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |
| str.assign_short.view | std::string | G9 | 0.317 | 0.343 | 1.082 | 1.079–1.084 | 1.082, 1.082 | worse |
| vec.iterate | std::vector | G9 | 1.000 | 1.001 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| vec.mid_erase | std::vector | G9 | 1.020 | 1.025 | 1.004 | 1.002–1.006 | 1.000, 1.009 |  |
| vec.mid_insert | std::vector | G9 | 0.992 | 0.962 | 0.970 | 0.968–0.972 | 0.970, 0.969 |  |
| vec.push_back | std::vector | G9 | 1.116 | 1.147 | 1.028 | 1.025–1.031 | 1.036, 1.020 | worse |
| vec.push_back_grow | std::vector | G9 | 0.774 | 0.770 | 0.994 | 0.991–0.997 | 1.008, 0.980 |  |
| hero.mid_erase.plain | std::vector | report | 4.046 | 4.075 | 1.007 | 1.004–1.011 | 1.005, 1.009 | worse |
| hero.mid_insert.plain | std::vector | report | 4.072 | 4.215 | 1.035 | 1.031–1.039 | 1.029, 1.041 | worse |
| hero.push_back_grow.plain | std::vector | report | 0.418 | 0.409 | 0.978 | 0.976–0.981 | 0.967, 0.989 |  |
| map.erase.fresh | std::map | report | 0.729 | 0.734 | 1.007 | 1.003–1.010 | 1.019, 0.994 |  |
| map.erase.reused | std::flat_map | report | 1.077 | 1.076 | 0.999 | 0.996–1.003 | 0.999, 1.000 |  |
| map.erase.reused | std::map | report | 1.628 | 1.620 | 0.995 | 0.989–1.001 | 1.014, 0.976 |  |
| map.find.fresh | std::map | report | 0.315 | 0.325 | 1.031 | 1.027–1.035 | 1.031, 1.031 | worse |
| map.find.reused | std::flat_map | report | 0.784 | 0.808 | 1.030 | 1.028–1.032 | 1.031, 1.028 | worse |
| map.find.reused | std::map | report | 0.709 | 0.839 | 1.184 | 1.180–1.188 | 1.195, 1.173 | worse |
| map.insert.fresh | std::map | report | 0.739 | 0.732 | 0.991 | 0.987–0.995 | 0.980, 1.002 |  |
| map.insert.reused | std::flat_map | report | 1.004 | 1.029 | 1.025 | 1.022–1.027 | 1.028, 1.022 | worse |
| map.insert.reused | std::map | report | 1.823 | 1.811 | 0.993 | 0.987–0.999 | 0.975, 1.011 |  |
| nested.build.plain | std | report | 0.355 | 0.340 | 0.958 | 0.955–0.961 | 0.955, 0.960 |  |
| set.erase.fresh | std::set | report | 0.659 | 0.661 | 1.003 | 0.999–1.008 | 1.013, 0.994 |  |
| set.erase.reused | std::flat_set | report | 1.142 | 1.130 | 0.990 | 0.987–0.993 | 0.991, 0.989 |  |
| set.erase.reused | std::set | report | 1.315 | 1.322 | 1.005 | 0.997–1.013 | 1.030, 0.982 |  |
| set.find.fresh | std::set | report | 0.299 | 0.299 | 0.999 | 0.994–1.004 | 1.002, 0.997 |  |
| set.find.reused | std::flat_set | report | 0.758 | 0.754 | 0.996 | 0.994–0.998 | 0.997, 0.995 |  |
| set.find.reused | std::set | report | 0.694 | 0.673 | 0.970 | 0.966–0.974 | 0.963, 0.977 |  |
| set.insert.fresh | std::set | report | 0.649 | 0.649 | 1.000 | 0.997–1.003 | 0.994, 1.006 |  |
| set.insert.reused | std::flat_set | report | 1.084 | 1.107 | 1.021 | 1.019–1.023 | 1.017, 1.024 | worse |
| set.insert.reused | std::set | report | 1.447 | 1.449 | 1.001 | 0.993–1.010 | 0.981, 1.022 |  |
| str.assign_short | std::string | report | 1.765 | 1.768 | 1.001 | 1.000–1.003 | 1.000, 1.003 |  |
| str.assign_short.copy | std::string | report | 1.148 | 1.150 | 1.002 | 0.998–1.005 | 1.000, 1.003 |  |

worse at t1 than t0 (by the rule): hero.mid_erase.ord / std::vector (G9) 1.013; hero.mid_insert.ord / std::vector (G9) 1.006; map.find.fresh / std::flat_map (G9) 1.030; str.append / std::string (G9) 1.017; str.assign_short.view / std::string (G9) 1.082; vec.push_back / std::vector (G9) 1.028; hero.mid_erase.plain / std::vector (report) 1.007; hero.mid_insert.plain / std::vector (report) 1.035; map.find.fresh / std::map (report) 1.031; map.find.reused / std::flat_map (report) 1.030; map.find.reused / std::map (report) 1.184; map.insert.reused / std::flat_map (report) 1.025; set.insert.reused / std::flat_set (report) 1.021

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 163.9 | 164.1 | 1.001 | 0.999–1.004 | 1.002, 1.001 |  |
| hero.mid_erase.plain | 912.9 | 920.6 | 1.008 | 1.006–1.011 | 1.014, 1.003 | worse |
| hero.mid_insert.ord | 227.5 | 227.7 | 1.001 | 0.999–1.003 | 1.003, 0.999 |  |
| hero.mid_insert.plain | 1221 | 1268 | 1.038 | 1.036–1.040 | 1.039, 1.036 | worse |
| hero.push_back_grow.ord | 33.34 | 31.99 | 0.959 | 0.958–0.961 | 0.964, 0.955 |  |
| hero.push_back_grow.plain | 36.85 | 35.45 | 0.962 | 0.960–0.964 | 0.970, 0.954 |  |
| map.erase.fresh | 33.07 | 33.03 | 0.999 | 0.996–1.001 | 0.997, 1.000 |  |
| map.erase.reused | 29.64 | 29.56 | 0.997 | 0.996–0.999 | 0.998, 0.997 |  |
| map.find.fresh | 5.137 | 5.281 | 1.028 | 1.027–1.029 | 1.028, 1.028 | worse |
| map.find.reused | 5.153 | 5.298 | 1.028 | 1.027–1.029 | 1.029, 1.027 | worse |
| map.insert.fresh | 32.28 | 31.96 | 0.990 | 0.988–0.992 | 0.991, 0.989 |  |
| map.insert.reused | 29.54 | 30.22 | 1.023 | 1.021–1.025 | 1.024, 1.023 | worse |
| nested.build.ord | 296.3 | 273.2 | 0.922 | 0.918–0.926 | 0.920, 0.924 |  |
| nested.build.plain | 302.1 | 281.2 | 0.931 | 0.928–0.933 | 0.925, 0.936 |  |
| reset | 30.47 | 29.07 | 0.954 | 0.950–0.958 | 0.948, 0.960 |  |
| set.erase.fresh | 29.79 | 29.8 | 1.000 | 0.997–1.003 | 0.998, 1.003 |  |
| set.erase.reused | 23.3 | 23.1 | 0.991 | 0.988–0.994 | 0.989, 0.993 |  |
| set.find.fresh | 4.892 | 4.883 | 0.998 | 0.997–0.999 | 0.999, 0.997 |  |
| set.find.reused | 4.895 | 4.88 | 0.997 | 0.995–0.999 | 0.996, 0.997 |  |
| set.insert.fresh | 28.88 | 28.85 | 0.999 | 0.997–1.001 | 0.998, 1.000 |  |
| set.insert.reused | 22.87 | 23.38 | 1.022 | 1.021–1.024 | 1.020, 1.025 | worse |
| str.append | 1.561 | 1.572 | 1.007 | 1.002–1.011 | 1.016, 0.997 |  |
| str.assign_long | 2.723 | 2.743 | 1.007 | 1.005–1.010 | 1.015, 1.000 | worse |
| str.assign_short | 0.9355 | 0.9498 | 1.015 | 1.011–1.019 | 1.023, 1.008 | worse |
| str.assign_short.copy | 0.6248 | 0.631 | 1.010 | 1.005–1.015 | 1.014, 1.005 | worse |
| str.assign_short.view | 0.9405 | 0.9509 | 1.011 | 1.007–1.015 | 1.017, 1.005 | worse |
| vec.iterate | 0.03314 | 0.03318 | 1.001 | 1.000–1.002 | 1.001, 1.001 | worse |
| vec.mid_erase | 40.43 | 40.41 | 1.000 | 0.998–1.001 | 0.994, 1.005 |  |
| vec.mid_insert | 40.55 | 40.58 | 1.001 | 0.999–1.002 | 1.000, 1.002 |  |
| vec.push_back | 0.2917 | 0.2981 | 1.022 | 1.010–1.034 | 1.019, 1.025 | worse |
| vec.push_back_grow | 0.2811 | 0.2776 | 0.988 | 0.982–0.993 | 0.997, 0.978 |  |

XOffset slower at t1 than t0 (worse by the rule): hero.mid_erase.plain 1.008; hero.mid_insert.plain 1.038; map.find.fresh 1.028; map.find.reused 1.028; map.insert.reused 1.023; set.insert.reused 1.022; str.assign_long 1.007; str.assign_short 1.015; str.assign_short.copy 1.010; str.assign_short.view 1.011; vec.iterate 1.001; vec.push_back 1.022

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.727 | 0.728 | 1.001 | 0.998–1.004 | 1.006, 0.996 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.764 | 0.759 | 0.993 | 0.990–0.997 | 0.998, 0.989 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.380 | 0.359 | 0.945 | 0.943–0.947 | 0.938, 0.952 |  |
| map.erase.fresh | std::flat_map | G9 | 0.938 | 0.943 | 1.005 | 1.003–1.008 | 1.003, 1.007 | worse |
| map.find.fresh | std::flat_map | G9 | 0.785 | 0.808 | 1.029 | 1.027–1.031 | 1.028, 1.031 | worse |
| map.insert.fresh | std::flat_map | G9 | 0.865 | 0.858 | 0.992 | 0.990–0.994 | 0.997, 0.988 |  |
| nested.build.ord | std | G9 | 0.325 | 0.315 | 0.972 | 0.965–0.978 | 0.950, 0.994 |  |
| reset | std | G9 | 0.209 | 0.224 | 1.071 | 1.053–1.090 | 1.039, 1.104 | worse |
| set.erase.fresh | std::flat_set | G9 | 1.094 | 1.094 | 1.000 | 0.997–1.003 | 0.999, 1.001 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.755 | 0.999 | 0.997–1.002 | 1.002, 0.997 |  |
| set.insert.fresh | std::flat_set | G9 | 0.998 | 0.993 | 0.994 | 0.992–0.997 | 0.994, 0.995 |  |
| str.append | std::string | G9 | 0.431 | 0.426 | 0.987 | 0.982–0.992 | 0.982, 0.992 |  |
| str.assign_long | std::string | G9 | 0.866 | 0.866 | 1.001 | 0.999–1.003 | 1.000, 1.001 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.003 | 1.000–1.005 | 1.003, 1.002 | worse |
| vec.iterate | std::vector | G9 | 1.000 | 1.000 | 1.000 | 0.999–1.001 | 1.000, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.022 | 1.019 | 0.997 | 0.995–0.999 | 0.999, 0.995 |  |
| vec.mid_insert | std::vector | G9 | 0.986 | 0.955 | 0.968 | 0.966–0.970 | 0.966, 0.970 |  |
| vec.push_back | std::vector | G9 | 1.118 | 1.121 | 1.003 | 1.001–1.006 | 0.999, 1.007 |  |
| vec.push_back_grow | std::vector | G9 | 0.771 | 0.762 | 0.988 | 0.984–0.991 | 0.997, 0.978 |  |
| hero.mid_erase.plain | std::vector | report | 4.029 | 4.055 | 1.007 | 1.003–1.010 | 1.009, 1.004 | worse |
| hero.mid_insert.plain | std::vector | report | 4.187 | 4.167 | 0.995 | 0.991–0.999 | 0.993, 0.997 |  |
| hero.push_back_grow.plain | std::vector | report | 0.421 | 0.403 | 0.956 | 0.954–0.958 | 0.953, 0.959 |  |
| map.erase.fresh | std::map | report | 0.737 | 0.745 | 1.011 | 1.008–1.014 | 1.019, 1.003 | worse |
| map.erase.reused | std::flat_map | report | 1.081 | 1.080 | 0.999 | 0.997–1.002 | 1.000, 0.998 |  |
| map.erase.reused | std::map | report | 1.591 | 1.586 | 0.997 | 0.990–1.004 | 0.989, 1.005 |  |
| map.find.fresh | std::map | report | 0.314 | 0.324 | 1.032 | 1.028–1.036 | 1.033, 1.031 | worse |
| map.find.reused | std::flat_map | report | 0.787 | 0.810 | 1.029 | 1.027–1.032 | 1.030, 1.029 | worse |
| map.find.reused | std::map | report | 0.704 | 0.728 | 1.034 | 1.030–1.038 | 1.048, 1.021 | worse |
| map.insert.fresh | std::map | report | 0.750 | 0.721 | 0.961 | 0.958–0.964 | 0.970, 0.952 |  |
| map.insert.reused | std::flat_map | report | 1.002 | 1.028 | 1.026 | 1.023–1.029 | 1.030, 1.022 | worse |
| map.insert.reused | std::map | report | 1.786 | 1.819 | 1.019 | 1.013–1.024 | 1.028, 1.009 | worse |
| nested.build.plain | std | report | 0.334 | 0.327 | 0.979 | 0.973–0.985 | 0.956, 1.003 |  |
| set.erase.fresh | std::set | report | 0.663 | 0.667 | 1.006 | 1.002–1.010 | 1.016, 0.996 |  |
| set.erase.reused | std::flat_set | report | 1.141 | 1.130 | 0.990 | 0.986–0.994 | 0.990, 0.990 |  |
| set.erase.reused | std::set | report | 1.297 | 1.291 | 0.995 | 0.988–1.003 | 0.982, 1.009 |  |
| set.find.fresh | std::set | report | 0.300 | 0.300 | 1.000 | 0.996–1.005 | 0.998, 1.002 |  |
| set.find.reused | std::flat_set | report | 0.756 | 0.756 | 1.001 | 0.999–1.003 | 1.001, 1.001 |  |
| set.find.reused | std::set | report | 0.679 | 0.668 | 0.983 | 0.979–0.987 | 0.976, 0.991 |  |
| set.insert.fresh | std::set | report | 0.656 | 0.651 | 0.992 | 0.990–0.995 | 1.005, 0.980 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.109 | 1.026 | 1.023–1.028 | 1.027, 1.024 | worse |
| set.insert.reused | std::set | report | 1.397 | 1.416 | 1.014 | 1.005–1.023 | 1.019, 1.009 | worse |
| str.assign_short | std::string | report | 1.769 | 1.774 | 1.003 | 1.001–1.005 | 1.000, 1.006 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.150 | 1.000 | 0.998–1.001 | 1.000, 1.000 |  |

worse at t1a than t0a (by the rule): map.erase.fresh / std::flat_map (G9) 1.005; map.find.fresh / std::flat_map (G9) 1.029; reset / std (G9) 1.071; str.assign_short.view / std::string (G9) 1.003; hero.mid_erase.plain / std::vector (report) 1.007; map.erase.fresh / std::map (report) 1.011; map.find.fresh / std::map (report) 1.032; map.find.reused / std::flat_map (report) 1.029; map.find.reused / std::map (report) 1.034; map.insert.reused / std::flat_map (report) 1.026; map.insert.reused / std::map (report) 1.019; set.insert.reused / std::flat_set (report) 1.026; set.insert.reused / std::set (report) 1.014

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.5 | 164.4 | 1.000 | 0.997–1.002 | 1.002, 0.998 |  |
| hero.mid_erase.plain | 911.9 | 912.8 | 1.001 | 0.998–1.004 | 1.003, 0.999 |  |
| hero.mid_insert.ord | 230.9 | 229 | 0.992 | 0.990–0.994 | 0.993, 0.991 |  |
| hero.mid_insert.plain | 1259 | 1253 | 0.995 | 0.993–0.998 | 0.989, 1.002 |  |
| hero.push_back_grow.ord | 33.11 | 31.64 | 0.956 | 0.954–0.957 | 0.955, 0.956 |  |
| hero.push_back_grow.plain | 36.58 | 35.32 | 0.966 | 0.964–0.968 | 0.970, 0.961 |  |
| map.erase.fresh | 32.97 | 32.97 | 1.000 | 0.999–1.001 | 1.001, 0.999 |  |
| map.erase.reused | 29.67 | 29.64 | 0.999 | 0.997–1.001 | 1.002, 0.996 |  |
| map.find.fresh | 5.141 | 5.289 | 1.029 | 1.027–1.030 | 1.027, 1.030 | worse |
| map.find.reused | 5.157 | 5.312 | 1.030 | 1.028–1.032 | 1.031, 1.029 | worse |
| map.insert.fresh | 32.22 | 31.94 | 0.991 | 0.990–0.993 | 0.992, 0.991 |  |
| map.insert.reused | 29.49 | 30.26 | 1.026 | 1.024–1.028 | 1.029, 1.024 | worse |
| nested.build.ord | 294.5 | 269.4 | 0.915 | 0.913–0.917 | 0.911, 0.919 |  |
| nested.build.plain | 301 | 279.6 | 0.929 | 0.926–0.931 | 0.931, 0.927 |  |
| reset | 30.55 | 29.22 | 0.956 | 0.951–0.962 | 0.979, 0.934 |  |
| set.erase.fresh | 29.83 | 29.84 | 1.000 | 0.998–1.003 | 1.002, 0.999 |  |
| set.erase.reused | 23.25 | 23.08 | 0.993 | 0.990–0.996 | 0.992, 0.993 |  |
| set.find.fresh | 4.883 | 4.884 | 1.000 | 0.999–1.002 | 1.000, 1.000 |  |
| set.find.reused | 4.881 | 4.884 | 1.001 | 0.999–1.002 | 1.000, 1.002 |  |
| set.insert.fresh | 28.88 | 28.79 | 0.997 | 0.996–0.998 | 0.996, 0.998 |  |
| set.insert.reused | 22.84 | 23.37 | 1.023 | 1.021–1.025 | 1.025, 1.021 | worse |
| str.append | 1.564 | 1.57 | 1.004 | 0.999–1.008 | 1.001, 1.007 |  |
| str.assign_long | 2.956 | 2.957 | 1.000 | 0.998–1.003 | 0.999, 1.002 |  |
| str.assign_short | 0.9537 | 0.9555 | 1.002 | 0.997–1.006 | 0.997, 1.006 |  |
| str.assign_short.copy | 0.6316 | 0.6307 | 0.999 | 0.994–1.003 | 0.992, 1.006 |  |
| str.assign_short.view | 0.9541 | 0.9566 | 1.003 | 0.998–1.007 | 1.002, 1.004 |  |
| vec.iterate | 0.03322 | 0.03325 | 1.001 | 1.000–1.001 | 1.001, 1.001 | worse |
| vec.mid_erase | 40.43 | 40.32 | 0.997 | 0.996–0.998 | 0.998, 0.996 |  |
| vec.mid_insert | 40.46 | 40.48 | 1.000 | 1.000–1.001 | 1.001, 1.000 |  |
| vec.push_back | 0.2717 | 0.2718 | 1.000 | 1.000–1.001 | 1.000, 1.000 |  |
| vec.push_back_grow | 0.2734 | 0.2692 | 0.985 | 0.982–0.987 | 0.983, 0.986 |  |

XOffset slower at t1a than t0a (worse by the rule): map.find.fresh 1.029; map.find.reused 1.030; map.insert.reused 1.026; set.insert.reused 1.023; vec.iterate 1.001

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.mid_erase.plain (XOffset ns/op) | 1.008 | 1.001 | 0.998–1.004 | not worse |
| hero.mid_insert.plain (XOffset ns/op) | 1.038 | 0.995 | 0.993–0.998 | not worse |
| map.find.fresh (XOffset ns/op) | 1.028 | 1.029 | 1.027–1.030 | still worse |
| map.find.reused (XOffset ns/op) | 1.028 | 1.030 | 1.028–1.032 | still worse |
| map.insert.reused (XOffset ns/op) | 1.023 | 1.026 | 1.024–1.028 | still worse |
| set.insert.reused (XOffset ns/op) | 1.022 | 1.023 | 1.021–1.025 | still worse |
| str.assign_long (XOffset ns/op) | 1.007 | 1.000 | 0.998–1.003 | not worse |
| str.assign_short (XOffset ns/op) | 1.015 | 1.002 | 0.997–1.006 | not worse |
| str.assign_short.copy (XOffset ns/op) | 1.010 | 0.999 | 0.994–1.003 | not worse |
| str.assign_short.view (XOffset ns/op) | 1.011 | 1.003 | 0.998–1.007 | not worse |
| vec.iterate (XOffset ns/op) | 1.001 | 1.001 | 1.000–1.001 | still worse |
| vec.push_back (XOffset ns/op) | 1.022 | 1.000 | 1.000–1.001 | not worse |
| hero.mid_erase.ord / std::vector (G9) | 1.013 | 1.001 | 0.998–1.004 | not worse |
| hero.mid_erase.plain / std::vector (report) | 1.007 | 1.007 | 1.003–1.010 | still worse |
| hero.mid_insert.ord / std::vector (G9) | 1.006 | 0.993 | 0.990–0.997 | not worse |
| hero.mid_insert.plain / std::vector (report) | 1.035 | 0.995 | 0.991–0.999 | not worse |
| map.find.fresh / std::flat_map (G9) | 1.030 | 1.029 | 1.027–1.031 | still worse |
| map.find.fresh / std::map (report) | 1.031 | 1.032 | 1.028–1.036 | still worse |
| map.find.reused / std::flat_map (report) | 1.030 | 1.029 | 1.027–1.032 | still worse |
| map.find.reused / std::map (report) | 1.184 | 1.034 | 1.030–1.038 | still worse |
| map.insert.reused / std::flat_map (report) | 1.025 | 1.026 | 1.023–1.029 | still worse |
| set.insert.reused / std::flat_set (report) | 1.021 | 1.026 | 1.023–1.028 | still worse |
| str.append / std::string (G9) | 1.017 | 0.987 | 0.982–0.992 | not worse |
| str.assign_short.view / std::string (G9) | 1.082 | 1.003 | 1.000–1.005 | still worse |
| vec.push_back / std::vector (G9) | 1.028 | 1.003 | 1.001–1.006 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 163.69 | 163.85 | 1.001 | 227.35 | 1.002 |
| hero.mid_erase.plain | 912.83 | 916.84 | 1.004 | 225.45 | 1.003 |
| hero.mid_insert.ord | 227.21 | 226.97 | 0.999 | 299.89 | 0.993 |
| hero.mid_insert.plain | 1218.03 | 1260.61 | 1.035 | 302.84 | 0.997 |
| hero.push_back_grow.ord | 33.26 | 31.90 | 0.959 | 88.39 | 0.955 |
| hero.push_back_grow.plain | 36.79 | 35.37 | 0.961 | 87.90 | 0.967 |
| map.erase.fresh | 32.99 | 32.99 | 1.000 | 35.17 | 1.001 |
| map.erase.reused | 29.53 | 29.45 | 0.998 | 27.35 | 0.997 |
| map.find.fresh | 5.14 | 5.28 | 1.026 | 6.55 | 1.027 |
| map.find.reused | 5.15 | 5.30 | 1.029 | 6.56 | 1.030 |
| map.insert.fresh | 32.25 | 31.91 | 0.989 | 37.13 | 0.991 |
| map.insert.reused | 29.47 | 30.17 | 1.024 | 29.37 | 1.025 |
| nested.build.ord | 294.35 | 273.09 | 0.928 | 851.29 | 0.918 |
| nested.build.plain | 301.02 | 280.67 | 0.932 | 849.48 | 0.932 |
| reset | 30.35 | 28.95 | 0.954 | 126.37 | 0.960 |
| set.erase.fresh | 29.71 | 29.76 | 1.002 | 27.49 | 1.000 |
| set.erase.reused | 23.08 | 22.87 | 0.991 | 20.32 | 0.991 |
| set.find.fresh | 4.89 | 4.87 | 0.996 | 6.47 | 1.000 |
| set.find.reused | 4.88 | 4.87 | 0.998 | 6.45 | 1.000 |
| set.insert.fresh | 28.88 | 28.81 | 0.997 | 28.93 | 0.999 |
| set.insert.reused | 22.83 | 23.39 | 1.025 | 21.08 | 1.025 |
| str.append | 1.54 | 1.56 | 1.010 | 3.61 | 1.002 |
| str.assign_long | 2.71 | 2.71 | 1.000 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.94 | 1.024 | 0.52 | 1.005 |
| str.assign_short.copy | 0.61 | 0.63 | 1.031 | 0.53 | 0.992 |
| str.assign_short.view | 0.93 | 0.94 | 1.012 | 2.94 | 1.002 |
| vec.iterate | 0.03 | 0.03 | 1.002 | 0.03 | 1.003 |
| vec.mid_erase | 40.31 | 40.37 | 1.001 | 39.63 | 0.997 |
| vec.mid_insert | 40.52 | 40.55 | 1.001 | 40.81 | 1.000 |
| vec.push_back | 0.29 | 0.30 | 1.022 | 0.26 | 1.000 |
| vec.push_back_grow | 0.28 | 0.28 | 0.986 | 0.36 | 0.985 |

## analyze t0 (G9 at the anchor v3.2.1 23c6e206)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.27 |       0.24 |  1.116 |  1.119 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.774 |  0.777 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.000 |  1.003 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      40.93 |  0.992 |  0.993 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.020 |  1.022 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     227.22 |  0.720 |  0.723 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     305.01 |  0.753 |  0.756 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.28 |      88.66 |  0.377 |  0.378 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    913.41 |     225.25 |  4.046 |  4.060 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1220.70 |     305.33 |  4.072 |  4.086 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.74 |      87.91 |  0.418 |  0.419 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.765 |  1.768 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.800 |  0.802 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.93 |  0.317 |  0.318 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.148 |  1.153 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.62 |  0.427 |  0.429 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.11 |  0.867 |  0.870 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.97 |  0.739 |  0.742 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.785 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.33 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.16 |  0.937 |  0.941 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      45.41 |  0.729 |  0.732 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.89 |  0.998 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      44.51 |  0.649 |  0.650 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.89 |       6.47 |  0.756 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.89 |      16.38 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.70 |      27.43 |  1.084 |  1.088 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.70 |      44.92 |  0.659 |  0.662 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.004 |  1.006 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.03 |  1.823 |  1.835 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.786 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.20 |  0.709 |  0.711 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.34 |  1.077 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.74 |  1.628 |  1.639 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.084 |  1.087 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      15.46 |  1.447 |  1.460 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.758 |  0.759 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       7.00 |  0.694 |  0.697 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.26 |  1.142 |  1.146 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      17.25 |  1.315 |  1.326 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     852.84 |  0.347 |  0.349 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     117.84 |  0.240 |  0.244 | pass    |
| nested.build.plain           | std            | report |  808 |    300.78 |     848.94 |  0.355 |  0.357 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.147 |  1.151 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.770 |  0.772 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.001 |  1.002 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.61 |      42.24 |  0.962 |  0.964 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.39 |  1.025 |  1.026 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     223.95 |  0.729 |  0.732 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.22 |     296.88 |  0.758 |  0.760 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     31.90 |      86.34 |  0.369 |  0.370 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    914.72 |     224.61 |  4.075 |  4.088 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1259.77 |     304.69 |  4.215 |  4.232 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     35.32 |      86.14 |  0.409 |  0.410 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.39 |  0.801 |  0.803 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.343 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.62 |       0.53 |  1.150 |  1.154 | report  |
| str.append                   | std::string    | G9     |  808 |      1.55 |       3.57 |  0.434 |  0.436 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     31.90 |      37.03 |  0.862 |  0.865 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     31.90 |      42.97 |  0.732 |  0.736 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.28 |       6.55 |  0.807 |  0.808 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.28 |      16.29 |  0.325 |  0.326 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.936 |  0.938 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.76 |  0.734 |  0.736 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.81 |      28.89 |  0.996 |  0.999 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.81 |      44.19 |  0.649 |  0.651 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.46 |  0.755 |  0.757 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.38 |  0.299 |  0.300 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.084 |  1.087 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.68 |  0.661 |  0.664 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     30.19 |      29.38 |  1.029 |  1.030 | report  |
| map.insert.reused            | std::map       | report |  808 |     30.19 |      16.28 |  1.811 |  1.820 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.30 |       6.55 |  0.808 |  0.809 | report  |
| map.find.reused              | std::map       | report |  808 |      5.30 |       6.27 |  0.839 |  0.841 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.46 |      27.34 |  1.076 |  1.080 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.46 |      17.90 |  1.620 |  1.629 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     23.36 |      21.08 |  1.107 |  1.109 | report  |
| set.insert.reused            | std::set       | report |  808 |     23.36 |      15.62 |  1.449 |  1.461 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.754 |  0.756 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.21 |  0.673 |  0.676 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     22.87 |      20.35 |  1.130 |  1.133 | report  |
| set.erase.reused             | std::set       | report |  808 |     22.87 |      17.01 |  1.322 |  1.332 | report  |
| nested.build.ord             | std            | G9     |  808 |    273.44 |     824.22 |  0.330 |  0.331 | pass    |
| reset                        | std            | G9     |  808 |     29.30 |     121.09 |  0.224 |  0.228 | pass    |
| nested.build.plain           | std            | report |  808 |    279.97 |     822.94 |  0.340 |  0.342 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-11-04T19:09:07Z block 1 start: waited 180s load 1.64 WindowServer 4.7% remote 6
2026-11-04T19:12:12Z block 2 start: waited 180s load 1.53 WindowServer 5% remote 6
2026-11-04T19:15:17Z block 3 start: waited 180s load 1.44 WindowServer 4.7% remote 6
2026-11-04T19:18:21Z block 4 start: waited 180s load 1.54 WindowServer 4.1% remote 6
```
