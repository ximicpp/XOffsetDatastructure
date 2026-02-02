# CI Monitoring Tools

本目录包含用于监控 GitHub Actions CI 状态的工具。

## 工具列表

### 1. `check-ci-status.sh`
一次性检查 CI 状态，可设置超时和检查间隔。

**用法：**
```bash
# 检查一次
./scripts/check-ci-status.sh next_cpp26 60 1

# 持续检查 2 小时（每 60 秒检查一次，最多 120 次）
./scripts/check-ci-status.sh next_cpp26 60 120
```

**参数：**
- 参数 1: 分支名称（默认：next_cpp26）
- 参数 2: 检查间隔（秒，默认：60）
- 参数 3: 最大检查次数（默认：120）

**退出代码：**
- 0: CI 成功
- 1: CI 失败
- 2: CI 完成但状态未知
- 3: 超时

---

### 2. `monitor-ci.sh`
持续监控 CI 状态，自动检测新运行并报告状态变化。

**用法：**
```bash
# 在前台运行
./scripts/monitor-ci.sh next_cpp26 ci-monitor.log

# 在后台运行
nohup ./scripts/monitor-ci.sh next_cpp26 ci-monitor.log > /dev/null 2>&1 &

# 查看日志
tail -f ci-monitor.log
```

**特性：**
- 🔄 自动检测新的 CI 运行
- 📊 实时报告状态变化
- 💓 定期心跳确认监控活跃
- 🏁 完成时自动退出（成功）或继续监控（失败，等待修复）

---

### 3. `Start-CIMonitor.ps1`
PowerShell 包装器，方便在 Windows 上使用。

**用法：**

```powershell
# 启动监控
.\scripts\Start-CIMonitor.ps1

# 查看日志
.\scripts\Start-CIMonitor.ps1 -ShowLog

# 停止监控
.\scripts\Start-CIMonitor.ps1 -Stop

# 监控特定分支
.\scripts\Start-CIMonitor.ps1 -Branch main
```

**特性：**
- ✅ 后台运行，不阻塞终端
- 📝 实时日志查看
- 🛑 一键停止监控
- 🎨 彩色输出

---

## 快速开始

### Windows (PowerShell)
```powershell
# 1. 启动监控
.\scripts\Start-CIMonitor.ps1

# 2. 定期查看状态
.\scripts\Start-CIMonitor.ps1 -ShowLog

# 3. 完成后停止
.\scripts\Start-CIMonitor.ps1 -Stop
```

### WSL/Linux
```bash
# 1. 启动监控
nohup ./scripts/monitor-ci.sh next_cpp26 ci-monitor.log > /dev/null 2>&1 &

# 2. 查看日志
tail -f ci-monitor.log

# 3. 停止监控
pkill -f monitor-ci.sh
```

---

## 监控输出说明

### 状态符号
- 🔍 监控启动
- 🆕 检测到新的 CI 运行
- 📊 状态更新
- ⚙️  构建进行中
- ⏳ 构建队列中
- 🏁 构建完成
- ✅ 成功
- ❌ 失败
- ⚠️  警告
- 💓 心跳（监控活跃）

### CI 状态
- `queued`: 等待运行
- `in_progress`: 正在运行
- `completed`: 已完成

### CI 结论
- `success`: 所有测试通过
- `failure`: 某些测试失败
- `cancelled`: 用户取消
- `skipped`: 跳过执行

---

## 示例输出

```
🔍 Starting CI Monitor for ximicpp/XOffsetDatastructure (next_cpp26)
📝 Log file: ci-monitor.log
⏰ Started at: 2026-02-02 11:00:00

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🆕 NEW CI RUN DETECTED!
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📌 Run #69 (ID: 21575872338)
🕐 Created: 2026-02-02T03:05:00Z
📊 Status: in_progress
🔗 URL: https://github.com/ximicpp/XOffsetDatastructure/actions/runs/21575872338

[2026-02-02 11:30:00] 📝 Status changed: in_progress → completed
   
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🏁 CI WORKFLOW COMPLETED!
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📌 Run #69
📊 Conclusion: success
🔗 URL: https://github.com/ximicpp/XOffsetDatastructure/actions/runs/21575872338

✅ SUCCESS! All tests passed!

📋 Next steps:
   1. Review results: https://github.com/ximicpp/XOffsetDatastructure/actions/runs/21575872338
   2. Archive proposal: openspec archive add-ci-cd-docker-support --yes
```

---

## 故障排查

### 监控未启动
```bash
# 检查脚本权限
chmod +x scripts/*.sh

# 手动测试
./scripts/check-ci-status.sh next_cpp26 10 1
```

### 日志文件为空
```bash
# 等待几秒让监控启动
sleep 5

# 检查进程
ps aux | grep monitor-ci
```

### API 速率限制
GitHub API 有速率限制（未认证：60次/小时）。如果遇到限制：
- 增加检查间隔（如 60 秒改为 120 秒）
- 使用 GitHub Personal Access Token（需修改脚本）

---

## 当前状态

✅ 监控已启动并在后台运行
📌 监控分支: `next_cpp26`
📝 日志文件: `ci-monitor.log`

最新 CI 运行:
- Run #68: ❌ failure (2026-02-02T03:03:30Z)
- URL: https://github.com/ximicpp/XOffsetDatastructure/actions/runs/21575872337

⏳ 等待新的 CI 运行触发...
