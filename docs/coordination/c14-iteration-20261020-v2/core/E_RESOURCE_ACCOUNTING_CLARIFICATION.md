# E Q/F资源计费澄清

2026-10-20，Asia/Shanghai。只读原`envelope-policy.json`、`conditioning_envelope.py`、`runtime_resources.py`和`six-role-campaign.py`的相关小型源码，并有限读取旧Q资源ledger的首尾metadata。未读取/哈希body、未运行工具或修改任何预算/去重源码。此文件澄清此前`E_Q32_SOURCE_RESOURCE_PRECHECK.md`的表述，不新增执行准入。

**此前“所有wire独立”漏了时间范围，不准确。原Q/F要求每个child实际产生fresh独立输出，但通过完整语义验证并退出后，wire与canonical/queries均可按原规则加入验证去重池。4/8GiB实际allocated上限包含wire；128/256GiB是未因去重减免的累计生成wire量。这两组限额没有源码层面的必然矛盾。** 内部R1筛选“wire始终不参与去重”的独立合同不能搬到原Q/F，反过来也不能把Q/F的wire去重搬到内部screen。

## 原字段各自计费什么

下表Q/F数字来自同一原`conditioning_envelope.py:19–28`与其冻结policy，没有修改。

| 字段 | Q32 / F64 | 原代码计费对象 |
|---|---|---|
| `maximum_wire_bytes` | 128 / 256GiB | 每个实际conditioner及primary产生的`.wire`文件size累计，去重前记录；包括warmup，去重不扣回 |
| `maximum_primary_wire_bytes` | 64 / 128GiB | 上项中所有primary的wire累计，包括预定primary warmup |
| `maximum_logical_retained_bytes` | 256 / 512GiB | output inventory每个保留路径的逻辑size总和，加metadata reserve；hardlink不同路径分别计数，pool anchor路径也计数 |
| `maximum_allocated_output_bytes` | 4 / 8GiB | 同一output inventory按唯一`(st_dev,st_ino)`计`st_blocks*512`，加metadata reserve；**wire包含在内**，同inode的多个hardlink不重复算物理块 |
| `metadata_reserve_bytes` | 372244480 / 674234368B | 原`actual_children*65536+64MiB`，覆盖queue及final metadata等有界增长，不能看成可再自由支出的空间 |
| `disk_margin_bytes` | 2GiB / 2GiB | guest与C backing各自实际free的保留余量；与上述output块账不同 |

`runtime_resources.py:14–15`明确七种child slot：stdout、stderr、receipt、wire、canonical、queries、native.json；其中`BODIES=('.wire','.canonical','.queries')`。`Ledger._inventory:62–72`包含output树内初始冻结/协议等文件及后续跟踪输出，排除单独封存的`queue/`；queue及最后metadata增长由reserve承担。`_store:73–97`按路径维护logical、按唯一inode维护allocated；运行中同inode块计数取保守较大值，最终reconcile重做实际census。记录的allocated不是RSS、文件系统总占用或VHD总物理尺寸。

`record_outputs:203–223`先跟踪实际slot，将本次wire size加入generated/primary累计，再在`verified=True`时执行三类body去重。`_check_caps:98–105`分别检查累计wire、primary wire、logical+reserve、allocated+reserve。因此既不存在“wire不计物理空间”的豁免，也不存在“hardlink把生成的几十GiB wire从预算里抹掉”。

## 独立生成与退出后去重的顺序

`six-role-campaign.py:636–640`为每个job使用独立的wire/canonical/queries/native路径。`:668–676`先execute，完整`native_result`语义/产物审计成功后才设verified，并在finally交给ledger记账；`:677`在这些在线工作之后检查完整window wall。

`runtime_resources.py:136–200`要求原输出fresh单链接且可写，核对descriptor/path身份，计算size+SHA；首次body保留为只读pool anchor，后续同identity body还必须完整逐字节比较、再次核身份，才用临时hardlink和`os.replace`原子替换。原proof保留原native文件的dev/ino/size/blocks/links=1、完整语义验证先行和精确字节比较事实。child的完整owning emit和文件生成已经发生，去重在其后，不预热、不预共享child输出，也不改变native阶段时间。去重/审计本身属于在线command真实开销，不能从Q/F wall或固定非child余量中漏掉。

