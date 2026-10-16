# 本轮 G4 研究交付胶囊

本目录只固定明确的本地研究交付，不关闭全路线的原生平台、默认 D/F 小差异归因、
兼容网络 sanitizer ABI 或真实业务/API 采用前提。G4 父验收及独立报告位于胶囊外，
通过本目录 manifest 绑定，避免把审阅报告加入自身哈希循环。

`source.tar.gz` 与 `source-manifest.json` 保存本轮选定的源码、实验工具、生成规则及
已接受的实际生成头。`accepted-executed/D030` 是原测量工具/测试字节，包含已如实记录的
approval 复制缺陷；修正后的 D031 live 工具另保留在正常实验路径，不能互相冒充。
归档只含普通文件，精确成员与字节逐项核对。

`metadata/` 保留同一 cut 的相关说明、合同、角色 handoff、构建身份、验收记录及证据
引用。原位置是仓库相对路径；`metadata-manifest.json` 与 `cut.json` 说明快照及范围。
live 状态后来更新不会修改本目录。原始失败、预期失败控制、噪声资格及未满足前提
分别解释，不合并成“全部通过”。
大小写相同文件的逻辑路径别名由 `metadata-path-aliases.json` 显式映射，不创建冲突成员。

这是**源码与依赖引用包**，不是完整离线自足构建环境。Boost/TypeLayout、compiler、
SDK/runtime、被测二进制及大型历史/本轮 wire 档案不重复复制，其已接受的内容身份、
许可证、构建/include 清单和档案位置由证据索引及构建身份记录引用。历史未保存的
binary/compiler/Python/full-SDK 字节 seal 仍明确缺失，不用事后哈希补造。

通过已接受输入身份继承基础验证；没有因 G4 名称新增全量 C++、Docker、63 类事件或
候选实验，也不重跑默认或失败尺寸校准。`manifest.json` 覆盖本目录全部实际文件，
自身除外；封存后不得向本目录追加独立报告、日志或新结果。
