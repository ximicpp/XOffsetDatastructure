# G9R3 result (2026-10-28T08:06:08Z)

t0 9ef5ce75d72a172660a97d9fb4e188da3945f6c6 (v3.1.0 9ef5ce75 (library of the INT4 head e913d22)); t1 af601ea0372e17333ff8a6d64cce0a409d815f7a (CKPT4 head (next_cpp26)); bench/reloc of t0 (aabaa1ed859747bc5237fe471e8107c6f92c642d) for both.

```
t0 9ef5ce75d72a172660a97d9fb4e188da3945f6c6 tree deb8af3b0cadbd09c4f51e7917da8c3496dc62b8 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.1.0 9ef5ce75 (library of the INT4 head e913d22))
t1 af601ea0372e17333ff8a6d64cce0a409d815f7a tree 85696b57beb6e821f5acb4ca678e415f0f8384f9 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (CKPT4 head (next_cpp26))
t1: diff over t0 sha256 158d69655c484dd2bd367090e7c1b7ed7e72a3889bf1bd499291fc9645b68d5f ( 229 files changed, 15137 insertions(+), 2261 deletions(-))
t1: library header diff over t0:  2 files changed, 1439 insertions(+), 900 deletions(-)
t1: first-parent commits over t0:
  af601ea0 Merge pull request #158 from ximicpp/claude/mapdemo-macos-window-20261028 (class 0)
  bb7714cf Merge pull request #157 from ximicpp/claude/mapdemo-noexc-20261028 (class 0)
  7ddfcee3 Merge pull request #155 from ximicpp/claude/mingw-tls-fix-20261028 (class S)
  c928334d Merge pull request #156 from ximicpp/claude/ledger-153-20261028
  7680be41 Merge pull request #153 from ximicpp/claude/d-iter-20261027 (class L)
  579ca446 Merge pull request #154 from ximicpp/claude/mapdemo-android-emu-20261028 (class 0)
  7599286a Merge pull request #151 from ximicpp/claude/fuzz-nightly-evolution-20261027 (class 0)
  92404a27 Merge pull request #148 from ximicpp/claude/no-exceptions-20261027 (class S)
  00b5d351 Merge pull request #152 from ximicpp/claude/mapdemo-e2e-20261027 (class 0)
  ed1852ab Merge pull request #150 from ximicpp/claude/docs-k-field-table-20261027 (class 0)
  43669586 Merge pull request #146 from ximicpp/claude/schema-evolution-20261027 (class 0)
  5af77d34 Merge pull request #149 from ximicpp/claude/client-native-cpp20-20261027 (class S)
  f0308ee7 Merge pull request #147 from ximicpp/claude/d-read-root-20261027 (class L)
  431a7ee2 Merge pull request #145 from ximicpp/claude/schema-handshake-e1-20261027 (class 0)
  c5968008 Merge pull request #144 from ximicpp/claude/client-docs-sc9-20261027 (class 0)
  d2db6627 Merge pull request #143 from ximicpp/claude/anchor-v3.1.0-20261027 (class 0)
bench/reloc of t1: tree a7e8b02792a9cf4258e012fe4881330c6c9d5e59 differs from t0 (aabaa1ed859747bc5237fe471e8107c6f92c642d): t0's is timed against both libraries
reloc_bench t0 sha256 fccdef1448731cfe460f520e3fabc7ec825494ee4bb2be4bd2d4227ef3ffa41b
reloc_bench t1 sha256 c0b285884fbce1dd049fbea2bec67e44b827e2f45501fb3b90f3eec209d45a62
reloc_bench t0a sha256 b9a3d4539809de34cd94106c20b31517af5cb2a8a32a8fdeb48a7619be731810
reloc_bench t1a sha256 af7211d3413f3e6fb9367c5012fa67052ecbe76f02c4c268705d424b266abeb7
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (no item worse than v3.1.0)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.725 | 0.726 | 1.002 | 0.997–1.006 | 0.998, 1.005 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.759 | 0.756 | 0.996 | 0.991–1.002 | 0.996, 0.996 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.383 | 0.379 | 0.989 | 0.985–0.993 | 0.995, 0.983 |  |
| map.erase.fresh | std::flat_map | G9 | 0.937 | 0.931 | 0.994 | 0.988–0.999 | 0.990, 0.997 |  |
| map.find.fresh | std::flat_map | G9 | 0.784 | 0.785 | 1.000 | 0.995–1.005 | 1.002, 0.999 |  |
| map.insert.fresh | std::flat_map | G9 | 0.864 | 0.863 | 0.999 | 0.994–1.004 | 1.005, 0.993 |  |
| nested.build.ord | std | G9 | 0.349 | 0.347 | 0.993 | 0.988–0.999 | 0.988, 0.999 |  |
| reset | std | G9 | 0.265 | 0.258 | 0.974 | 0.967–0.980 | 0.971, 0.976 |  |
| set.erase.fresh | std::flat_set | G9 | 1.091 | 1.084 | 0.994 | 0.990–0.999 | 0.996, 0.993 |  |
| set.find.fresh | std::flat_set | G9 | 0.752 | 0.755 | 1.005 | 1.000–1.010 | 1.004, 1.005 |  |
| set.insert.fresh | std::flat_set | G9 | 0.995 | 0.993 | 0.999 | 0.994–1.003 | 0.997, 1.000 |  |
| str.append | std::string | G9 | 0.423 | 0.430 | 1.015 | 1.010–1.020 | 1.051, 0.981 | worse? |
| str.assign_long | std::string | G9 | 0.866 | 0.750 | 0.867 | 0.864–0.870 | 0.866, 0.867 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.343 | 1.002 | 0.998–1.007 | 1.005, 1.000 |  |
| vec.iterate | std::vector | G9 | 0.950 | 1.006 | 1.059 | 1.050–1.067 | 1.055, 1.062 | worse |
| vec.mid_erase | std::vector | G9 | 1.012 | 1.016 | 1.004 | 0.999–1.008 | 1.001, 1.007 |  |
| vec.mid_insert | std::vector | G9 | 0.962 | 0.994 | 1.034 | 1.029–1.038 | 1.039, 1.029 | worse |
| vec.push_back | std::vector | G9 | 1.145 | 1.219 | 1.065 | 1.059–1.071 | 1.064, 1.066 | worse |
| vec.push_back_grow | std::vector | G9 | 0.779 | 0.774 | 0.994 | 0.987–1.001 | 0.999, 0.989 |  |
| hero.mid_erase.plain | std::vector | report | 4.053 | 4.019 | 0.992 | 0.987–0.997 | 0.991, 0.992 |  |
| hero.mid_insert.plain | std::vector | report | 4.229 | 4.218 | 0.998 | 0.993–1.002 | 0.995, 1.000 |  |
| hero.push_back_grow.plain | std::vector | report | 0.422 | 0.417 | 0.988 | 0.984–0.991 | 0.994, 0.981 |  |
| map.erase.fresh | std::map | report | 0.743 | 0.741 | 0.997 | 0.992–1.001 | 0.996, 0.997 |  |
| map.erase.reused | std::flat_map | report | 1.074 | 1.083 | 1.008 | 1.003–1.013 | 1.008, 1.008 | worse |
| map.erase.reused | std::map | report | 1.674 | 1.657 | 0.990 | 0.981–0.998 | 0.979, 1.000 |  |
| map.find.fresh | std::map | report | 0.315 | 0.315 | 1.001 | 0.995–1.006 | 1.004, 0.997 |  |
| map.find.reused | std::flat_map | report | 0.785 | 0.784 | 1.000 | 0.995–1.004 | 0.997, 1.002 |  |
| map.find.reused | std::map | report | 0.708 | 0.718 | 1.014 | 1.007–1.022 | 1.013, 1.016 | worse |
| map.insert.fresh | std::map | report | 0.756 | 0.739 | 0.977 | 0.972–0.983 | 0.986, 0.969 |  |
| map.insert.reused | std::flat_map | report | 1.002 | 1.000 | 0.999 | 0.994–1.004 | 1.003, 0.994 |  |
| map.insert.reused | std::map | report | 1.808 | 1.823 | 1.008 | 1.000–1.017 | 0.996, 1.021 |  |
| nested.build.plain | std | report | 0.363 | 0.352 | 0.972 | 0.966–0.977 | 0.964, 0.979 |  |
| set.erase.fresh | std::set | report | 0.675 | 0.672 | 0.996 | 0.991–1.001 | 0.999, 0.993 |  |
| set.erase.reused | std::flat_set | report | 1.140 | 1.140 | 1.000 | 0.995–1.005 | 1.000, 0.999 |  |
| set.erase.reused | std::set | report | 1.383 | 1.351 | 0.977 | 0.969–0.986 | 0.985, 0.970 |  |
| set.find.fresh | std::set | report | 0.300 | 0.297 | 0.991 | 0.985–0.996 | 0.991, 0.991 |  |
| set.find.reused | std::flat_set | report | 0.755 | 0.754 | 0.998 | 0.994–1.002 | 0.998, 0.998 |  |
| set.find.reused | std::set | report | 0.719 | 0.747 | 1.039 | 1.032–1.045 | 1.026, 1.052 | worse |
| set.insert.fresh | std::set | report | 0.659 | 0.658 | 0.999 | 0.995–1.002 | 0.996, 1.002 |  |
| set.insert.reused | std::flat_set | report | 1.081 | 1.080 | 0.999 | 0.995–1.003 | 0.993, 1.005 |  |
| set.insert.reused | std::set | report | 1.410 | 1.439 | 1.021 | 1.010–1.031 | 1.027, 1.014 | worse |
| str.assign_short | std::string | report | 1.770 | 1.768 | 0.999 | 0.995–1.003 | 0.994, 1.004 |  |
| str.assign_short.copy | std::string | report | 1.151 | 1.153 | 1.001 | 0.996–1.007 | 0.999, 1.004 |  |

worse at t1 than t0 (by the rule): vec.iterate / std::vector (G9) 1.059; vec.mid_insert / std::vector (G9) 1.034; vec.push_back / std::vector (G9) 1.065; map.erase.reused / std::flat_map (report) 1.008; map.find.reused / std::map (report) 1.014; set.find.reused / std::set (report) 1.039; set.insert.reused / std::set (report) 1.021

above 1.01 but not worse by the rule: str.append / std::string (G9) 1.015

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.3 | 164.9 | 1.004 | 0.999–1.008 | 1.007, 1.000 |  |
| hero.mid_erase.plain | 915.8 | 906.1 | 0.989 | 0.985–0.994 | 0.989, 0.989 |  |
| hero.mid_insert.ord | 228.5 | 229.1 | 1.003 | 0.999–1.006 | 1.003, 1.003 |  |
| hero.mid_insert.plain | 1267 | 1265 | 0.999 | 0.995–1.002 | 0.999, 0.999 |  |
| hero.push_back_grow.ord | 33.46 | 33.6 | 1.004 | 1.000–1.008 | 1.006, 1.003 | worse |
| hero.push_back_grow.plain | 36.94 | 37 | 1.002 | 0.998–1.005 | 1.003, 1.000 |  |
| map.erase.fresh | 33.18 | 33.05 | 0.996 | 0.993–0.999 | 0.993, 0.999 |  |
| map.erase.reused | 29.81 | 29.92 | 1.004 | 0.999–1.009 | 1.011, 0.996 |  |
| map.find.fresh | 5.16 | 5.163 | 1.001 | 0.996–1.005 | 1.005, 0.996 |  |
| map.find.reused | 5.176 | 5.2 | 1.005 | 1.000–1.009 | 1.006, 1.003 | worse |
| map.insert.fresh | 32.43 | 32.47 | 1.001 | 0.997–1.005 | 1.000, 1.003 |  |
| map.insert.reused | 29.74 | 29.71 | 0.999 | 0.994–1.003 | 1.008, 0.990 |  |
| nested.build.ord | 297.8 | 296.1 | 0.994 | 0.989–1.000 | 0.997, 0.991 |  |
| nested.build.plain | 306.3 | 300.6 | 0.981 | 0.977–0.986 | 0.981, 0.982 |  |
| reset | 31.2 | 30.51 | 0.978 | 0.972–0.984 | 0.983, 0.972 |  |
| set.erase.fresh | 29.99 | 29.91 | 0.997 | 0.993–1.002 | 0.998, 0.997 |  |
| set.erase.reused | 23.37 | 23.37 | 1.000 | 0.995–1.005 | 0.996, 1.004 |  |
| set.find.fresh | 4.904 | 4.91 | 1.001 | 0.997–1.005 | 1.001, 1.001 |  |
| set.find.reused | 4.899 | 4.896 | 0.999 | 0.996–1.003 | 1.002, 0.997 |  |
| set.insert.fresh | 28.99 | 29 | 1.000 | 0.997–1.004 | 0.997, 1.004 |  |
| set.insert.reused | 22.99 | 22.87 | 0.995 | 0.990–0.999 | 0.980, 1.009 |  |
| str.append | 1.541 | 1.549 | 1.005 | 1.000–1.010 | 1.025, 0.985 |  |
| str.assign_long | 2.947 | 2.725 | 0.924 | 0.922–0.927 | 0.924, 0.925 |  |
| str.assign_short | 0.9316 | 0.9207 | 0.988 | 0.984–0.993 | 0.978, 0.999 |  |
| str.assign_short.copy | 0.6055 | 0.6164 | 1.018 | 1.011–1.025 | 1.014, 1.022 | worse |
| str.assign_short.view | 0.9389 | 0.9386 | 1.000 | 0.995–1.005 | 0.996, 1.003 |  |
| vec.iterate | 0.0321 | 0.03437 | 1.071 | 1.063–1.079 | 1.070, 1.072 | worse |
| vec.mid_erase | 40.48 | 40.78 | 1.007 | 1.003–1.011 | 1.005, 1.010 | worse |
| vec.mid_insert | 41.02 | 41.31 | 1.007 | 1.002–1.012 | 1.010, 1.004 | worse |
| vec.push_back | 0.2943 | 0.3198 | 1.087 | 1.075–1.098 | 1.083, 1.090 | worse |
| vec.push_back_grow | 0.2835 | 0.286 | 1.009 | 1.001–1.017 | 1.008, 1.010 | worse |

XOffset slower at t1 than t0 (worse by the rule): hero.push_back_grow.ord 1.004; map.find.reused 1.005; str.assign_short.copy 1.018; vec.iterate 1.071; vec.mid_erase 1.007; vec.mid_insert 1.007; vec.push_back 1.087; vec.push_back_grow 1.009

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.728 | 0.729 | 1.002 | 0.997–1.007 | 0.999, 1.005 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.761 | 0.764 | 1.004 | 0.999–1.009 | 1.002, 1.006 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.381 | 0.381 | 1.000 | 0.995–1.004 | 0.998, 1.002 |  |
| map.erase.fresh | std::flat_map | G9 | 0.936 | 0.942 | 1.007 | 1.002–1.012 | 1.007, 1.007 | worse |
| map.find.fresh | std::flat_map | G9 | 0.783 | 0.784 | 1.001 | 0.997–1.004 | 1.004, 0.997 |  |
| map.insert.fresh | std::flat_map | G9 | 0.868 | 0.869 | 1.002 | 0.997–1.007 | 1.007, 0.997 |  |
| nested.build.ord | std | G9 | 0.348 | 0.346 | 0.994 | 0.989–1.000 | 0.992, 0.997 |  |
| reset | std | G9 | 0.259 | 0.251 | 0.966 | 0.959–0.972 | 0.968, 0.964 |  |
| set.erase.fresh | std::flat_set | G9 | 1.089 | 1.093 | 1.003 | 0.998–1.008 | 1.001, 1.005 |  |
| set.find.fresh | std::flat_set | G9 | 0.755 | 0.757 | 1.003 | 0.999–1.007 | 1.000, 1.006 |  |
| set.insert.fresh | std::flat_set | G9 | 1.000 | 1.001 | 1.001 | 0.998–1.005 | 1.007, 0.996 |  |
| str.append | std::string | G9 | 0.426 | 0.436 | 1.024 | 1.019–1.029 | 1.041, 1.008 | worse |
| str.assign_long | std::string | G9 | 0.865 | 0.865 | 1.000 | 0.996–1.004 | 0.996, 1.003 |  |
| str.assign_short.view | std::string | G9 | 0.342 | 0.342 | 1.002 | 0.999–1.005 | 1.001, 1.004 |  |
| vec.iterate | std::vector | G9 | 1.000 | 1.002 | 1.002 | 0.998–1.007 | 1.001, 1.004 |  |
| vec.mid_erase | std::vector | G9 | 1.017 | 1.019 | 1.002 | 0.998–1.006 | 1.006, 0.998 |  |
| vec.mid_insert | std::vector | G9 | 0.957 | 0.992 | 1.036 | 1.031–1.041 | 1.028, 1.044 | worse |
| vec.push_back | std::vector | G9 | 1.125 | 1.120 | 0.995 | 0.990–1.000 | 1.001, 0.989 |  |
| vec.push_back_grow | std::vector | G9 | 0.772 | 0.774 | 1.003 | 0.996–1.009 | 1.001, 1.005 |  |
| hero.mid_erase.plain | std::vector | report | 4.042 | 4.045 | 1.001 | 0.996–1.006 | 1.003, 0.999 |  |
| hero.mid_insert.plain | std::vector | report | 4.187 | 4.174 | 0.997 | 0.992–1.002 | 0.993, 1.001 |  |
| hero.push_back_grow.plain | std::vector | report | 0.424 | 0.423 | 0.998 | 0.993–1.003 | 0.998, 0.998 |  |
| map.erase.fresh | std::map | report | 0.744 | 0.748 | 1.005 | 1.000–1.009 | 1.004, 1.005 |  |
| map.erase.reused | std::flat_map | report | 1.087 | 1.081 | 0.995 | 0.989–1.000 | 0.990, 0.999 |  |
| map.erase.reused | std::map | report | 1.619 | 1.623 | 1.002 | 0.992–1.012 | 0.993, 1.012 |  |
| map.find.fresh | std::map | report | 0.314 | 0.313 | 0.998 | 0.993–1.003 | 1.003, 0.994 |  |
| map.find.reused | std::flat_map | report | 0.787 | 0.785 | 0.998 | 0.993–1.002 | 0.999, 0.996 |  |
| map.find.reused | std::map | report | 0.699 | 0.710 | 1.016 | 1.008–1.024 | 1.023, 1.010 | worse |
| map.insert.fresh | std::map | report | 0.754 | 0.751 | 0.996 | 0.991–1.000 | 1.003, 0.988 |  |
| map.insert.reused | std::flat_map | report | 1.008 | 1.002 | 0.995 | 0.991–0.999 | 1.004, 0.986 |  |
| map.insert.reused | std::map | report | 1.849 | 1.847 | 0.999 | 0.991–1.008 | 1.012, 0.987 |  |
| nested.build.plain | std | report | 0.356 | 0.358 | 1.006 | 1.001–1.012 | 0.990, 1.023 |  |
| set.erase.fresh | std::set | report | 0.679 | 0.676 | 0.996 | 0.991–1.000 | 0.997, 0.995 |  |
| set.erase.reused | std::flat_set | report | 1.138 | 1.149 | 1.010 | 1.005–1.015 | 1.012, 1.008 | worse |
| set.erase.reused | std::set | report | 1.322 | 1.319 | 0.997 | 0.988–1.007 | 0.993, 1.002 |  |
| set.find.fresh | std::set | report | 0.288 | 0.300 | 1.040 | 1.034–1.046 | 1.036, 1.045 | worse |
| set.find.reused | std::flat_set | report | 0.755 | 0.756 | 1.001 | 0.997–1.004 | 0.999, 1.002 |  |
| set.find.reused | std::set | report | 0.670 | 0.688 | 1.026 | 1.020–1.032 | 1.011, 1.041 | worse |
| set.insert.fresh | std::set | report | 0.662 | 0.663 | 1.001 | 0.997–1.005 | 1.002, 1.000 |  |
| set.insert.reused | std::flat_set | report | 1.084 | 1.085 | 1.000 | 0.997–1.004 | 1.007, 0.994 |  |
| set.insert.reused | std::set | report | 1.467 | 1.465 | 0.999 | 0.990–1.008 | 1.006, 0.991 |  |
| str.assign_short | std::string | report | 1.770 | 1.768 | 0.999 | 0.996–1.002 | 0.997, 1.001 |  |
| str.assign_short.copy | std::string | report | 1.147 | 1.156 | 1.008 | 1.001–1.015 | 1.016, 1.001 | worse |

worse at t1a than t0a (by the rule): map.erase.fresh / std::flat_map (G9) 1.007; str.append / std::string (G9) 1.024; vec.mid_insert / std::vector (G9) 1.036; map.find.reused / std::map (report) 1.016; set.erase.reused / std::flat_set (report) 1.010; set.find.fresh / std::set (report) 1.040; set.find.reused / std::set (report) 1.026; str.assign_short.copy / std::string (report) 1.008

above 1.01 but not worse by the rule: none

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 164.2 | 165.1 | 1.006 | 1.001–1.011 | 1.003, 1.009 | worse |
| hero.mid_erase.plain | 913 | 911.7 | 0.999 | 0.994–1.003 | 1.000, 0.997 |  |
| hero.mid_insert.ord | 230.9 | 231.2 | 1.001 | 0.998–1.005 | 1.003, 1.000 |  |
| hero.mid_insert.plain | 1259 | 1251 | 0.994 | 0.991–0.998 | 0.988, 1.000 |  |
| hero.push_back_grow.ord | 33.57 | 33.53 | 0.999 | 0.994–1.004 | 0.999, 0.999 |  |
| hero.push_back_grow.plain | 37.08 | 37.11 | 1.001 | 0.996–1.006 | 1.002, 1.000 |  |
| map.erase.fresh | 33.12 | 33.28 | 1.005 | 1.000–1.009 | 1.001, 1.009 |  |
| map.erase.reused | 30.07 | 29.98 | 0.997 | 0.992–1.002 | 0.995, 1.000 |  |
| map.find.fresh | 5.156 | 5.155 | 1.000 | 0.997–1.003 | 0.999, 1.001 |  |
| map.find.reused | 5.186 | 5.17 | 0.997 | 0.993–1.001 | 1.000, 0.993 |  |
| map.insert.fresh | 32.63 | 32.51 | 0.996 | 0.992–1.001 | 1.004, 0.989 |  |
| map.insert.reused | 29.7 | 29.7 | 1.000 | 0.996–1.003 | 0.997, 1.003 |  |
| nested.build.ord | 295.7 | 293.8 | 0.994 | 0.989–0.999 | 0.987, 1.001 |  |
| nested.build.plain | 302.8 | 302.1 | 0.998 | 0.992–1.003 | 0.985, 1.011 |  |
| reset | 30.49 | 29.58 | 0.970 | 0.963–0.977 | 0.971, 0.969 |  |
| set.erase.fresh | 29.98 | 30.01 | 1.001 | 0.997–1.005 | 1.002, 1.000 |  |
| set.erase.reused | 23.29 | 23.59 | 1.013 | 1.008–1.018 | 1.018, 1.007 | worse |
| set.find.fresh | 4.903 | 4.907 | 1.001 | 0.997–1.005 | 0.999, 1.003 |  |
| set.find.reused | 4.902 | 4.901 | 1.000 | 0.996–1.004 | 0.996, 1.003 |  |
| set.insert.fresh | 29.16 | 29.03 | 0.995 | 0.992–0.999 | 1.003, 0.988 |  |
| set.insert.reused | 22.96 | 22.96 | 1.000 | 0.996–1.004 | 1.010, 0.990 |  |
| str.append | 1.522 | 1.546 | 1.016 | 1.011–1.021 | 1.035, 0.997 | worse? |
| str.assign_long | 2.957 | 2.951 | 0.998 | 0.994–1.002 | 0.996, 1.000 |  |
| str.assign_short | 0.9252 | 0.9261 | 1.001 | 0.998–1.004 | 0.998, 1.004 |  |
| str.assign_short.copy | 0.6098 | 0.6219 | 1.020 | 1.012–1.028 | 1.040, 1.000 | worse? |
| str.assign_short.view | 0.9339 | 0.9304 | 0.996 | 0.993–0.999 | 0.995, 0.997 |  |
| vec.iterate | 0.03326 | 0.03326 | 1.000 | 0.997–1.003 | 1.000, 1.001 |  |
| vec.mid_erase | 40.86 | 40.65 | 0.995 | 0.991–0.999 | 0.999, 0.991 |  |
| vec.mid_insert | 40.75 | 40.93 | 1.004 | 1.000–1.009 | 0.998, 1.011 |  |
| vec.push_back | 0.2742 | 0.2727 | 0.994 | 0.990–0.999 | 1.004, 0.985 |  |
| vec.push_back_grow | 0.2747 | 0.2753 | 1.002 | 0.997–1.007 | 1.007, 0.997 |  |

XOffset slower at t1a than t0a (worse by the rule): hero.mid_erase.ord 1.006; set.erase.reused 1.013

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| hero.push_back_grow.ord (XOffset ns/op) | 1.004 | 0.999 | 0.994–1.004 | not worse |
| map.find.reused (XOffset ns/op) | 1.005 | 0.997 | 0.993–1.001 | not worse |
| str.assign_short.copy (XOffset ns/op) | 1.018 | 1.020 | 1.012–1.028 | worse? |
| vec.iterate (XOffset ns/op) | 1.071 | 1.000 | 0.997–1.003 | not worse |
| vec.mid_erase (XOffset ns/op) | 1.007 | 0.995 | 0.991–0.999 | not worse |
| vec.mid_insert (XOffset ns/op) | 1.007 | 1.004 | 1.000–1.009 | not worse |
| vec.push_back (XOffset ns/op) | 1.087 | 0.994 | 0.990–0.999 | not worse |
| vec.push_back_grow (XOffset ns/op) | 1.009 | 1.002 | 0.997–1.007 | not worse |
| map.erase.reused / std::flat_map (report) | 1.008 | 0.995 | 0.989–1.000 | not worse |
| map.find.reused / std::map (report) | 1.014 | 1.016 | 1.008–1.024 | still worse |
| set.find.reused / std::set (report) | 1.039 | 1.026 | 1.020–1.032 | still worse |
| set.insert.reused / std::set (report) | 1.021 | 0.999 | 0.990–1.008 | not worse |
| vec.iterate / std::vector (G9) | 1.059 | 1.002 | 0.998–1.007 | not worse |
| vec.mid_insert / std::vector (G9) | 1.034 | 1.036 | 1.031–1.041 | still worse |
| vec.push_back / std::vector (G9) | 1.065 | 0.995 | 0.990–1.000 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 162.92 | 164.14 | 1.008 | 223.91 | 1.009 |
| hero.mid_erase.plain | 910.11 | 900.96 | 0.990 | 225.13 | 1.001 |
| hero.mid_insert.ord | 227.54 | 227.49 | 1.000 | 296.89 | 1.002 |
| hero.mid_insert.plain | 1257.40 | 1257.69 | 1.000 | 299.81 | 0.992 |
| hero.push_back_grow.ord | 33.17 | 33.34 | 1.005 | 86.80 | 0.997 |
| hero.push_back_grow.plain | 36.70 | 36.79 | 1.002 | 86.94 | 0.999 |
| map.erase.fresh | 33.01 | 32.96 | 0.998 | 35.14 | 0.999 |
| map.erase.reused | 29.53 | 29.62 | 1.003 | 27.49 | 0.999 |
| map.find.fresh | 5.15 | 5.14 | 0.999 | 6.55 | 1.000 |
| map.find.reused | 5.15 | 5.15 | 1.000 | 6.56 | 1.000 |
| map.insert.fresh | 32.23 | 32.26 | 1.001 | 37.14 | 0.999 |
| map.insert.reused | 29.49 | 29.56 | 1.002 | 29.42 | 0.997 |
| nested.build.ord | 294.35 | 294.10 | 0.999 | 847.38 | 0.992 |
| nested.build.plain | 302.89 | 299.00 | 0.987 | 838.48 | 0.995 |
| reset | 31.15 | 30.42 | 0.976 | 116.93 | 0.965 |
| set.erase.fresh | 29.77 | 29.72 | 0.999 | 27.37 | 1.001 |
| set.erase.reused | 23.03 | 23.10 | 1.003 | 20.35 | 1.011 |
| set.find.fresh | 4.88 | 4.88 | 1.000 | 6.46 | 1.000 |
| set.find.reused | 4.88 | 4.87 | 1.000 | 6.46 | 1.000 |
| set.insert.fresh | 28.88 | 28.89 | 1.000 | 28.98 | 0.997 |
| set.insert.reused | 22.75 | 22.78 | 1.001 | 21.08 | 0.998 |
| str.append | 1.54 | 1.52 | 0.985 | 3.62 | 1.015 |
| str.assign_long | 2.93 | 2.71 | 0.924 | 3.39 | 1.000 |
| str.assign_short | 0.92 | 0.92 | 1.000 | 0.52 | 1.001 |
| str.assign_short.copy | 0.60 | 0.60 | 1.002 | 0.52 | 1.001 |
| str.assign_short.view | 0.93 | 0.93 | 1.000 | 2.71 | 0.997 |
| vec.iterate | 0.03 | 0.03 | 1.060 | 0.03 | 1.001 |
| vec.mid_erase | 40.18 | 40.57 | 1.010 | 39.71 | 0.993 |
| vec.mid_insert | 40.54 | 41.20 | 1.016 | 42.25 | 1.005 |
| vec.push_back | 0.29 | 0.32 | 1.089 | 0.25 | 0.997 |
| vec.push_back_grow | 0.28 | 0.28 | 1.008 | 0.36 | 1.001 |

## analyze t0 (G9 at v3.1.0)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.145 |  1.151 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.35 |  0.779 |  0.785 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.950 |  0.958 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.53 |      42.24 |  0.962 |  0.967 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.12 |      39.79 |  1.012 |  1.017 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    162.77 |     224.61 |  0.725 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     293.62 |  0.759 |  0.762 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.20 |      86.85 |  0.383 |  0.384 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     225.25 |  4.053 |  4.073 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.16 |     304.69 |  4.229 |  4.249 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.70 |      87.04 |  0.422 |  0.424 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.770 |  1.780 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.39 |  0.866 |  0.869 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.342 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.151 |  1.155 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.423 |  0.425 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.11 |  0.864 |  0.869 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      42.56 |  0.756 |  0.760 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.15 |       6.55 |  0.784 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.15 |      16.38 |  0.315 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.16 |  0.937 |  0.942 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      44.27 |  0.743 |  0.747 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.995 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.78 |  0.659 |  0.661 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.752 |  0.756 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.34 |  0.300 |  0.301 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.34 |  1.091 |  1.095 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      44.11 |  0.675 |  0.678 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.38 |  1.002 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      16.20 |  1.808 |  1.825 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.785 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.29 |  0.708 |  0.713 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.54 |      27.43 |  1.074 |  1.079 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.54 |      17.50 |  1.674 |  1.690 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.081 |  1.086 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.79 |  1.410 |  1.427 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.755 |  0.758 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       6.75 |  0.719 |  0.724 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.03 |      20.35 |  1.140 |  1.147 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.03 |      16.60 |  1.383 |  1.394 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     847.66 |  0.349 |  0.352 | pass    |
| reset                        | std            | G9     |  808 |     31.25 |     116.55 |  0.265 |  0.267 | pass    |
| nested.build.plain           | std            | report |  808 |    303.38 |     837.22 |  0.363 |  0.365 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.30 |       0.24 |  1.219 |  1.226 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.28 |       0.36 |  0.774 |  0.779 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.006 |  1.014 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.49 |      40.77 |  0.994 |  0.997 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.45 |      39.79 |  1.016 |  1.020 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     225.92 |  0.726 |  0.729 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.86 |     305.01 |  0.756 |  0.761 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     33.45 |      87.97 |  0.379 |  0.380 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    899.75 |     224.61 |  4.019 |  4.042 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1257.17 |     305.33 |  4.218 |  4.238 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.78 |      88.30 |  0.417 |  0.419 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.52 |  1.768 |  1.770 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.71 |       3.61 |  0.750 |  0.753 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.345 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.52 |  1.153 |  1.162 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.430 |  0.432 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      37.35 |  0.863 |  0.867 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.29 |  0.739 |  0.743 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.55 |  0.785 |  0.789 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.39 |  0.315 |  0.317 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     32.96 |      35.24 |  0.931 |  0.936 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     32.96 |      44.31 |  0.741 |  0.744 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.993 |  0.998 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      43.86 |  0.658 |  0.660 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.47 |  0.755 |  0.759 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.53 |  0.297 |  0.299 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.71 |      27.42 |  1.084 |  1.090 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.71 |      44.11 |  0.672 |  0.676 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.54 |      29.46 |  1.000 |  1.004 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.54 |      16.03 |  1.823 |  1.839 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.788 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.25 |  0.718 |  0.724 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.34 |  1.083 |  1.088 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      17.66 |  1.657 |  1.670 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.79 |      21.08 |  1.080 |  1.084 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.79 |      15.54 |  1.439 |  1.454 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.45 |  0.754 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       6.46 |  0.747 |  0.752 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.34 |  1.140 |  1.145 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      16.93 |  1.351 |  1.365 | report  |
| nested.build.ord             | std            | G9     |  808 |    294.28 |     850.25 |  0.347 |  0.348 | pass    |
| reset                        | std            | G9     |  808 |     29.95 |     117.83 |  0.258 |  0.259 | pass    |
| nested.build.plain           | std            | report |  808 |    299.47 |     850.25 |  0.352 |  0.354 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-10-28T07:56:51Z block 1 start: waited 180s load 1.74 WindowServer 3.6% remote 5
2026-10-28T07:59:55Z block 2 start: waited 180s load 1.85 WindowServer 4.3% remote 5
2026-10-28T08:03:00Z block 3 start: waited 180s load 1.52 WindowServer 4% remote 5
2026-10-28T08:06:04Z block 4 start: waited 180s load 1.86 WindowServer 4.6% remote 5
```
