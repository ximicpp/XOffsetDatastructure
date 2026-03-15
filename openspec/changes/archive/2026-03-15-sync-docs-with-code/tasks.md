## 1. docs/QUICK_REFERENCE.md — 测试矩阵重写

- [x] 1.1 替换测试矩阵部分（第 145-185 行附近），用与 `tests/README.md` 一致的 27 个测试列表（7 基础 + 20 反射）
- [x] 1.2 修正第 147 行 "总测试: 30个" → "总测试: 27个"
- [x] 1.3 修正第 156 行 "反射测试 (23个)" → "反射测试 (20个)"
- [x] 1.4 修正第 183-184 行 "Tests Run: 30 / Tests Passed: 30" → "Tests Run: 27 / Tests Passed: 27"
- [x] 1.5 修正第 269 行 "本地测试运行 5-10分钟 18个测试" → "5-10分钟 27个测试"
- [x] 1.6 修正第 287 行检查清单 "18个测试全部通过" → "27个测试全部通过"

## 2. docs/README.md — 测试数量修正

- [x] 2.1 修正第 82 行 "18个测试 = 6个基础测试 + 12个反射测试" → "27个测试 = 7个基础测试 + 20个反射测试"
- [x] 2.2 修正第 84-86 行 "Tests Run: 18 / Tests Passed: 18" → "Tests Run: 27 / Tests Passed: 27"

## 3. docs/ZERO_BOILERPLATE.md — 测试数量修正

- [x] 3.1 修正第 257 行 "30 tests total" → "27 tests total"

## 4. README.md（根目录）— Docker 命令修正

- [x] 4.1 修正第 266 行 Docker run 命令：`./build.sh` → `bash ./build.sh`
- [x] 4.2 修正第 269 行 docker-compose 命令：`./build.sh` → `bash ./build.sh`

## 5. 验证

- [x] 5.1 使用 grep 搜索所有 docs/ 中 "18 个测试" / "30 个" / "30个" 残留，确认无遗漏（额外发现并修正了 BUILD_AND_TEST_GUIDE.md 中的 7 处过时内容）
