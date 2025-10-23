# WSL 构建工具和脚本

此文件夹包含所有 WSL 相关的构建脚本、测试文件和文档。

## 📁 文件结构

```
wsl/
├── README.md                           # 本文件
├── WSL2_QUICK_START.md                # WSL 快速开始指南
│
├── 环境设置脚本
│   ├── wsl_setup_tools.bat            # 安装基础工具
│   ├── wsl_build_clang_p2996.bat      # 构建 P2996 Clang 编译器
│   ├── wsl_check_environment.bat      # 检查环境
│   └── wsl_upgrade_cmake.bat          # 升级 CMake
│
├── 项目构建脚本
│   ├── build_cpp26_wsl.sh             # WSL 构建脚本（Bash）
│   ├── wsl_build_project.bat          # 构建项目（Windows入口）
│   └── wsl_rebuild_with_reflection.bat # 重新构建（启用反射）
│
├── 测试运行脚本
│   ├── wsl_run_tests.bat              # 交互式测试菜单
│   ├── wsl_run_demo.bat               # 运行完整演示
│   └── wsl_quick_validation.bat       # 快速验证
│
└── 反射测试文件
    ├── test_cpp26_simple.cpp          # 简单 C++26 测试
    ├── test_reflection_syntax.cpp     # 反射语法测试
    ├── test_reflection_final.cpp      # 完整反射测试
    ├── test_splice.cpp                # Splice 语法测试
    ├── test_reflect_syntax.cpp        # 反射语法验证
    └── test_meta_full.cpp             # 完整 Meta 测试
```

## 🚀 Quick Start

### First Time Setup

1. **Initialize environment** (one time only, ~30-60 minutes):
   ```cmd
   cd wsl
   wsl_setup_tools.bat
   wsl_build_clang_p2996.bat
   ```

2. **Build project**:
   ```cmd
   wsl_rebuild_with_reflection.bat
   ```

3. **Run tests**:
   ```cmd
   wsl_run_tests.bat
   ```

### Daily Usage

```cmd
# Enter wsl directory
cd wsl

# Rebuild
wsl_rebuild_with_reflection.bat

# Run tests
wsl_run_tests.bat

# Quick validation
wsl_quick_validation.bat
```

### Or Use Main Menu

```cmd
# From project root
wsl.bat

# Or from wsl directory
cd wsl
wsl.bat
```

## 📋 脚本说明

### 环境设置

| 脚本 | 用途 | 执行时间 |
|------|------|----------|
| `wsl_setup_tools.bat` | 安装必要工具（git, cmake, ninja等） | ~5 分钟 |
| `wsl_build_clang_p2996.bat` | 从源码构建 P2996 Clang | ~30-60 分钟 |
| `wsl_check_environment.bat` | 检查环境配置 | <1 分钟 |
| `wsl_upgrade_cmake.bat` | 升级 CMake 到最新版 | ~2 分钟 |

### 项目构建

| 脚本 | 用途 | 执行时间 |
|------|------|----------|
| `build_cpp26_wsl.sh` | Bash 构建脚本（WSL内部使用） | ~1-2 分钟 |
| `wsl_build_project.bat` | Windows 入口，调用 Bash 脚本 | ~1-2 分钟 |
| `wsl_rebuild_with_reflection.bat` | 清理并重新构建 | ~1-2 分钟 |

### 测试运行

| 脚本 | 用途 | 
|------|------|
| `wsl_run_tests.bat` | 交互式菜单，选择测试 |
| `wsl_run_demo.bat` | 直接运行完整演示 |
| `wsl_quick_validation.bat` | 快速验证编译器和环境 |

## 🔧 高级用法

### 手动使用 Bash 脚本

```bash
# 进入 WSL
wsl

# 进入项目目录
cd /mnt/g/workspace/XOffsetDatastructure

# 运行构建脚本
bash wsl/build_cpp26_wsl.sh

# 运行测试
cd build_cpp26/bin
export LD_LIBRARY_PATH=~/clang-p2996-install/lib
./xoffsetdatastructure2_demo
```

### 单独编译反射测试

```bash
# 进入 WSL
wsl

# 编译测试
~/clang-p2996-install/bin/clang++ \
    -std=c++26 \
    -freflection \
    -stdlib=libc++ \
    wsl/test_splice.cpp \
    -o test_splice \
    -L~/clang-p2996-install/lib \
    -Wl,-rpath,~/clang-p2996-install/lib

# 运行
LD_LIBRARY_PATH=~/clang-p2996-install/lib ./test_splice
```

## 📖 文档

- **[WSL2_QUICK_START.md](WSL2_QUICK_START.md)** - 完整的 WSL2 快速开始指南
- **[../COMPILE_AND_RUN.md](../COMPILE_AND_RUN.md)** - 编译和运行详细说明
- **[../RUNNING_TESTS.md](../RUNNING_TESTS.md)** - 测试运行指南

## 🛠️ Troubleshooting

### Issue: Script Not Found

**Error**: `The system cannot find the path specified`

**Solution**: Make sure to run from the correct directory:
```cmd
cd G:\workspace\XOffsetDatastructure
cd wsl
call wsl_run_tests.bat
```

### Issue: Encoding Problems (Chinese Characters)

**Error**: Garbled text or encoding errors

**Solution**: All scripts now use English only. See [ENCODING_FIX.md](ENCODING_FIX.md) for details.

### Issue: WSL Command Failed

**Error**: `bash: command not found`

**Solution**: Ensure WSL2 is installed and configured:
```cmd
wsl --list --verbose
wsl --set-default-version 2
```

### Issue: Compiler Not Found

**Error**: `Clang P2996 not found`

**Solution**: Build the compiler first:
```cmd
cd wsl
call wsl_build_clang_p2996.bat
```

## 💡 Best Practices

1. **First Time**: Execute environment setup scripts in order
2. **Daily Development**: Just run `wsl_rebuild_with_reflection.bat`
3. **Testing**: Use `wsl_run_tests.bat` interactive menu
4. **Quick Check**: Use `wsl_quick_validation.bat` to verify environment
5. **All scripts are now in English** to avoid encoding issues

## 🔗 相关链接

- [P2996 Clang 分支](https://github.com/bloomberg/clang-p2996)
- [C++26 反射提案](https://wg21.link/p2996)
- [项目主页](https://github.com/yourusername/XOffsetDatastructure)

---

## 📝 Important Notes

- **All batch files now use English** to avoid encoding issues
- **All WSL files are in the `wsl/` directory** for better organization
- **Use `wsl.bat` as the main entry point** for interactive menu
- **See [ENCODING_FIX.md](ENCODING_FIX.md)** for details on encoding fixes

**Tip**: If this is your first time, read [WSL2_QUICK_START.md](WSL2_QUICK_START.md) first
