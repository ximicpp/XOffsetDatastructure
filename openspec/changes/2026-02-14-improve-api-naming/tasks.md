# Tasks: Improve API Method Naming

## Phase 1: Core Header (xoffsetdatastructure.hpp)

### 1.1 XBuffer 序列化 API 重命名
- [ ] `save_to_string()` → `save()`
- [ ] `save_to_string_full()` → `save_raw()`
- [ ] `save_to_vector()` → `save_bytes()`
- [ ] `load_from_string(data)` → `load(const std::string&)`
- [ ] `load_from_vector(data)` → `load(const std::vector<char>&)`

### 1.2 XCompactor 方法重命名
- [ ] `compact_automatic<T>()` → `compact<T>()`

### 1.3 XBufferStats 方法重命名
- [ ] `get_memory_stats()` → `memory_stats()`
- [ ] `print_stats()` → `print()`
- [ ] 更新 `XBuffer::stats()` 内部对 `memory_stats()` 的调用

## Phase 2: Tests Update
- [ ] 2.1 替换所有测试文件中的 `save_to_string` → `save`
- [ ] 2.2 替换所有测试文件中的 `save_to_string_full` → `save_raw`
- [ ] 2.3 替换所有测试文件中的 `save_to_vector` → `save_bytes`
- [ ] 2.4 替换所有测试文件中的 `load_from_string` → `load`
- [ ] 2.5 替换所有测试文件中的 `load_from_vector` → `load`
- [ ] 2.6 替换所有测试文件中的 `compact_automatic` → `compact`
- [ ] 2.7 替换所有测试文件中的 `get_memory_stats` → `memory_stats`
- [ ] 2.8 替换所有测试文件中的 `print_stats` → `print`

## Phase 3: Examples Update
- [ ] 3.1 更新 `examples/demo.cpp`
- [ ] 3.2 更新 `examples/helloworld.cpp`

## Phase 4: Verification
- [ ] 4.1 本地构建
- [ ] 4.2 全部 25 测试通过
- [ ] 4.3 提交推送
