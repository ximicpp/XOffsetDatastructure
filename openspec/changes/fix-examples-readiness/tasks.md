## 1. 数据结构统一
- [x] 1.1 player.hpp: 添加 allocator_type + allocator_arg_t 约束 + move+alloc 构造函数
- [x] 1.2 player.hpp: 移除未使用的 full constructor
- [x] 1.3 game_data.hpp: GameData 添加 allocator_type + move+alloc 构造函数

## 2. helloworld.cpp 修复
- [x] 2.1 F1: L50 拆分多语句，改用安全的 if 检查
- [x] 2.2 F2: L100 消除 XBUFFER_ROOT_NAME，改用 XBufferExt + root<T>()
- [x] 2.3 W1: 消除所有多语句单行合并

## 3. demo.cpp 修复
- [x] 3.1 F3: L182 消除 XBUFFER_ROOT_NAME，改用 root<T>()
- [x] 3.2 F4: L305 消除 XBUFFER_ROOT_NAME，改用 XBufferExt + root<T>()
- [x] 3.3 W2: demo_memory_management 展示 grow 后指针重获取

## 4. 验证
- [x] 4.1 全量构建测试通过
- [x] 4.2 确认 examples 中零 XBUFFER_ROOT_NAME 引用