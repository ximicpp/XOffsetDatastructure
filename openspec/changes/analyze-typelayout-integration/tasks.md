## 1. 职责边界分析
- [x] 1.1 梳理 TypeLayout 的核心职责和 API 边界
- [x] 1.2 梳理 XOffsetDatastructure 对类型签名的需求场景
- [x] 1.3 评估职责分离是否清晰（是否有功能重叠或缺口）

## 2. 使用方式分析
- [x] 2.1 分析容器特化模式（XString/XVector/XSet/XMap 的 Opaque Signature）
- [x] 2.2 评估特化放置位置（在 XOffsetDatastructure 的 `boost::typelayout` 命名空间中）
- [x] 2.3 分析 Definition vs Layout 签名的选择策略
- [x] 2.4 评估 `static_assert` 签名验证模式的合理性

## 3. TypeLayout 功能完整性分析
- [x] 3.1 核心签名引擎（signature_detail.hpp）功能评估
- [x] 3.2 工具层（tools/）功能评估（compat_check, sig_export, platform_detect）
- [x] 3.3 是否有 XOffsetDatastructure 需要但 TypeLayout 未提供的功能
- [x] 3.4 是否有 TypeLayout 提供但未被使用的功能（冗余分析）

## 4. 耦合与风险分析
- [x] 4.1 版本耦合风险（submodule 版本锁定策略）
- [x] 4.2 命名空间侵入分析（在外部命名空间注册特化的影响）
- [x] 4.3 编译器依赖同步（两者都依赖 P2996 Clang）

## 5. 输出
- [x] 5.1 撰写 `docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md` 分析报告
- [x] 5.2 提出改进建议清单（标记优先级）