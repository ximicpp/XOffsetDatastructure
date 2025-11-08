# Release v2.0 Quick Reference

## 一键发布

```bash
./release_v2.0.sh
```

## 脚本完成的工作

✅ 从 `next_practical` 创建 `release/v2.0-practical` (无历史记录)  
✅ 从 `next_cpp26` 创建 `release/v2.0-cpp26` (无历史记录)  
✅ 为两个版本打标签 `v2.0-practical` 和 `v2.0-cpp26`  
✅ (可选) 推送到 GitHub  

## 两个版本对比

| 特性 | v2.0-practical | v2.0-cpp26 |
|------|----------------|------------|
| 类型反射 | Boost.PFR | C++26 (P2996) |
| 生产环境 | ✅ 推荐 | ⚠️ 实验性 |
| 编译器 | C++20 标准 | Clang P2996 |
| 依赖 | Boost.PFR | 无 |

## 验证发布

```bash
# 查看分支
git branch | grep release

# 查看标签
git tag | grep v2.0

# 检查提交历史（应该只有1个提交）
git checkout release/v2.0-practical
git log --oneline

git checkout release/v2.0-cpp26
git log --oneline
```

## GitHub Release

推送后，在 GitHub 创建 Release：

- **v2.0-practical**: 标记为 "Latest release" ✅
- **v2.0-cpp26**: 标记为 "Pre-release" ⚠️

详细说明见：[RELEASE_GUIDE.md](RELEASE_GUIDE.md)
