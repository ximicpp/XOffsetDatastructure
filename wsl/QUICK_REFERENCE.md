# 🚀 Quick Reference - Running Examples

## One-Line Commands

### Run Demo (Fastest)
```cmd
wsl\wsl_run_demo.bat
```

### Run All Examples
```cmd
wsl\wsl_run_all_examples.bat
```

### Interactive Menu
```cmd
wsl\wsl_run_tests.bat
```

---

## Individual Examples

| Command | Description |
|---------|-------------|
| `wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/helloworld"` | Hello World |
| `wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_basic_types"` | Basic Types |
| `wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_vector"` | Vector Test |
| `wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/xoffsetdatastructure2_demo"` | Full Demo |

---

## From WSL Terminal

```bash
wsl
cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26/bin
export LD_LIBRARY_PATH=~/clang-p2996-install/lib

# Run any example
./helloworld
./test_vector
./xoffsetdatastructure2_demo
```

---

## Available Scripts

| Script | Purpose |
|--------|---------|
| `wsl_run_demo.bat` | Run full demo |
| `wsl_run_tests.bat` | Interactive menu |
| `wsl_run_all_examples.bat` | Run all examples sequentially |
| `wsl_rebuild_with_reflection.bat` | Rebuild project |

---

See [RUNNING_EXAMPLES.md](RUNNING_EXAMPLES.md) for detailed guide.
