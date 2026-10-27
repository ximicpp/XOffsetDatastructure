# G9R3 result (2026-10-27T11:11:55Z)

t0 e383aef51d6983a753d86f6840308c01c45b5a2e (v3.0.0, next_cpp26 e383aef (#124; code of 30af216)); t1 e913d220d1f42bf90883396e63286651ba50a352 (INT4 integration head); bench/reloc of t0 (aabaa1ed859747bc5237fe471e8107c6f92c642d) for both.

```
t0 e383aef51d6983a753d86f6840308c01c45b5a2e tree fca89ecb77a98211402c702e1c4a82561f79ef4e typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (v3.0.0, next_cpp26 e383aef (#124; code of 30af216))
t1 e913d220d1f42bf90883396e63286651ba50a352 tree 71354565c4268712a16b8dcdc2a30214b3993f93 typelayout fe295929d0a5bfa7809fb737a6eb3db279599346 (INT4 integration head)
t1: diff over t0 sha256 fa85d169751b2eb9752876c497410ec5cbb85a68c13f2e72f4f9772522be5981 ( 90 files changed, 3226 insertions(+), 1694 deletions(-))
t1: library header diff over t0:  2 files changed, 722 insertions(+), 381 deletions(-)
t1: first-parent commits over t0:
  e913d220 Merge #138: wire V2.1 R4 follow-ups (owner map atomics for TSan, move-assign without 2.0 range checks)
  2f17ca98 Merge #140: G9 L10/L11, inline mid insert and erase of trivially copyable elements under Clang
  71ccc903 Merge #139: small dense reload fast path (SLG D read)
  d9bce7ca Merge next_cpp26 (2e30bdf8) into claude/wire-v21
  923a6109 Merge next_cpp26 (5ae91c54) into claude/wire-v21
  db766505 Merge #135: wire V2.1 (d) docs and formal M re-pin
  6de6286a Merge #134: wire V2.1 R7 (40-byte header, payload header off the wire)
  1321b320 Merge #133: wire V2.1 R4 (owner by address, 12-byte heads)
  4ebe7e1c Merge release 3.0.0 (next_cpp26 e383aef5, #124) into claude/wire-v21
bench/reloc of t1: same tree as t0
reloc_bench t0 sha256 feaaeab71a10b25a2345694eb865b6c7661c4b9b5c37aee92a5b484c1dd14afe
reloc_bench t1 sha256 51f8059c3aa00ecbbb9df13a2af11f0a268a5ac3979a0c624ebad8401996db84
reloc_bench t0a sha256 cd9c4fc0cdddec416dd134d60c7ef5f9649eb3b29a3fdc58d53359a85c511b64
reloc_bench t1a sha256 680b0019da57ca2656d6463dcaab74b78da29ea841f7909933f9ffca08dbfd24
```

## G9 items, t1 / t0 and the aligned pair t1a / t0a (O3: no item worse than v3.0.0)

## XOffset / baseline, t1 / t0 (default build)

| case | baseline | class | t0 GM | t1 GM | t1 / t0 | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.753 | 0.718 | 0.954 | 0.948–0.959 | 0.947, 0.960 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.806 | 0.754 | 0.934 | 0.927–0.942 | 0.934, 0.935 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.422 | 0.364 | 0.862 | 0.855–0.870 | 0.864, 0.861 |  |
| map.erase.fresh | std::flat_map | G9 | 0.964 | 0.938 | 0.973 | 0.967–0.979 | 0.970, 0.976 |  |
| map.find.fresh | std::flat_map | G9 | 0.784 | 0.784 | 1.000 | 0.995–1.006 | 0.998, 1.003 |  |
| map.insert.fresh | std::flat_map | G9 | 0.966 | 0.871 | 0.902 | 0.898–0.906 | 0.898, 0.906 |  |
| nested.build.ord | std | G9 | 0.430 | 0.333 | 0.773 | 0.767–0.779 | 0.780, 0.766 |  |
| reset | std | G9 | 0.208 | 0.186 | 0.893 | 0.886–0.901 | 0.911, 0.876 |  |
| set.erase.fresh | std::flat_set | G9 | 1.130 | 1.092 | 0.966 | 0.959–0.973 | 0.962, 0.970 |  |
| set.find.fresh | std::flat_set | G9 | 0.757 | 0.755 | 0.997 | 0.992–1.003 | 1.001, 0.994 |  |
| set.insert.fresh | std::flat_set | G9 | 1.111 | 0.995 | 0.895 | 0.888–0.902 | 0.895, 0.895 |  |
| str.append | std::string | G9 | 0.442 | 0.417 | 0.945 | 0.939–0.950 | 0.958, 0.931 |  |
| str.assign_long | std::string | G9 | 0.862 | 0.841 | 0.975 | 0.969–0.982 | 0.997, 0.955 |  |
| str.assign_short.view | std::string | G9 | 0.366 | 0.343 | 0.938 | 0.933–0.943 | 0.938, 0.938 |  |
| vec.iterate | std::vector | G9 | 1.002 | 0.951 | 0.949 | 0.946–0.953 | 0.951, 0.948 |  |
| vec.mid_erase | std::vector | G9 | 1.043 | 1.013 | 0.971 | 0.967–0.976 | 0.970, 0.972 |  |
| vec.mid_insert | std::vector | G9 | 1.092 | 0.954 | 0.873 | 0.868–0.878 | 0.871, 0.876 |  |
| vec.push_back | std::vector | G9 | 1.287 | 1.128 | 0.877 | 0.871–0.882 | 0.895, 0.858 |  |
| vec.push_back_grow | std::vector | G9 | 0.782 | 0.764 | 0.977 | 0.968–0.987 | 0.977, 0.978 |  |
| hero.mid_erase.plain | std::vector | report | 4.638 | 4.016 | 0.866 | 0.861–0.871 | 0.861, 0.871 |  |
| hero.mid_insert.plain | std::vector | report | 4.717 | 4.196 | 0.890 | 0.885–0.894 | 0.895, 0.884 |  |
| hero.push_back_grow.plain | std::vector | report | 0.469 | 0.406 | 0.866 | 0.862–0.870 | 0.866, 0.866 |  |
| map.erase.fresh | std::map | report | 0.713 | 0.697 | 0.978 | 0.971–0.984 | 0.969, 0.987 |  |
| map.erase.reused | std::flat_map | report | 1.100 | 1.090 | 0.991 | 0.986–0.997 | 0.999, 0.984 |  |
| map.erase.reused | std::map | report | 1.303 | 1.397 | 1.072 | 1.059–1.085 | 1.064, 1.080 | worse |
| map.find.fresh | std::map | report | 0.316 | 0.314 | 0.993 | 0.988–0.999 | 0.993, 0.993 |  |
| map.find.reused | std::flat_map | report | 0.783 | 0.784 | 1.001 | 0.996–1.006 | 1.001, 1.001 |  |
| map.find.reused | std::map | report | 0.707 | 0.725 | 1.024 | 1.016–1.033 | 0.990, 1.060 | worse? |
| map.insert.fresh | std::map | report | 0.811 | 0.733 | 0.904 | 0.899–0.909 | 0.907, 0.901 |  |
| map.insert.reused | std::flat_map | report | 1.132 | 1.004 | 0.887 | 0.883–0.890 | 0.885, 0.888 |  |
| map.insert.reused | std::map | report | 1.855 | 1.629 | 0.878 | 0.872–0.884 | 0.880, 0.877 |  |
| nested.build.plain | std | report | 0.456 | 0.341 | 0.749 | 0.743–0.754 | 0.754, 0.743 |  |
| set.erase.fresh | std::set | report | 0.654 | 0.636 | 0.972 | 0.964–0.979 | 0.970, 0.973 |  |
| set.erase.reused | std::flat_set | report | 1.144 | 1.149 | 1.004 | 0.999–1.009 | 0.998, 1.011 |  |
| set.erase.reused | std::set | report | 1.175 | 1.181 | 1.005 | 0.996–1.014 | 1.023, 0.986 |  |
| set.find.fresh | std::set | report | 0.301 | 0.301 | 1.001 | 0.995–1.006 | 1.003, 0.999 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.752 | 0.994 | 0.989–0.999 | 0.993, 0.995 |  |
| set.find.reused | std::set | report | 0.681 | 0.687 | 1.009 | 1.002–1.017 | 1.021, 0.998 |  |
| set.insert.fresh | std::set | report | 0.722 | 0.638 | 0.885 | 0.879–0.890 | 0.879, 0.891 |  |
| set.insert.reused | std::flat_set | report | 1.274 | 1.081 | 0.849 | 0.844–0.854 | 0.847, 0.851 |  |
| set.insert.reused | std::set | report | 1.499 | 1.261 | 0.841 | 0.834–0.849 | 0.829, 0.854 |  |
| str.assign_short | std::string | report | 1.931 | 1.831 | 0.949 | 0.944–0.953 | 0.946, 0.951 |  |
| str.assign_short.copy | std::string | report | 1.596 | 1.187 | 0.744 | 0.739–0.748 | 0.744, 0.743 |  |

worse at t1 than t0 (by the rule): map.erase.reused / std::map (report) 1.072

above 1.01 but not worse by the rule: map.find.reused / std::map (report) 1.024

## XOffset ns/op alone, t1 / t0 (default build)

| case | t0 ns/op | t1 ns/op | t1 / t0 | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 173.7 | 166.4 | 0.958 | 0.952–0.963 | 0.968, 0.948 |  |
| hero.mid_erase.plain | 1055 | 915.7 | 0.868 | 0.864–0.872 | 0.864, 0.872 |  |
| hero.mid_insert.ord | 246.9 | 231.2 | 0.936 | 0.930–0.942 | 0.936, 0.936 |  |
| hero.mid_insert.plain | 1426 | 1268 | 0.889 | 0.885–0.893 | 0.890, 0.888 |  |
| hero.push_back_grow.ord | 38.62 | 33.47 | 0.867 | 0.858–0.875 | 0.872, 0.862 |  |
| hero.push_back_grow.plain | 42.63 | 36.54 | 0.857 | 0.853–0.861 | 0.855, 0.859 |  |
| map.erase.fresh | 34.09 | 33.17 | 0.973 | 0.970–0.976 | 0.975, 0.971 |  |
| map.erase.reused | 30.3 | 29.93 | 0.988 | 0.982–0.994 | 0.994, 0.982 |  |
| map.find.fresh | 5.165 | 5.155 | 0.998 | 0.994–1.002 | 0.997, 0.999 |  |
| map.find.reused | 5.167 | 5.177 | 1.002 | 0.999–1.005 | 1.001, 1.003 |  |
| map.insert.fresh | 35.78 | 32.48 | 0.908 | 0.903–0.912 | 0.913, 0.903 |  |
| map.insert.reused | 33.34 | 29.66 | 0.890 | 0.886–0.893 | 0.891, 0.888 |  |
| nested.build.ord | 378.1 | 293.9 | 0.777 | 0.771–0.783 | 0.771, 0.784 |  |
| nested.build.plain | 393 | 297.8 | 0.758 | 0.753–0.763 | 0.757, 0.758 |  |
| reset | 34.31 | 30.62 | 0.892 | 0.885–0.900 | 0.902, 0.882 |  |
| set.erase.fresh | 30.97 | 30.14 | 0.973 | 0.967–0.980 | 0.970, 0.976 |  |
| set.erase.reused | 23.35 | 23.42 | 1.003 | 0.998–1.008 | 0.997, 1.009 |  |
| set.find.fresh | 4.912 | 4.912 | 1.000 | 0.996–1.004 | 1.002, 0.998 |  |
| set.find.reused | 4.903 | 4.902 | 1.000 | 0.996–1.003 | 0.998, 1.001 |  |
| set.insert.fresh | 32.45 | 29.17 | 0.899 | 0.894–0.904 | 0.899, 0.900 |  |
| set.insert.reused | 27.1 | 23.03 | 0.850 | 0.844–0.856 | 0.851, 0.848 |  |
| str.append | 1.574 | 1.539 | 0.978 | 0.972–0.983 | 0.999, 0.957 |  |
| str.assign_long | 2.965 | 2.96 | 0.999 | 0.994–1.003 | 0.998, 0.999 |  |
| str.assign_short | 1.015 | 0.9273 | 0.914 | 0.908–0.920 | 0.918, 0.910 |  |
| str.assign_short.copy | 0.8301 | 0.608 | 0.732 | 0.729–0.736 | 0.736, 0.729 |  |
| str.assign_short.view | 1.017 | 0.9372 | 0.922 | 0.915–0.929 | 0.916, 0.928 |  |
| vec.iterate | 0.03395 | 0.03258 | 0.960 | 0.952–0.967 | 0.970, 0.949 |  |
| vec.mid_erase | 41.88 | 40.7 | 0.972 | 0.968–0.976 | 0.968, 0.976 |  |
| vec.mid_insert | 45.33 | 41.02 | 0.905 | 0.901–0.909 | 0.901, 0.908 |  |
| vec.push_back | 0.3348 | 0.2961 | 0.884 | 0.875–0.894 | 0.886, 0.883 |  |
| vec.push_back_grow | 0.292 | 0.2849 | 0.976 | 0.967–0.984 | 0.974, 0.977 |  |

XOffset slower at t1 than t0 (worse by the rule): none

## XOffset / baseline, t1a / t0a (functions aligned to 64 B, layout check)

| case | baseline | class | t0a GM | t1a GM | t1a / t0a | 95% interval | halves | note |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | std::vector | G9 | 0.749 | 0.716 | 0.956 | 0.948–0.963 | 0.964, 0.947 |  |
| hero.mid_insert.ord | std::vector | G9 | 0.807 | 0.761 | 0.943 | 0.937–0.949 | 0.941, 0.945 |  |
| hero.push_back_grow.ord | std::vector | G9 | 0.423 | 0.364 | 0.860 | 0.854–0.867 | 0.891, 0.830 |  |
| map.erase.fresh | std::flat_map | G9 | 0.966 | 0.939 | 0.972 | 0.966–0.978 | 0.971, 0.973 |  |
| map.find.fresh | std::flat_map | G9 | 0.781 | 0.786 | 1.006 | 1.001–1.011 | 1.009, 1.003 | worse |
| map.insert.fresh | std::flat_map | G9 | 0.965 | 0.873 | 0.904 | 0.899–0.910 | 0.912, 0.896 |  |
| nested.build.ord | std | G9 | 0.436 | 0.340 | 0.781 | 0.777–0.785 | 0.783, 0.779 |  |
| reset | std | G9 | 0.210 | 0.189 | 0.897 | 0.890–0.904 | 0.896, 0.899 |  |
| set.erase.fresh | std::flat_set | G9 | 1.126 | 1.093 | 0.971 | 0.965–0.976 | 0.965, 0.976 |  |
| set.find.fresh | std::flat_set | G9 | 0.756 | 0.757 | 1.002 | 0.998–1.006 | 1.004, 1.001 |  |
| set.insert.fresh | std::flat_set | G9 | 1.120 | 0.998 | 0.890 | 0.884–0.897 | 0.892, 0.889 |  |
| str.append | std::string | G9 | 0.426 | 0.423 | 0.993 | 0.988–0.999 | 0.986, 1.001 |  |
| str.assign_long | std::string | G9 | 0.864 | 0.864 | 0.999 | 0.995–1.004 | 1.001, 0.998 |  |
| str.assign_short.view | std::string | G9 | 0.365 | 0.343 | 0.940 | 0.935–0.945 | 0.941, 0.939 |  |
| vec.iterate | std::vector | G9 | 1.002 | 1.001 | 0.999 | 0.995–1.004 | 0.998, 1.000 |  |
| vec.mid_erase | std::vector | G9 | 1.044 | 1.020 | 0.978 | 0.972–0.983 | 0.979, 0.976 |  |
| vec.mid_insert | std::vector | G9 | 1.092 | 0.962 | 0.881 | 0.876–0.886 | 0.887, 0.875 |  |
| vec.push_back | std::vector | G9 | 1.190 | 1.115 | 0.937 | 0.931–0.943 | 0.934, 0.940 |  |
| vec.push_back_grow | std::vector | G9 | 0.782 | 0.763 | 0.974 | 0.966–0.983 | 0.979, 0.970 |  |
| hero.mid_erase.plain | std::vector | report | 4.588 | 4.018 | 0.876 | 0.871–0.880 | 0.879, 0.872 |  |
| hero.mid_insert.plain | std::vector | report | 4.658 | 4.172 | 0.896 | 0.890–0.902 | 0.894, 0.897 |  |
| hero.push_back_grow.plain | std::vector | report | 0.476 | 0.407 | 0.855 | 0.850–0.860 | 0.838, 0.871 |  |
| map.erase.fresh | std::map | report | 0.724 | 0.694 | 0.958 | 0.951–0.965 | 0.959, 0.957 |  |
| map.erase.reused | std::flat_map | report | 1.103 | 1.081 | 0.980 | 0.976–0.985 | 0.973, 0.988 |  |
| map.erase.reused | std::map | report | 1.351 | 1.324 | 0.980 | 0.969–0.991 | 0.960, 1.000 |  |
| map.find.fresh | std::map | report | 0.314 | 0.315 | 1.003 | 0.996–1.010 | 1.004, 1.002 |  |
| map.find.reused | std::flat_map | report | 0.782 | 0.786 | 1.005 | 1.000–1.010 | 1.007, 1.003 |  |
| map.find.reused | std::map | report | 0.709 | 0.716 | 1.009 | 1.001–1.017 | 1.047, 0.973 |  |
| map.insert.fresh | std::map | report | 0.819 | 0.742 | 0.906 | 0.900–0.912 | 0.908, 0.904 |  |
| map.insert.reused | std::flat_map | report | 1.136 | 1.009 | 0.888 | 0.884–0.892 | 0.888, 0.888 |  |
| map.insert.reused | std::map | report | 1.934 | 1.694 | 0.876 | 0.870–0.882 | 0.880, 0.871 |  |
| nested.build.plain | std | report | 0.461 | 0.345 | 0.748 | 0.743–0.752 | 0.734, 0.761 |  |
| set.erase.fresh | std::set | report | 0.658 | 0.641 | 0.974 | 0.969–0.979 | 0.973, 0.975 |  |
| set.erase.reused | std::flat_set | report | 1.151 | 1.154 | 1.003 | 0.998–1.008 | 1.002, 1.004 |  |
| set.erase.reused | std::set | report | 1.094 | 1.106 | 1.011 | 1.000–1.023 | 1.028, 0.994 | worse? |
| set.find.fresh | std::set | report | 0.299 | 0.289 | 0.965 | 0.959–0.971 | 0.968, 0.961 |  |
| set.find.reused | std::flat_set | report | 0.757 | 0.757 | 0.999 | 0.995–1.003 | 1.002, 0.997 |  |
| set.find.reused | std::set | report | 0.677 | 0.661 | 0.975 | 0.967–0.983 | 0.937, 1.015 |  |
| set.insert.fresh | std::set | report | 0.714 | 0.645 | 0.904 | 0.897–0.910 | 0.902, 0.906 |  |
| set.insert.reused | std::flat_set | report | 1.270 | 1.084 | 0.854 | 0.849–0.859 | 0.849, 0.859 |  |
| set.insert.reused | std::set | report | 1.534 | 1.285 | 0.838 | 0.828–0.848 | 0.823, 0.853 |  |
| str.assign_short | std::string | report | 1.923 | 1.829 | 0.951 | 0.943–0.959 | 0.950, 0.952 |  |
| str.assign_short.copy | std::string | report | 1.599 | 1.193 | 0.746 | 0.745–0.747 | 0.747, 0.745 |  |

worse at t1a than t0a (by the rule): map.find.fresh / std::flat_map (G9) 1.006

above 1.01 but not worse by the rule: set.erase.reused / std::set (report) 1.011

## XOffset ns/op alone, t1a / t0a (functions aligned to 64 B, layout check)

| case | t0a ns/op | t1a ns/op | t1a / t0a | 95% interval | halves | note |
| --- | ---: | ---: | ---: | --- | --- | --- |
| hero.mid_erase.ord | 173.4 | 165.9 | 0.957 | 0.949–0.964 | 0.958, 0.955 |  |
| hero.mid_erase.plain | 1047 | 919.2 | 0.878 | 0.874–0.882 | 0.877, 0.878 |  |
| hero.mid_insert.ord | 247.7 | 231.5 | 0.934 | 0.929–0.940 | 0.936, 0.933 |  |
| hero.mid_insert.plain | 1408 | 1262 | 0.896 | 0.892–0.900 | 0.900, 0.892 |  |
| hero.push_back_grow.ord | 38.32 | 33.26 | 0.868 | 0.863–0.873 | 0.872, 0.864 |  |
| hero.push_back_grow.plain | 42.7 | 36.64 | 0.858 | 0.854–0.862 | 0.860, 0.856 |  |
| map.erase.fresh | 34.2 | 33.21 | 0.971 | 0.965–0.976 | 0.970, 0.972 |  |
| map.erase.reused | 30.4 | 29.89 | 0.983 | 0.978–0.988 | 0.975, 0.992 |  |
| map.find.fresh | 5.152 | 5.161 | 1.002 | 0.997–1.007 | 1.005, 0.999 |  |
| map.find.reused | 5.174 | 5.178 | 1.001 | 0.997–1.005 | 1.004, 0.998 |  |
| map.insert.fresh | 35.71 | 32.68 | 0.915 | 0.909–0.922 | 0.921, 0.909 |  |
| map.insert.reused | 33.46 | 29.64 | 0.886 | 0.881–0.890 | 0.879, 0.892 |  |
| nested.build.ord | 372.8 | 294.7 | 0.791 | 0.787–0.794 | 0.790, 0.791 |  |
| nested.build.plain | 390.4 | 299.2 | 0.766 | 0.759–0.773 | 0.756, 0.777 |  |
| reset | 34.51 | 30.7 | 0.890 | 0.883–0.896 | 0.894, 0.886 |  |
| set.erase.fresh | 30.87 | 30.31 | 0.982 | 0.977–0.987 | 0.973, 0.991 |  |
| set.erase.reused | 23.48 | 23.66 | 1.008 | 1.001–1.014 | 1.007, 1.008 | worse |
| set.find.fresh | 4.901 | 4.917 | 1.003 | 0.999–1.007 | 1.004, 1.003 |  |
| set.find.reused | 4.911 | 4.897 | 0.997 | 0.993–1.001 | 1.000, 0.994 |  |
| set.insert.fresh | 32.46 | 29.24 | 0.901 | 0.894–0.908 | 0.903, 0.899 |  |
| set.insert.reused | 27.1 | 23.21 | 0.857 | 0.851–0.863 | 0.843, 0.870 |  |
| str.append | 1.555 | 1.528 | 0.982 | 0.977–0.987 | 0.978, 0.987 |  |
| str.assign_long | 2.97 | 2.94 | 0.990 | 0.985–0.994 | 0.990, 0.990 |  |
| str.assign_short | 0.9994 | 0.9363 | 0.937 | 0.929–0.945 | 0.939, 0.934 |  |
| str.assign_short.copy | 0.8199 | 0.6211 | 0.758 | 0.754–0.761 | 0.760, 0.755 |  |
| str.assign_short.view | 0.9982 | 0.9469 | 0.949 | 0.942–0.956 | 0.950, 0.947 |  |
| vec.iterate | 0.03327 | 0.0337 | 1.013 | 1.006–1.020 | 1.028, 0.998 | worse? |
| vec.mid_erase | 41.97 | 40.76 | 0.971 | 0.967–0.975 | 0.970, 0.972 |  |
| vec.mid_insert | 44.77 | 40.93 | 0.914 | 0.910–0.919 | 0.917, 0.912 |  |
| vec.push_back | 0.2926 | 0.2725 | 0.931 | 0.928–0.935 | 0.939, 0.924 |  |
| vec.push_back_grow | 0.2832 | 0.2752 | 0.972 | 0.964–0.979 | 0.977, 0.966 |  |

XOffset slower at t1a than t0a (worse by the rule): set.erase.reused 1.008

## layout check: what is worse at t1 in the default build, in the aligned build

| comparison | t1 / t0 | t1a / t0a | 95% interval (aligned) | aligned |
| --- | ---: | ---: | --- | --- |
| map.erase.reused / std::map (report) | 1.072 | 0.980 | 0.969–0.991 | not worse |

## reloc t1 / t0 (median per file, GM of 8 files each)

| case | t0 xo ns/op | t1 xo ns/op | t1 / t0 | first baseline ns/op (t0 runs) | t1a / t0a (aligned) |
| --- | ---: | ---: | ---: | ---: | ---: |
| hero.mid_erase.ord | 172.52 | 164.14 | 0.951 | 227.74 | 0.947 |
| hero.mid_erase.plain | 1042.25 | 910.84 | 0.874 | 227.86 | 0.885 |
| hero.mid_insert.ord | 244.63 | 228.06 | 0.932 | 304.59 | 0.937 |
| hero.mid_insert.plain | 1414.62 | 1256.62 | 0.888 | 301.01 | 0.894 |
| hero.push_back_grow.ord | 37.95 | 32.75 | 0.863 | 90.51 | 0.865 |
| hero.push_back_grow.plain | 42.31 | 36.21 | 0.856 | 90.42 | 0.854 |
| map.erase.fresh | 33.91 | 33.00 | 0.973 | 34.97 | 0.973 |
| map.erase.reused | 30.01 | 29.59 | 0.986 | 27.18 | 0.984 |
| map.find.fresh | 5.14 | 5.14 | 1.001 | 6.54 | 1.004 |
| map.find.reused | 5.15 | 5.15 | 1.001 | 6.56 | 1.004 |
| map.insert.fresh | 35.53 | 32.19 | 0.906 | 36.71 | 0.907 |
| map.insert.reused | 33.19 | 29.43 | 0.887 | 29.27 | 0.889 |
| nested.build.ord | 372.15 | 290.19 | 0.780 | 869.26 | 0.791 |
| nested.build.plain | 387.28 | 295.56 | 0.763 | 851.92 | 0.760 |
| reset | 33.93 | 30.49 | 0.899 | 163.34 | 0.890 |
| set.erase.fresh | 30.74 | 29.82 | 0.970 | 27.27 | 0.975 |
| set.erase.reused | 23.14 | 23.17 | 1.001 | 20.21 | 1.007 |
| set.find.fresh | 4.87 | 4.88 | 1.001 | 6.45 | 1.000 |
| set.find.reused | 4.88 | 4.88 | 0.999 | 6.45 | 0.999 |
| set.insert.fresh | 32.25 | 28.85 | 0.895 | 28.93 | 0.894 |
| set.insert.reused | 26.95 | 22.83 | 0.847 | 21.13 | 0.853 |
| str.append | 1.55 | 1.51 | 0.978 | 3.51 | 0.985 |
| str.assign_long | 2.93 | 2.93 | 1.000 | 3.40 | 1.000 |
| str.assign_short | 0.99 | 0.92 | 0.927 | 0.52 | 0.951 |
| str.assign_short.copy | 0.82 | 0.60 | 0.737 | 0.51 | 0.756 |
| str.assign_short.view | 0.99 | 0.93 | 0.943 | 2.72 | 0.941 |
| vec.iterate | 0.03 | 0.03 | 0.954 | 0.03 | 0.998 |
| vec.mid_erase | 41.77 | 40.29 | 0.965 | 39.91 | 0.975 |
| vec.mid_insert | 45.08 | 40.73 | 0.904 | 41.13 | 0.906 |
| vec.push_back | 0.33 | 0.29 | 0.882 | 0.26 | 0.939 |
| vec.push_back_grow | 0.29 | 0.28 | 0.976 | 0.37 | 0.973 |

## analyze t0 (G9 at v3.0.0)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.31 |       0.24 |  1.287 |  1.294 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.28 |       0.36 |  0.782 |  0.791 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  1.002 |  1.005 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     45.17 |      40.77 |  1.092 |  1.099 | fail    |
| vec.mid_erase                | std::vector    | G9     |  808 |     42.07 |      39.96 |  1.043 |  1.048 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    172.53 |     227.88 |  0.753 |  0.757 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    244.80 |     307.94 |  0.806 |  0.813 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     38.04 |      90.13 |  0.422 |  0.425 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |   1041.02 |     227.88 |  4.638 |  4.662 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1414.71 |     306.64 |  4.717 |  4.742 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     42.28 |      90.82 |  0.469 |  0.471 | report  |
| str.assign_short             | std::string    | report |  808 |      0.97 |       0.50 |  1.931 |  1.943 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.40 |  0.862 |  0.865 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.99 |       2.71 |  0.366 |  0.369 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.80 |       0.50 |  1.596 |  1.607 | report  |
| str.append                   | std::string    | G9     |  808 |      1.54 |       3.49 |  0.442 |  0.444 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     35.56 |      36.70 |  0.966 |  0.970 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     35.56 |      43.70 |  0.811 |  0.815 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.55 |  0.784 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.28 |  0.316 |  0.318 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.94 |      34.91 |  0.964 |  0.970 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.94 |      47.36 |  0.713 |  0.718 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     32.23 |      28.89 |  1.111 |  1.118 | fail    |
| set.insert.fresh             | std::set       | report |  808 |     32.23 |      44.68 |  0.722 |  0.725 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.87 |       6.45 |  0.757 |  0.761 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.87 |      16.30 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     30.76 |      27.26 |  1.130 |  1.136 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     30.76 |      46.88 |  0.654 |  0.659 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     33.20 |      29.30 |  1.132 |  1.136 | report  |
| map.insert.reused            | std::map       | report |  808 |     33.20 |      17.74 |  1.855 |  1.868 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.783 |  0.786 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.26 |  0.707 |  0.712 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     30.03 |      27.18 |  1.100 |  1.106 | report  |
| map.erase.reused             | std::map       | report |  808 |     30.03 |      22.13 |  1.303 |  1.321 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     26.94 |      21.16 |  1.274 |  1.279 | report  |
| set.insert.reused            | std::set       | report |  808 |     26.94 |      17.66 |  1.499 |  1.512 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.88 |       6.45 |  0.757 |  0.760 | report  |
| set.find.reused              | std::set       | report |  808 |      4.88 |       7.21 |  0.681 |  0.686 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.11 |      20.18 |  1.144 |  1.149 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.11 |      19.29 |  1.175 |  1.186 | report  |
| nested.build.ord             | std            | G9     |  808 |    372.38 |     869.78 |  0.430 |  0.434 | pass    |
| reset                        | std            | G9     |  808 |     33.86 |     162.77 |  0.208 |  0.210 | pass    |
| nested.build.plain           | std            | report |  808 |    388.03 |     850.28 |  0.456 |  0.460 | report  |

G9: 15 of 19 comparisons have UB <= 1.05; above: vec.push_back, vec.mid_insert, set.insert.fresh, set.erase.fresh
```

## analyze t1 (G9 at the integration head)

```
files: 8  compiler: clang 21.0.0git (https://github.com/bloomberg/clang-p2996.git d77eff1cbd78fd065668acf93b1f5f400d39134d)
G9 comparisons k = 19; one-sided 95% UB, Bonferroni t quantile 0.99737; limit UB <= 1.05

| case                         | baseline       | role   |    n |  xo ns/op | base ns/op |     GM |     UB | verdict |
| ---------------------------- | -------------- | ------ | ---: | --------: | ---------: | -----: | -----: | ------- |
| vec.push_back                | std::vector    | G9     |  808 |      0.28 |       0.24 |  1.128 |  1.136 | fail    |
| vec.push_back_grow           | std::vector    | G9     |  808 |      0.27 |       0.36 |  0.764 |  0.770 | pass    |
| vec.iterate                  | std::vector    | G9     |  808 |      0.03 |       0.03 |  0.951 |  0.955 | pass    |
| vec.mid_insert               | std::vector    | G9     |  808 |     40.37 |      42.32 |  0.954 |  0.958 | pass    |
| vec.mid_erase                | std::vector    | G9     |  808 |     40.28 |      39.71 |  1.013 |  1.018 | pass    |
| hero.mid_erase.ord           | std::vector    | G9     |  808 |    164.06 |     228.52 |  0.718 |  0.722 | pass    |
| hero.mid_insert.ord          | std::vector    | G9     |  808 |    227.88 |     307.95 |  0.754 |  0.759 | pass    |
| hero.push_back_grow.ord      | std::vector    | G9     |  808 |     32.84 |      90.74 |  0.364 |  0.368 | pass    |
| hero.mid_erase.plain         | std::vector    | report |  808 |    910.16 |     225.92 |  4.016 |  4.041 | report  |
| hero.mid_insert.plain        | std::vector    | report |  808 |   1256.50 |     305.34 |  4.196 |  4.219 | report  |
| hero.push_back_grow.plain    | std::vector    | report |  808 |     36.21 |      89.27 |  0.406 |  0.408 | report  |
| str.assign_short             | std::string    | report |  808 |      0.92 |       0.50 |  1.831 |  1.837 | report  |
| str.assign_long              | std::string    | G9     |  808 |      2.93 |       3.40 |  0.841 |  0.848 | pass    |
| str.assign_short.view        | std::string    | G9     |  808 |      0.93 |       2.71 |  0.343 |  0.344 | pass    |
| str.assign_short.copy        | std::string    | report |  808 |      0.60 |       0.50 |  1.187 |  1.192 | report  |
| str.append                   | std::string    | G9     |  808 |      1.50 |       3.61 |  0.417 |  0.420 | pass    |
| map.insert.fresh             | std::flat_map  | G9     |  808 |     32.23 |      36.87 |  0.871 |  0.875 | pass    |
| map.insert.fresh             | std::map       | report |  808 |     32.23 |      43.70 |  0.733 |  0.737 | report  |
| map.find.fresh               | std::flat_map  | G9     |  808 |      5.14 |       6.54 |  0.784 |  0.788 | pass    |
| map.find.fresh               | std::map       | report |  808 |      5.14 |      16.37 |  0.314 |  0.316 | report  |
| map.erase.fresh              | std::flat_map  | G9     |  808 |     33.04 |      35.07 |  0.938 |  0.943 | pass    |
| map.erase.fresh              | std::map       | report |  808 |     33.04 |      47.12 |  0.697 |  0.702 | report  |
| set.insert.fresh             | std::flat_set  | G9     |  808 |     28.89 |      28.97 |  0.995 |  1.005 | pass    |
| set.insert.fresh             | std::set       | report |  808 |     28.89 |      45.25 |  0.638 |  0.643 | report  |
| set.find.fresh               | std::flat_set  | G9     |  808 |      4.88 |       6.46 |  0.755 |  0.760 | pass    |
| set.find.fresh               | std::set       | report |  808 |      4.88 |      16.35 |  0.301 |  0.303 | report  |
| set.erase.fresh              | std::flat_set  | G9     |  808 |     29.79 |      27.43 |  1.092 |  1.102 | fail    |
| set.erase.fresh              | std::set       | report |  808 |     29.79 |      47.04 |  0.636 |  0.641 | report  |
| map.insert.reused            | std::flat_map  | report |  808 |     29.46 |      29.30 |  1.004 |  1.008 | report  |
| map.insert.reused            | std::map       | report |  808 |     29.46 |      18.15 |  1.629 |  1.641 | report  |
| map.find.reused              | std::flat_map  | report |  808 |      5.15 |       6.56 |  0.784 |  0.789 | report  |
| map.find.reused              | std::map       | report |  808 |      5.15 |       7.30 |  0.725 |  0.732 | report  |
| map.erase.reused             | std::flat_map  | report |  808 |     29.62 |      27.26 |  1.090 |  1.096 | report  |
| map.erase.reused             | std::map       | report |  808 |     29.62 |      20.75 |  1.397 |  1.412 | report  |
| set.insert.reused            | std::flat_set  | report |  808 |     22.87 |      21.08 |  1.081 |  1.089 | report  |
| set.insert.reused            | std::set       | report |  808 |     22.87 |      17.90 |  1.261 |  1.272 | report  |
| set.find.reused              | std::flat_set  | report |  808 |      4.87 |       6.46 |  0.752 |  0.757 | report  |
| set.find.reused              | std::set       | report |  808 |      4.87 |       7.02 |  0.687 |  0.691 | report  |
| set.erase.reused             | std::flat_set  | report |  808 |     23.19 |      20.18 |  1.149 |  1.155 | report  |
| set.erase.reused             | std::set       | report |  808 |     23.19 |      19.53 |  1.181 |  1.192 | report  |
| nested.build.ord             | std            | G9     |  808 |    290.38 |     878.91 |  0.333 |  0.335 | pass    |
| reset                        | std            | G9     |  808 |     30.59 |     164.72 |  0.186 |  0.187 | pass    |
| nested.build.plain           | std            | report |  808 |    295.56 |     865.88 |  0.341 |  0.343 | report  |

G9: 17 of 19 comparisons have UB <= 1.05; above: vec.push_back, set.erase.fresh
```

## quiet

```
2026-10-27T11:02:38Z block 1 start: waited 180s load 1.46 WindowServer 4% remote 6
2026-10-27T11:05:42Z block 2 start: waited 180s load 1.82 WindowServer 4.1% remote 6
2026-10-27T11:08:47Z block 3 start: waited 180s load 1.56 WindowServer 4.1% remote 6
2026-10-27T11:11:51Z block 4 start: waited 180s load 1.67 WindowServer 3.7% remote 6
```
