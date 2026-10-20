# R1 + M2 source change delivery

2026-10-20（Asia/Shanghai）。候选：`r1-m2-combined-v1`，基于 C14 提交 `1904b9c71230026ac6f2d58f919a9ab76447d264`。

同目录 `COMBINED.patch` 是已审组合补丁的逐字节副本，2520 字节，仅修改生产文件 `xoffsetdatastructure.hpp`：

| 身份 | SHA-256 |
| --- | --- |
| 应用前 main | `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3` |
| 应用后 main | `155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501` |
| COMBINED.patch | `02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb` |

R1 将 canonical 地址恢复拆成可内联的 inactive 入口和 `noinline` active helper。每次实际调用仍执行原 acquire 检查；active helper 保留原 cold body、owner/TLS/registry 检查和释放行为，不增加跨调用缓存。

M2 在当前非 string owning-vector 节点复用一次 `data()` 解析。原 capacity 包含检查仍先执行，只有 nontrivial miss 才读取 size；null-safe end 避免空指针算术，递归顺序、unqualified ADL 调用、提前返回和公开 move 的双向 owns 检查不变。

组合自身的完整功能闭环已由 [COMBINED_FULL_ACTUAL_ACCEPTANCE.json](../reviews/COMBINED_FULL_ACTUAL_ACCEPTANCE.json) 接受（10321 B，SHA-256 `6b3a0227d1e258181f4824c0862fbb609cc8ea32a2a549c17c40e367f4c4d27c`）：

| 已接受范围 | 结果 |
| --- | --- |
| 全库 normal / SAN | 73 / 73 |
| 业务 normal / SAN | 80 / 80，覆盖五方法 S/M/L 真值及原完整 body audits |
| V1 | main84 + extra8，AB4 / C4 个 native actions |
| 消费端 | 29 个实际 consumer cases，含安装、子项目、搬迁、归档和 shared runtime 闭环 |
| 生命周期 | 52 个有限模型及 pool 用例 |

[COMBINED_SOURCE_ACCEPTANCE.json](../reviews/COMBINED_SOURCE_ACCEPTANCE.json) 已证明：154 个 source members / 14 个生产文件的组合只改变 main；原 R1、M2 补丁的两种应用顺序均得到本候选，没有 M1/L1 或其他 source delta。当前交付复用该精确合成和生产文件证明，不重复全库或大 body 验证。

组合自身编译成功及上述功能结果已接受；机器码和机制依据保留原 R1/M2 组件范围，并结合精确源码 union/交互证明，不声称组合额外执行过诊断、ASM 或独立72对筛选。单项速度不能相加或代替组合结果。现有 demo、完整校验、V1 及独立拥有的输出语义均保持。

上述验收限于记录中的 WSL/P2996 工具链及 normal、address/undefined SAN 范围，不代表原生 Windows、ARM 或任意自定义 lifetime 已通过。完整性能结论统一见同目录 [RESULTS.md](RESULTS.md)（根会话在原完整 audit 收束后生成）；本说明不宣告正式目标达成。

代码以独立候选和可审查补丁交付。默认 C14 main 仍为上表应用前的 `66818de4…`，尚未集成此补丁；候选 main 为 `155f0847…`。后续应在匹配基线的隔离候选中应用并核对完整目标 SHA。本轮只更新交付说明并新增窄范围 implementation acceptance；补丁及源码不变，未替用户 commit/push，未执行新编译、native 或 lease。