失败/不完整输出不以verified方式去重，仍计数并保留。不能预判未来所有wire必然相同或按少数anchor直接批准新窗口；真实不同内容会增加唯一inode、占用和时间，原逐child门禁可能拒绝下一次launch并封存已执行prefix，不扩容、不删证据、不补样。

## 物理峰值并未只检查最后池大小

`six-role-campaign.py:586–595`的freeze逻辑投影不假设去重：wire总量、primary总量和retained各自过原cap；物理启动投影使用初始base加最坏child七slot峰值及metadata reserve，并要求guest和C backing均有`MAX_ALLOCATED+DISK_MARGIN`空余。

`runtime_resources.py:107–132`逐次launch再次检查已占用量；原temporary logical峰值为`10*file_limit+reserve`，physical峰值为`7*(file_limit+4096)+reserve`。已占用allocated加该峰值须≤4/8GiB，同时两卷free须≥该physical峰值+2GiB。这里`file_limit`来自各角色final wire、canonical、queries及1MiB的最大值（executor:643–646），不是只留一个去重后小文件的空间。去重前新wire及其他临时文件已纳入原瞬时预算。

## 旧Q实际allocated记录的可复用范围

有限读取旧`/root/xoffset-four-paths/20261018/v1/qualification-v1/resource-ledger.json`首1800B和尾2400B metadata，看到以下原记录（没有重审其13968条body proofs或body）：

| 原记录字段 | 值 |
|---|---:|
| status / recorded_attempts | passed / 4656 |
| generated_wire_bytes | 79083313912 |
| generated_primary_wire_bytes | 39541656956 |
| logical_retained_bytes | 162411657321 |
| unique_inode_bytes | 237921473 |
| allocated_output_bytes | 266027008 |
| body_anchors / deduplicated_body_slots | 7 / 13968 |
| metadata_reserve_bytes | 372244480 |

因此原记录的`allocated+reserve=638271488B`小于4GiB，而累计wire约79.08GB。差异来自原合同允许的**验证后wire/canonical/queries共享inode**，不是wire漏计。它是旧窗口原执行地点的资源记录，可用来解释该定义及原审计范围；不能直接作为当前最终六角色的实际峰值、当前guest/C卷free、新body可去重数量、耗时或新Q可完成保证。

`six-role-campaign.py:980–1020`明确可移植审计仍核原存储policy、generated计数、每个原fresh输出及pool proof；原dev/ino/block描述执行地点，搬迁可能将hardlink展开为独立文件，不能用搬迁后du重新冒充旧执行时allocated，也不能用旧数值免除新地点的实测。当前最终闭包可以复用这套**定义与源码语义**，须重新绑定实际output/staging/产物身份、unique-body数量/峰值、临时空间和双卷free；旧数值只作历史参照。

若新投影坚持“4656/9264份wire最终永久各占独立inode”，即额外禁止原Q/F wire pooling，则约79GB旧wire生成量本身已远高于4GiB，不可能把它解释为原合同已经满足。这是另一套约束，不能在不改预算/政策的情况下默默加入。本澄清恢复原已冻结Q/F计费语义，不建议或实施任何政策变更。

当前仍等待M2终态、root最终候选和真实六角色投影，Q/F未准入。完成此项有限澄清后idle；无新compile/native/queue、对外消息或大body读取。

## 本次只读源码身份

只哈希下列小型源码/policy；旧resource-ledger只读上述首尾，不生成新的全文件或body验证声明。

| 冻结成员 | bytes | SHA256 |
|---|---:|---|
| `envelope-policy.json` | 1048 | `0c886fc0d3e582de8ac5ea21a9b6f85a319e7c6fbe3edbcebbb838d70a993161` |
| `tools/conditioning_envelope.py` | 2915 | `c1ab6d1ad275b716500e269942969abb4ace4a4550a5eb890758fa1b9f1b6fa1` |
| `tools/runtime_resources.py` | 14438 | `11888f0dfbf398749eb5a5c730d35e7ce138a2fdd49070e7005562847dba5fff` |
| `tools/six-role-campaign.py` | 115800 | `7e7bdebf06bc58df4a645d1b04fe237e807b2cb46447d7c276fc1bd1377857d3` |
