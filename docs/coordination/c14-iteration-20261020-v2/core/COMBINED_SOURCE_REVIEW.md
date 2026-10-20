# 实际r1-m2-combined-v1源码检查

root已在FINAL_CANDIDATE_SELECTION.json（bcbb7c0e…18345）明确选择并授权创建。实际独立源码位于`build/c14-iteration-20261020-v2/candidates/r1-m2-combined-v1/source`；154成员/14生产文件，仅main改变。main279066B，SHA256 `155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501`；combined.patch2520B，SHA256 `02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb`。

prepare_combined.py实际核对根C14全部154成员、两独立候选154及其patch身份，检查root两component retention pin；把原M2的精确context hunk应用于原R1头，同时在原M2头插入R1两个原patch片段，两个应用顺序输出字节完全一致。仅精确插入/替换，不以模糊patch或新实现代替。根C14全154前后仍相同，没有M1/L1、默认集成、额外生产变化或新screen。

交互证明沿用COMBINED_INTERACTION_PRECHECK.md及原两独立source reviews：每个仍发生的canonical调用保留acquire；R1 cold body、owner/TLS/anchor/retirement/release相对顺序原样；M2 capacity先于miss-only size/null-safe live end，原ADL/const child顺序、双向owns、copy-before-retire不变。无新增持久cache、field/layout、allocation/free/maintenance或callback边界，无额外父pointer寿命；不泛称generic ADL无callback，不扩展并发释放支持域。

实际combined.patch只含这两份已保留patch的union，当前静态检查无新增阻断。真实组合编译/targeted/full/OOM/V1/consumer/runtime/lifecycle仍须自己的实际证据，独立候选通过不能替代。此为核心自检和精确派生证明，root/验证会话独立准入另行决定，不伪称已有独立最终接受。

初始请求只允许新目录normal73/SAN73和两个既有external fixture各normal/SAN，共4程序；六repo定向用例通过真实组合full的fixture/compileDB/assertions/SAN/JUnit范围核验覆盖，不满足时只补精确缺项。初始工具/请求将保持不可变，后续business闭包使用独立完整请求桥接，不追加重复初始运行。所有实际执行由验证会话独占，原全局runner/lease及outer61B c8ea…pin保持。核心本次没有C++编译/native/queue；后续无新72、诊断control或Q/F准入。
