from pathlib import Path
import json,hashlib,subprocess,xml.etree.ElementTree as ET
ROOT=Path.cwd();W=ROOT/'experiments/memmove-serialization-20261104'
A=json.loads((W/'measurement-v1/summary-recovered.json').read_text())
B=json.loads((W/'measurement-v2-borrowed/summary.json').read_text())
contract=json.loads((W/'measurement-v2-borrowed/preregister.json').read_text())
for name,sha in contract['identity'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha,name
assert B['qualified_cases']==8 and B['strict_all_cases']
assert ET.parse(W/'release.xml').getroot().attrib['failures']=='0'
headers=['xoffset_dense_export.hpp','xoffset_image_view.hpp','xoffset_image_builder.hpp']
row=[]
for c in B['cases']:
    v=c['modes']['borrowed'];size=c['bytes']
    label={128:'128 B',1024:'1 KiB',4096:'4 KiB',65536:'64 KiB',1048576:'1 MiB',16777216:'16 MiB'}[size]
    gate=f"额外开销上界 {v['delta_ci_ns'][2]:.2f} ns" if size<=4096 else f"耗时比上界 {v['ratio_ci'][2]:.4f}"
    row.append(f"| {label} | {c['nodes']} / {c['holes']} | {v['median_ns']:.2f} | {gate} | 通过 |")
big=max(c['modes']['borrowed']['ratio_ci'][2] for c in B['cases'] if c['bytes']>=65536)
small=max(c['modes']['borrowed']['delta_ci_ns'][2] for c in B['cases'] if c['bytes']<=4096)
baseline=json.loads((W/'baseline.json').read_text())
report=f'''# Mac 一次内存搬运实现与验证结果

2026-11-04。**受控构建、可信来源、目标已准备好的只读借用路径已达到“一次 memmove 加固定开销”目标，第二候选 8/8 场景通过。** 这是显式 opt-in 接口；现有任意外部输入的完整验证和原生可变 owning API 没有变成一次搬运。

## 实现与使用范围

- `xoffset_dense_export.hpp`：三段稠密输出、不可变 `PackedWire<T>`、整条已打包 wire 的一次搬运。初次 pack 仍有一份复制和内存成本。
- `xoffset_image_builder.hpp`：在最终 Wire 2.1 布局中受控构建消息。普通记录、标量、数组、字符串及 vector 递归支持；`seal()` 只写固定头部并转交已准备的 owner，不扫描图、不复制 payload、不分配。
- `xoffset_image_view.hpp`：只读字节代理，避免在复制后假装已存在 owning T。`PreparedImageSlot` / `ImageView` 提供存储持有及覆盖保护；新增 `BorrowedImageBuffer` 采用与 XBuffer/span 一致的借用失效规则，移除热路径引用计数。
- CMake 显式安装三个新头，注册 `test_dense_image`。生产主头、Wire 2.1、默认 emission 和原验证入口均保持原字节身份。

构建器只允许受控字段写入，每个动态块恰好链接一次。map/set 还要求键有序，已明确从构建器类型域排除，不能因继承 vector 存储而绕过检查；任意派生 string/vector 也不能借用宽泛类型 trait 进入。负向编译断言覆盖这些情况。

`PackedWire<T>` 只能由受控 builder 或已有完整验证快照产生，没有任意 span 加 trusted 标记的入口。跨进程收到的新字节仍需完整验证；本轮没有新增免验证网络协议。

接口示例、生命周期与构建不变量证明见 [合同说明](../experiments/memmove-serialization-20261104/lifetime-and-contract.md)。借用型接收必须在下次 reload、move 或销毁前结束旧代理/string_view 的使用，并由调用方同步并发访问。需要独立持有寿命时使用 ImageView，不把两种合同混为一谈。

## 环境与计时合同

当前 Mac mini 实测硬件为 **Apple M4 Pro**，macOS 26.4.1（25E253），SDK 26.4；P2996 Clang 21，提交 `9ffb96e3ce362289008e14ad2a79a249f58aa90a`，配套 runtime-v2 libc++，Release `-O3 -DNDEBUG`。不是历史文档中的另一批 M4/Clang 数字。

源码基点 `{baseline['head']}`。主头 SHA256 保持 `c4999e76010d419d231428dd262a329f7ea1d475f9cb4db602572fa8c0c75341`。

每个候选各自固定 12 个 A/A 进程、12 个对照进程，每进程 12 块；顺序按进程和块交替正反。复制前预分配并热身，同尺寸裸 memmove 使用同一来源和同一个目标地址。没有与编译、测试或归档重叠。每个场景以进程为统计单位，报告配对单侧 95% 上界；这些是逐场景区间，不是同时覆盖全部场景的校正区间。

A/A 的双侧 95% 区间须在 [0.97, 1.03] 内，小消息绝对差也须在 ±15 ns 内。64 KiB 及以上的候选/裸复制耗时比上界须 ≤1.10；128 B、1 KiB、4 KiB 的额外延迟上界须 ≤50 ns。两轮预算分别预先固定，未延长或合并样本。

## 第二候选性能

以下是借用型 `BorrowedImageBuffer::reload`，包含一次完整 wire 搬运、检查、root/长度/epoch 发布及用于消费结果的标量读取。8 个场景 A/A 全部合格。

| wire 大小 | 记录数 / 空洞数 | 每操作中位数 ns | 单侧 95% 验收项 | 结果 |
| --- | ---: | ---: | --- | --- |
{chr(10).join(row)}

大消息耗时比上界最高 {big:.4f}，小消息额外延迟上界最高 {small:.2f} ns，均在原先门槛内。固定 1 MiB、改变记录及空洞数量的结果与源码/IR 的无遍历路径一致。表中记录数不是完整验证节点计数。

这些是热页、可信只读合同下的复制结果，不能直接用于宣布网络延迟、首次分配或完整验证也达到相同成本。原 verified 与新代理的承诺不同，也不以二者的耗时比宣称同等安全合同下的加速。

## 首候选结果保留

首候选的缓存发送 8/8 达标；持有型 ImageView 接收 7/8 达标，64 KiB 耗时比上界为 1.1653，未过 1.10 门槛。其热路径已有一次 memmove，但持有/释放视图仍有原子引用计数。第二候选用独立借用接口移除这类固定操作，并保留持有型接口；没有删除原接口的保护来制造提升，也不能仅凭这两轮测量确定全部偏差由引用计数造成。

第一轮全部 24 个进程已完成后，汇总程序在极短诊断查询的 log(0) 处失败。原始数据保持不变，离线恢复只将低于计时分辨率的查询比值置为空；复制数据无零计时，阈值、样本选择和统计方法不变。其候选源码、二进制与原脚本已单独封存。

第二轮处理该诊断情况的方式在运行前登记。单次 seal/空查询也可能落在计时分辨率以下，不把 0 ns 读数当作真实零成本；seal 的 O(1) 结论依赖构建不变量和编译产物。

## 正确性与编译产物

| 检查 | 结果与边界 |
| --- | --- |
| 完整 Release CTest | 106/106 通过；后续新增接口修改只影响新增测试，最终版本已再次运行该测试 |
| 最终新增测试 | Release、ASan/UBSan、安装包独立消费者均通过；包括两组各 1000 例固定种子的有效图差分检查 |
| 字节与行为 | dense golden、原验证器读取 builder 产物、嵌套/SSO/数组、脏 bool spare、空洞、OOM、别名（含未发送 arena 头）、源销毁、持有/借用、移动、复用尾部、无分配、错误类型及排序类型拒绝 |
| 既有 sanitizer 子集 | 8 项中 7 通过；`test_verified_paths` 的分配失败注入断言失败，在未修改的 HEAD 测试及生产主头上同样复现。保留失败，不将整组写成全绿；本轮未改旧测试或关闭该断言 |
| SLG 实际模型 | Report 的 10000 条事件，240088 字节 wire；原生完整验证与代理 checksum 同为 50995077。A/D schema 属于构建器支持域，B/C 的 map schema 显式拒绝 |
| 优化 IR | `copy_packed_into`、`copy_image_into`、借用 reload 各有一次完整 `llvm.memmove`；成功路径无节点循环。借用 reload 无原子操作；seal 无 payload 搬运，仅固定头/指针写入 |

ASan/UBSan 使用这台 Mac 已有的兼容 runtime，显式加入 sanitizer-v3 头目录；最初缺头文件的配置失败保留。macOS 运行未启用 LeakSanitizer。原始 sanitizer 失败的基线对照见 `baseline-asan-control/result.json`，不能用新增测试通过掩盖它。

## 成本与未覆盖范围

构建时的分配、零初始化、字段写入和批量数组复制都记在 `controlled_build_ns`；原方式的构建、freeze、pack 分列；`context_prepare_ns` 包含两种目标及原加载器的共同准备，不能当作单一 API 的分配成本。查询与复制后读取分别计时。各进程记录峰值 RSS 与页故障；RSS 是整个测量进程高水位，包含多个对照缓冲，不能解释为单个库或单条消息的独占内存。

本轮未实现通用 ImageEditor，未迁移含 map/set 的 SLG B/C，未对完整 SLG A/B/C/D 业务 trace 建立新的正式端到端性能结论，也未实现远端可信传输准入。小型实际 Report 检查是功能整合证据，不是完整 SLG 性能验收。冷页、跨平台以及任意外部输入继续独立评估。

因此最终判断是：**受控构建的封口为固定工作，可信只读借用接收与已打包发送实现一次搬运热路径；通用、不可信、可变 owning 加载不属于此次达标范围。** 新接口保持实验性与显式使用，默认行为不变。

## 证据与复现

- [初始基线](../experiments/memmove-serialization-20261104/baseline.json)、[验证命令和退出码](../experiments/memmove-serialization-20261104/checks.json)。
- [第二轮预登记](../experiments/memmove-serialization-20261104/measurement-v2-borrowed/preregister.json)、[第二轮完整统计](../experiments/memmove-serialization-20261104/measurement-v2-borrowed/summary.json)。
- [第一轮恢复统计](../experiments/memmove-serialization-20261104/measurement-v1/summary-recovered.json)，其旁保留所有原始样本及 `candidate/` 冻结来源。
- `ir-audit-implementation.json`、`ir-audit-borrowed.json` 与对应 `.ll` 保存实际优化产物；最终交付清单记录文件 SHA256。

源码仍位于绑定的 Mac 工程，本地此文件为审阅副本。复现应在同一完整仓库及配套 P2996 工具链中使用新输出目录。正式测量脚本以输出目录已存在即拒绝来防止覆盖；不得直接覆盖本次两个窗口。没有提交、推送或发布。
'''
target=ROOT/'docs/MEMMOVE_SERIALIZATION_RESULTS_20261104.md'
assert not target.exists()
target.write_text(report,encoding='utf-8')
progress={'P0':'complete: baseline and two separate candidate identities preserved','P1':'complete: dense span and immutable packed emission implemented; v1 packed 8/8','P2':'existing verified/direct reload compared; checks preserved; no default change','P3':'complete within closed controlled-producer and trusted read-only contracts; borrowed 8/8; guarded v1 7/8','P4':'actual SLG Report correctness passed; full SLG trace and map/set mutable integration not implemented','result':'one-copy hot path verified within documented contract; generic untrusted/mutable owning decode not claimed'}
(W/'progress.json').write_text(json.dumps(progress,indent=2))
print('REPORT',target,'SHA256',hashlib.sha256(target.read_bytes()).hexdigest())
