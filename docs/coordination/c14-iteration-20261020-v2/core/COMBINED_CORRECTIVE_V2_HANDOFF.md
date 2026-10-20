# 组合external corrective v2：最终ready索引

当前修正已完成源码/metadata一致性检查，等待统筹窄审及唯一验证者执行。未执行新的compile/native/queue，不声称external通过。原INITIAL3006、external88e8、FULL d535及其已审工具、已通过normal73/SAN73和失败queue/log/1compile失败/0native均保留；不再运行full73x2。

| 当前产物 | bytes | SHA256 |
|---|---:|---|
| REQUESTS_COMBINED_EXTERNAL_v2.json | 190834 | `70f4e4a8a12b246fe5ae03294328e5aa1c86271ffe157f00102a519c80a9734f` |
| REQUESTS_COMBINED_FULL_CORRECTIVE_v2.json | 14797 | `16cf01135fdf789ed82cbfe005310767e26ffef11f4431c082ad8c69c871045e` |
| COMBINED_EXTERNAL_V2_SOURCE_EQUIVALENCE.json | 38521 | `6e9e7dd7cb31e807c828434b9ee155a37e248db6f322ebc5a9cd11694b903c3c` |
| validation/combined_full/EXTERNAL_CORRECTION_SNAPSHOT_PROOF.json | 48713 | `baae8bb4cf26497bf8cd3f48bc3b705902c52544dc56e9ad1144cc18a68cfbd1` |

实际provider为`/root/xoffset-c14-iteration/20261020/v2/validation/combined-full-v1-normal/source`，与候选完整154逐成员相同，main仍为`155f0847…cd501`；14生产map的原canonical JSON digest为`8cd90345f109308d150caa141e49fa65e2e7b1fac957957e8e45bc8ee8eb5518`。core证明另用compact/sorted JSON记录**完整154map** digest `25e8a9c92740c79adec8b6942387adb2e3c322a864ff981bb80acd41d61758e9`，两者对象不同，不混用。

四个compile各13项`-I`共52项已逐项检查。原mounted项目source与source/tests都换到上面case-sensitive snapshot，所有显式include均为/root路径，目录下不存在小写`version`。保留原`VERSION`14B，不删改生产文件，不改fixture、gnu++26、P2996/libc++/SAN/断言flags。源码提供者前后完整154由原run_native_targets的input/source guards检查。worker_argv在新request已正确指向v2，不再保留旧informational入口。

新的独立prefix为`/root/xoffset-c14-iteration/20261020/v2/validation/combined-external-corrective-v2`，lease同名加`-lease`。精确outer argv见`COMBINED_EXTERNAL_CORRECTIVE_V2_READY.json`或corrective addendum的`corrective_external_job`：原run_job.py单lease内调用原run_native_targets，4compile各120s、4native各20s、outer900s，不扩展实验。新的结果尚不存在成功声明；若最终全部成功，历史总compile attempt为1旧失败+4新成功、native为4新实际，失败历史不被覆盖。

后续只新增三份metadata派生，原65文件business/runtime/lifecycle框架不变：

- `verify_combined_initial_v2.py`仍验证原normal73/SAN73真实source/compileDB/CTest/JUnit及六repo scope，只将external验证改为v2实际request/receipt，并检查相同source证明、全部include路径、原failed1compile/0native。它不执行任何程序；未有v2成功回执就不能写passed。
- `prepare_combined_business_v2.py`写新的`COMBINED_INITIAL_GATES_v2.json`，source bridge保留原d535 FULL request/PREPARED身份并显式增加本corrective addendum；随后调用原未改的business prepare。原bridge若已有不同内容则拒绝，不覆盖历史。
- `finalize_combined_full_v2.py`重查原d535全部工具与本addendum的新closure，消费上述v2实际gate，最终仅全通过后写`COMBINED_FUNCTIONAL_GATES_v2.json`，带原失败、实际external_v2及provider等价refs。

完整before/after及差分在`COMBINED_EXTERNAL_V2_BINDER_DERIVATION.json`和`tools/c14_iteration_20261020_v2/core/COMBINED_EXTERNAL_V2_BINDER.patch`。corrective addendum的七个remaining jobs只替换prepare与finalizer入口，中间bench/functional/package/runtime/lifecycle五组argv及原caps逐字节相同；没有重新发normal/SAN/full候选、ASM、机制或72/QF请求。

`COMBINED_CORRECTIVE_V2_PREPARATION_CHECK.json`实际核了原请求三SHA、原d535本地工具不变、新18项refs、三新helper AST、external命令除provider与新prefix外完全同原、52 include、五组原business argv和预算未变。此为有限source/metadata检查，不是external功能pass。统筹review/admission另行封口；本会话此后idle，不新增binder/probe/诊断/实验。
