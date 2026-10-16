# 有界网络诊断（W2，2026-10-16）

本目录只生成独立诊断 consumer，不修改生产头、W1/W3、冻结 runtime/catalog/contract。设计与运行依据为 `docs/coordination/vm-followup/contracts/W2-network-v1/` 的正式派发；20 个源码锚点来自 W1 prepared-v2，诊断双头来自已验收 foundation。

最终原始证据位于 `docs/validation/vm-observability/network/collection-v1/`；摘要与覆盖增量位于同级 `summary-v1/`。四夹具 + 十二网络条件各有原始 W1 与诊断进程，共 32 个 workload 进程；每网络条件两次 work，warm 模式另有完整一次 warm。两次构建中首次因丢失 C++ driver 名称而链接失败，修正后的第二次构建通过；失败产物保留。

离线复核（不会执行 C++）：

```
python3 docs/validation/vm-observability/network/collection-v1/input/tool/run_network_observation.py audit --record docs/validation/vm-observability/network/collection-v1
```

`run_network_observation.py` 在编译和 workload 阶段检查 W4 默认队列的直接 W2 子进程身份。compiler 的 `clang++` 调用名保持原样，依赖清单实际核对三个冻结头唯一解析路径。输出被限定在 W2 的两个预算根；每次 launch 前记入累计 ledger，最多 32 次；封存 COMPLETE 前检查 512 MiB。ledger 支持正常进程退出后的恢复，不承诺突然断电的严格持久性。

raw stdout 保留 W1 JSON；`n3-observation.json` 为 native 标量观测，`n3-events.json` 为完整 trace；`sidecar.json` 在进程结束后绑定 input/output/stdout/trace/原始与诊断 binary/生成器锚点及基础快照身份。审计使用 copied frozen auditor/catalog，不依赖 live checker。运行只能在所有 emitter 结束后 dump；warm、preflight、pool 构造/清理及额外 `diag.pool.check` 查询都保留。

`test_network_audit.py` 的 25 项合成测试与 `test_network_runner.py` 的 11 项产物/账本测试是 Python 控制，不是 native 覆盖。`verify_network_package.py` 以实际证据的小型搬迁副本验证 1 正控和 11 负控；`summarize_network.py` 重新核验冻结 collection 后导出范围明确的摘要和增量。

每次 load 的 domain5 实际请求为 16/32/64/128 字节，合计 240，局部观察序峰值 192。编译静态断言固定 WireValidationRange=16、wire header=88、u32=4、size_t=8；5 owned/0 free ranges 的区间数来自冻结 schema/source，不是从四次 vector 分配直接数出。共享域2/3/4不是 owner 独占；mapping 寿命与 registry 代次分别建账。

字符串 constructor 的初始化长度是逻辑请求，capacity 排除终止符，不是 malloc 实用字节、实际 libc memset 或 RSS；socket bytes 不是内核复制次数。warm 的 owning Sample 短暂延长到输出归档后再释放，属于诊断准备开销。所有阶段时间只归档，不用于性能、候选采用或端到端节省结论。N3 完整归因、代表性多 owner 争用和进程采样仍有各自 gate。
