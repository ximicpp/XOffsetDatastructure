# 共享文档最小编辑建议

2026-10-16（上海时间）。**D024 已接受三处建议并授予仅 README.md 的窗口；补丁已应用、
核对后交回窗口。**迁移说明和 C++ 示例保持不变。原补丁基于审阅时工作区字节生成，
不以 Git HEAD 为底稿，不引入公共 API，也不要求重跑基础矩阵。

| 建议 | 当前位置 | 具体内容与理由 |
| --- | --- | --- |
| README-1 | [README.md:16](../../../README.md#requirements) | 将“仓库没有非反射模式”限定为反射公共 API，并写明独立 `xoffset_virtual_memory.hpp` 为 C++17；消除与同文 storage 构建说明的冲突 |
| README-2 | README.md:125 后 | 追加验证按 profile 解释、原生 Windows 执行/独立原生 Linux 性能未验证、既有 amd64 full 仿真与 ARM64 GNU storage 分开；避免把 OS API 清单当作全部平台已实测 |
| README-3 | README.md:127 后 | 追加已验收原生 macOS 同步 TCP 参考的功能/生命周期及测量状态链接；保留生产 transport、代表性业务集成和公共 I/O API 采用边界，不硬编码易变测量数字 |

完整的逐条替换文字、旧文字和输入文件 SHA-256 见
[shared-document-edits.json](shared-document-edits.json)；可审阅的三处最小差异见
[shared-document-edits.patch](shared-document-edits.patch)。JSON 和补丁保留原提案/preimage，
最新应用结果及 postimage 见 [readme-window-return.json](readme-window-return.json)。

[迁移说明](../../STABLE_VM_STORAGE_MIGRATION.md) 的 VM/失败、复制、idle-only 池预算和
现有 allocator/载入行为与 F 一致，既未承诺 N7 新 API，也没有把复制描述为零复制。
无需插入研究进度或变更迁移契约。

[stable_storage.cpp](../../../examples/stable_storage.cpp) 仅演示现有公开头、地址稳定、
save 拥有型复制、可选 Automatic 和池。没有新增公共 API，也没有网络或平台性能承诺，
因此不改 C++ 示例，不为文档重编译或重跑基线。

应用前已复核 D024 指定的输入身份、补丁身份及三处唯一旧文字；应用后与审阅目标
postimage 完全相同。其他既有修改未覆盖，窗口现已交回。最终结果到齐后仍按
已接受的完整相关输入身份继承证据，填入新增结果，再处理最终冻结和独立 review。
