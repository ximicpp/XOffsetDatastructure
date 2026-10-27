# 用户 schema 签名固定（K7）

`XOFFSET_PIN_SCHEMA` 把一个类型的 wire 布局固定成一段签名文本。任何编译器、任何
目标平台算出的布局和这段文本不一致时，**编译直接失败**，错误信息里给出这个编译器
算出的布局。收发两端（服务端、各平台客户端）的构建都包含同一份固定文件，就能在
构建阶段发现对象表示不一致，而不是等到运行时 `load_verified` 因 schema 指纹不同而
拒绝。

## 用法

1. 生成固定文本（只需在任意一个受支持的编译器上运行一次）：

   ```cpp
   #include "my_schema.hpp"
   #include <iostream>
   int main() {
       std::cout << XOffsetDatastructure::schema_pin<game::Player>("game::Player") << "\n";
   }
   ```

   输出形如：

   ```cpp
   XOFFSET_PIN_SCHEMA(game::Player, "record[s:96,a:8]{@0:u64,...}");
   ```

2. 把输出放进一个固定文件（例如 `my_schema_pins.hpp`），提交到代码库，并在每个
   收发数据的构建里至少包含一次（任意一个翻译单元即可）。

3. 有意修改 schema 时，重新生成并提交固定文件。评审时看固定文件的 diff，就能看到
   布局的变化。

## 说明

- 签名覆盖布局的全部内容：字段偏移、标量类型和宽度、容器头格式、对齐、数组长度、
  enum 的底层类型。它就是 wire 头里 schema 指纹的输入（指纹另外包含注册的 schema
  名称）。
- 宏可以放在命名空间作用域或函数体内。类型名里有逗号时，先用 `using` 起别名。
- 每个构建由它自己的编译器求值，交叉编译时就是目标平台的编译器，所以 Windows（MinGW）
  客户端的构建同样受约束。3.1.0 下 Android、iOS 还编不了本库，见
  [CLIENT_TARGETS.md](CLIENT_TARGETS.md)。
- 这是构建期检查，不替代运行时校验：`load_verified` 仍然完整校验每个 wire。
- CTest 里的 `schema_pin_mismatch` 测试确认不一致时确实编译失败。
