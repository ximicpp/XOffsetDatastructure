# Running Examples in WSL

This guide shows you how to run XOffsetDatastructure2 examples and tests in WSL.

## 🚀 Quick Start

### Method 1: Interactive Menu (Easiest)

```cmd
# From project root
wsl.bat

# Then select option 3 to run tests
```

Or from wsl directory:

```cmd
cd wsl
wsl.bat
# Select option 3
```

---

### Method 2: Direct Script Execution

```cmd
cd wsl

# Run all tests
wsl_run_tests.bat

# Or run demo directly
wsl_run_demo.bat
```

---

### Method 3: Manual WSL Commands

```bash
# Enter WSL
wsl

# Navigate to project
cd /mnt/g/workspace/XOffsetDatastructure

# Set library path
export LD_LIBRARY_PATH=~/clang-p2996-install/lib

# Run examples
cd build_cpp26/bin

# Available examples:
./helloworld
./test_basic_types
./test_vector
./test_nested
./test_map_set
./test_modify
./test_compaction
./xoffsetdatastructure2_demo
```

---

## 📚 Available Examples

### 1. Hello World
**File**: `helloworld`
**Description**: Basic example showing serialization/deserialization

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/helloworld"
```

**What it does**:
- Creates a player object
- Adds items
- Serializes to binary
- Deserializes and verifies

---

### 2. Basic Types Test
**File**: `test_basic_types`
**Description**: Tests fundamental data types

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_basic_types"
```

**What it does**:
- Tests int32_t, int64_t
- Tests float, double
- Tests string handling
- Verifies serialization

---

### 3. Vector Container Test
**File**: `test_vector`
**Description**: Tests dynamic array operations

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_vector"
```

**What it does**:
- push_back operations
- Element access
- Iterator operations
- Persistence test

---

### 4. Nested Structures Test
**File**: `test_nested`
**Description**: Tests complex nested types

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_nested"
```

**What it does**:
- Nested struct serialization
- Deep hierarchy handling
- Cross-reference validation

---

### 5. Map/Set Container Test
**File**: `test_map_set`
**Description**: Tests associative containers

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_map_set"
```

**What it does**:
- XMap operations
- XSet operations
- Key-value pair handling

---

### 6. Modify Operations Test
**File**: `test_modify`
**Description**: Tests data modification

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_modify"
```

**What it does**:
- Update operations
- Delete operations
- In-place modifications

---

### 7. Memory Compaction Test
**File**: `test_compaction`
**Description**: Tests memory compaction

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/test_compaction"
```

**What it does**:
- Memory fragmentation handling
- Compaction algorithms
- Memory usage optimization

---

### 8. Full Demo (Recommended)
**File**: `xoffsetdatastructure2_demo`
**Description**: Comprehensive demonstration of all features

```bash
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26 && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./bin/xoffsetdatastructure2_demo"
```

**What it does**:
- All basic operations
- Container demonstrations
- Performance benchmarks
- Complete feature showcase

---

## 🎯 Step-by-Step Tutorial

### First Time Setup

1. **Build the project** (if not already built):
   ```cmd
   cd wsl
   wsl_rebuild_with_reflection.bat
   ```

2. **Verify build succeeded**:
   ```cmd
   dir ..\build_cpp26\bin
   ```
   
   You should see files like:
   - helloworld.exe
   - test_basic_types.exe
   - xoffsetdatastructure2_demo.exe
   - etc.

### Running Examples

#### Option A: Using the interactive menu

```cmd
cd wsl
wsl_run_tests.bat
```

Then select:
- `1` - Run all tests
- `2` - Hello World
- `3` - Basic types
- `9` - Full demo
- etc.

#### Option B: Direct execution

```cmd
cd wsl
wsl_run_demo.bat
```

#### Option C: Manual WSL command

```cmd
# Single command from Windows
wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26/bin && LD_LIBRARY_PATH=~/clang-p2996-install/lib ./xoffsetdatastructure2_demo"
```

---

## 🔍 Troubleshooting

### Issue: "No such file or directory"

**Problem**: Build directory doesn't exist

**Solution**:
```cmd
cd wsl
wsl_rebuild_with_reflection.bat
```

### Issue: "error while loading shared libraries"

**Problem**: LD_LIBRARY_PATH not set

**Solution**: Use the provided scripts which automatically set the library path:
```cmd
cd wsl
wsl_run_demo.bat
```

Or set it manually:
```bash
export LD_LIBRARY_PATH=~/clang-p2996-install/lib
```

### Issue: "command not found"

**Problem**: Not in correct directory

**Solution**:
```bash
wsl
cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26/bin
ls -la  # Verify files exist
```

---

## 📊 Example Output

### Hello World Output:
```
=== XOffsetDatastructure2 Hello World (C++26) ===

1. Creating buffer...
2. Creating player...
3. Adding items...
4. Player info:
   Name: Alice
   ID: 1, Level: 10
   Items: 101 102 103 

5. Serializing...
   Serialized size: 4096 bytes
6. Deserializing...
   Loaded player: Alice (Level 10)
   Items count: 3

=== Done! ===
```

### Demo Output (excerpt):
```
+====================================================================+
| XOffsetDatastructure2 - Comprehensive Demo                         |
+====================================================================+

+- Platform Requirements
  Architecture        : 64-bit only
  Byte Order          : Little-endian
  Compatibility       : x86-64, ARM64

+- Benchmarking Example
  Operations          : 1000 item insertions
  Time                : 323 us
  Avg per Item        : 0.323000 us

[PASS] All demonstrations completed successfully!
```

---

## 🎓 Learning Path

### Beginner
1. Start with `helloworld`
2. Try `test_basic_types`
3. Explore `test_vector`

### Intermediate
4. Run `test_nested`
5. Try `test_map_set`
6. Experiment with `test_modify`

### Advanced
7. Study `test_compaction`
8. Run full `xoffsetdatastructure2_demo`
9. Review source code in `examples/` and `tests/`

---

## 💻 Quick Reference

### From Windows Command Prompt

```cmd
# Run demo (easiest)
wsl\wsl_run_demo.bat

# Run test menu
wsl\wsl_run_tests.bat

# Rebuild and run
cd wsl
wsl_rebuild_with_reflection.bat
wsl_run_demo.bat
```

### From WSL Bash

```bash
# Enter WSL
wsl

# Navigate
cd /mnt/g/workspace/XOffsetDatastructure/build_cpp26/bin

# Set library path
export LD_LIBRARY_PATH=~/clang-p2996-install/lib

# Run any example
./helloworld
./test_vector
./xoffsetdatastructure2_demo
```

---

## 📝 Notes

- **All examples are already compiled** after running `wsl_rebuild_with_reflection.bat`
- **No additional build steps needed** for individual examples
- **Library path is automatically set** by the provided .bat scripts
- **Interactive menu is the easiest** way to explore examples

---

## 🔗 See Also

- [WSL Tools README](README.md) - Complete WSL tools documentation
- [WSL2 Quick Start](WSL2_QUICK_START.md) - Environment setup guide
- [Project Quick Start](../QUICKSTART.md) - Overall project guide

---

**Happy exploring!** 🎉
