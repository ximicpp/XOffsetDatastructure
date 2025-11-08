# C++ 中如何表示 JSON 数据

## 核心挑战

JSON 的两大特点在 C++ 中的挑战:

```json
{
  "name": "Alice",           // 字符串
  "age": 30,                 // 数字
  "active": true,            // 布尔
  "scores": [95, 87, 92],    // 数组
  "address": {               // 嵌套对象
    "city": "Beijing"
  },
  "metadata": null           // 空值
}
```

**挑战1: 动态字段**
- JSON可以随时添加/删除字段
- C++结构体字段在编译期固定

**挑战2: 异构类型**
- 同一个JSON对象中，不同字段类型不同
- C++容器通常要求元素类型统一

---

## 解决方案总览

| 方案 | 核心技术 | 优点 | 缺点 | 使用场景 |
|------|---------|------|------|---------|
| **1. std::variant** | C++17类型联合 | 类型安全 | 手动实现 | 轻量级需求 |
| **2. nlohmann/json** | 库 | 简单易用 | 性能一般 | 通用首选 ⭐ |
| **3. RapidJSON** | 库 | 极速 | API复杂 | 高性能场景 |
| **4. simdjson** | SIMD加速 | 超快解析 | 只读 | 大文件解析 |
| **5. Boost.PropertyTree** | Boost库 | 功能丰富 | 笨重 | 已有Boost项目 |
| **6. 自定义 (本项目可能方案)** | XBuffer + variant | 内存可控 | 需要开发 | 特殊需求 |

---

## 方案1: std::variant (C++17原生方案)

### 原理

```cpp
#include <variant>
#include <string>
#include <map>
#include <vector>

// 前向声明
struct JsonValue;

// JSON值的6种可能类型
using JsonNull = std::monostate;
using JsonBool = bool;
using JsonNumber = double;
using JsonString = std::string;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

// 递归variant (C++17支持)
struct JsonValue {
    std::variant<
        JsonNull,
        JsonBool,
        JsonNumber,
        JsonString,
        JsonArray,
        JsonObject
    > value;
    
    // 构造函数
    JsonValue() : value(JsonNull{}) {}
    JsonValue(bool b) : value(b) {}
    JsonValue(double n) : value(n) {}
    JsonValue(const char* s) : value(std::string(s)) {}
    JsonValue(std::string s) : value(std::move(s)) {}
    
    // 类型检查
    bool is_null() const { return std::holds_alternative<JsonNull>(value); }
    bool is_bool() const { return std::holds_alternative<JsonBool>(value); }
    bool is_number() const { return std::holds_alternative<JsonNumber>(value); }
    bool is_string() const { return std::holds_alternative<JsonString>(value); }
    bool is_array() const { return std::holds_alternative<JsonArray>(value); }
    bool is_object() const { return std::holds_alternative<JsonObject>(value); }
    
    // 类型获取
    bool& as_bool() { return std::get<JsonBool>(value); }
    double& as_number() { return std::get<JsonNumber>(value); }
    std::string& as_string() { return std::get<JsonString>(value); }
    JsonArray& as_array() { return std::get<JsonArray>(value); }
    JsonObject& as_object() { return std::get<JsonObject>(value); }
    
    // 数组操作
    void push_back(JsonValue v) {
        if (!is_array()) {
            value = JsonArray{};
        }
        as_array().push_back(std::move(v));
    }
    
    // 对象操作
    JsonValue& operator[](const std::string& key) {
        if (!is_object()) {
            value = JsonObject{};
        }
        return as_object()[key];
    }
};
```

### 使用示例

```cpp
int main() {
    // 创建JSON对象
    JsonValue json;
    json["name"] = "Alice";
    json["age"] = 30.0;
    json["active"] = true;
    
    // 创建数组
    JsonValue scores;
    scores.push_back(95.0);
    scores.push_back(87.0);
    scores.push_back(92.0);
    json["scores"] = scores;
    
    // 嵌套对象
    json["address"]["city"] = "Beijing";
    json["address"]["zip"] = "100000";
    
    // 访问数据
    std::cout << json["name"].as_string() << std::endl;  // "Alice"
    std::cout << json["age"].as_number() << std::endl;   // 30
    
    // 遍历数组
    for (auto& score : json["scores"].as_array()) {
        std::cout << score.as_number() << " ";  // 95 87 92
    }
    
    // 遍历对象
    for (auto& [key, val] : json["address"].as_object()) {
        std::cout << key << ": " << val.as_string() << std::endl;
    }
    
    return 0;
}
```

### 特点

✅ **优点:**
- 纯C++17标准库，无依赖
- 类型安全 (编译期检查)
- 完全控制内存布局

❌ **缺点:**
- 需要手动实现序列化/反序列化
- API不如专业库友好
- 性能一般 (variant有额外开销)

📊 **性能数据:**
```
内存占用: sizeof(JsonValue) = 40-48字节 (variant开销)
访问速度: ★★★☆☆ (需要类型检查)
序列化:   需要手动实现
```

---

## 方案2: nlohmann/json (最流行) ⭐

### 原理

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;
```

**内部实现核心:**
```cpp
// 简化版内部结构
class basic_json {
    enum class value_t {
        null, boolean, number_integer, number_unsigned,
        number_float, string, array, object
    };
    
    union json_value {
        object_t*   object;
        array_t*    array;
        string_t*   string;
        boolean_t   boolean;
        number_integer_t number_integer;
        number_unsigned_t number_unsigned;
        number_float_t number_float;
    };
    
    value_t m_type;         // 当前类型标记
    json_value m_value;     // 联合体存储值
};
```

**关键技术:**
1. **类型擦除 (Type Erasure)**: 用`union`存储不同类型
2. **运行时类型标记**: `m_type`记录当前实际类型
3. **智能指针管理**: 复杂类型用指针+引用计数

### 使用示例

```cpp
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

int main() {
    // 1. 直接构造
    json j = {
        {"name", "Alice"},
        {"age", 30},
        {"active", true},
        {"scores", {95, 87, 92}},
        {"address", {
            {"city", "Beijing"},
            {"zip", "100000"}
        }},
        {"metadata", nullptr}
    };
    
    // 2. 动态添加字段
    j["email"] = "alice@example.com";
    j["tags"] = json::array({"cpp", "json", "programming"});
    
    // 3. 访问数据 (自动类型转换)
    std::string name = j["name"];              // "Alice"
    int age = j["age"];                        // 30
    bool active = j["active"];                 // true
    
    // 4. 数组操作
    j["scores"].push_back(88);
    for (auto& score : j["scores"]) {
        std::cout << score << " ";             // 95 87 92 88
    }
    
    // 5. 对象遍历
    for (auto& [key, val] : j["address"].items()) {
        std::cout << key << ": " << val << std::endl;
    }
    
    // 6. 类型检查
    if (j["age"].is_number()) {
        std::cout << "Age is a number" << std::endl;
    }
    
    // 7. 序列化
    std::string json_str = j.dump();           // 紧凑格式
    std::string pretty = j.dump(4);            // 缩进4空格
    
    // 8. 反序列化
    json j2 = json::parse(R"({"key": "value"})");
    
    // 9. 从文件读写
    std::ofstream o("output.json");
    o << j << std::endl;
    
    std::ifstream i("input.json");
    json j3;
    i >> j3;
    
    // 10. 异常安全访问
    std::string city = j.value("city", "Unknown");  // 默认值
    
    // 11. 路径访问 (JSON Pointer)
    j["/address/city"_json_pointer] = "Shanghai";
    
    // 12. 合并
    json j4 = {{"new_field", 123}};
    j.merge_patch(j4);
    
    return 0;
}
```

### 高级特性

#### 1. **自定义类型映射**

```cpp
struct Person {
    std::string name;
    int age;
    std::vector<int> scores;
};

// 定义转换规则
namespace nlohmann {
    template <>
    struct adl_serializer<Person> {
        static void to_json(json& j, const Person& p) {
            j = json{
                {"name", p.name},
                {"age", p.age},
                {"scores", p.scores}
            };
        }
        
        static void from_json(const json& j, Person& p) {
            j.at("name").get_to(p.name);
            j.at("age").get_to(p.age);
            j.at("scores").get_to(p.scores);
        }
    };
}

// 使用
Person p{"Alice", 30, {95, 87, 92}};
json j = p;                         // 自动序列化
Person p2 = j.get<Person>();        // 自动反序列化
```

#### 2. **宏简化定义**

```cpp
struct Person {
    std::string name;
    int age;
    std::vector<int> scores;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Person, name, age, scores)
};

// 自动支持序列化/反序列化
Person p{"Alice", 30, {95, 87}};
json j = p;
```

#### 3. **BSON/CBOR/MessagePack支持**

```cpp
// JSON → BSON (二进制)
std::vector<uint8_t> bson = json::to_bson(j);

// BSON → JSON
json j2 = json::from_bson(bson);

// 同样支持 CBOR, MessagePack, UBJSON
auto cbor = json::to_cbor(j);
auto msgpack = json::to_msgpack(j);
```

### 特点

✅ **优点:**
- **API极简**: 类似JavaScript操作
- **功能全面**: 序列化/反序列化/类型转换全包
- **STL友好**: 无缝集成std容器
- **Header-only**: 单文件引入
- **异常安全**: 完善的错误处理
- **社区活跃**: GitHub 40k+ stars

❌ **缺点:**
- **性能一般**: 比RapidJSON慢2-3倍
- **编译慢**: Header-only导致编译时间长
- **内存开销**: 每个值约32-40字节
- **不支持注释**: 标准JSON无注释

📊 **性能数据:**
```
解析速度:   ★★★☆☆ (约 200 MB/s)
序列化速度: ★★★☆☆
内存占用:   ★★☆☆☆ (每个值 32-40字节)
易用性:     ★★★★★
```

### 安装使用

```bash
# 方法1: 单头文件
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp

# 方法2: CMake
find_package(nlohmann_json REQUIRED)
target_link_libraries(your_target nlohmann_json::nlohmann_json)

# 方法3: vcpkg
vcpkg install nlohmann-json
```

---

## 方案3: RapidJSON (极速性能)

### 原理

**SAX解析器 + DOM双模式:**

```cpp
// DOM模式 (类似nlohmann/json)
#include "rapidjson/document.h"
using namespace rapidjson;

Document d;
d.Parse(R"({"name": "Alice", "age": 30})");

// SAX模式 (流式解析, 零拷贝)
#include "rapidjson/reader.h"
struct MyHandler {
    bool String(const char* str, SizeType length, bool copy) {
        // 直接处理字符串, 无需拷贝!
        return true;
    }
};
```

**关键优化:**
1. **就地解析 (In-situ)**: 直接修改输入缓冲区, 零拷贝
2. **SIMD加速**: 使用SSE2/SSE4.2加速字符串扫描
3. **内存池**: 自定义分配器减少malloc调用

### 使用示例

```cpp
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <iostream>

using namespace rapidjson;

int main() {
    // 1. 解析JSON
    const char* json_str = R"({
        "name": "Alice",
        "age": 30,
        "scores": [95, 87, 92]
    })";
    
    Document d;
    d.Parse(json_str);
    
    // 2. 访问数据
    assert(d.IsObject());
    assert(d["name"].IsString());
    std::cout << d["name"].GetString() << std::endl;  // "Alice"
    std::cout << d["age"].GetInt() << std::endl;      // 30
    
    // 3. 遍历数组
    const Value& scores = d["scores"];
    assert(scores.IsArray());
    for (SizeType i = 0; i < scores.Size(); i++) {
        std::cout << scores[i].GetInt() << " ";       // 95 87 92
    }
    
    // 4. 动态添加字段 (需要Allocator)
    Document::AllocatorType& allocator = d.GetAllocator();
    
    // 添加字符串
    Value key("email", allocator);
    Value val("alice@example.com", allocator);
    d.AddMember(key, val, allocator);
    
    // 添加数组
    Value arr(kArrayType);
    arr.PushBack(1, allocator);
    arr.PushBack(2, allocator);
    d.AddMember("tags", arr, allocator);
    
    // 5. 序列化
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
    std::cout << buffer.GetString() << std::endl;
    
    // 6. 就地解析 (零拷贝, 更快!)
    char mutable_json[] = R"({"key": "value"})";
    d.ParseInsitu(mutable_json);  // 直接修改mutable_json!
    
    return 0;
}
```

### SAX解析示例 (超高性能)

```cpp
#include "rapidjson/reader.h"
#include <iostream>

using namespace rapidjson;

// 自定义处理器
struct StatsHandler {
    int null_count = 0;
    int bool_count = 0;
    int number_count = 0;
    int string_count = 0;
    
    bool Null() { null_count++; return true; }
    bool Bool(bool b) { bool_count++; return true; }
    bool Int(int i) { number_count++; return true; }
    bool Uint(unsigned u) { number_count++; return true; }
    bool Int64(int64_t i) { number_count++; return true; }
    bool Uint64(uint64_t u) { number_count++; return true; }
    bool Double(double d) { number_count++; return true; }
    bool String(const char* str, SizeType length, bool copy) {
        string_count++;
        std::cout << "Found string: " << str << std::endl;
        return true;
    }
    bool StartObject() { return true; }
    bool Key(const char* str, SizeType length, bool copy) { return true; }
    bool EndObject(SizeType memberCount) { return true; }
    bool StartArray() { return true; }
    bool EndArray(SizeType elementCount) { return true; }
};

int main() {
    const char* json = R"({"name":"Alice","age":30,"active":true,"data":null})";
    
    StatsHandler handler;
    Reader reader;
    StringStream ss(json);
    
    reader.Parse(ss, handler);  // 流式解析, 超快!
    
    std::cout << "Nulls: " << handler.null_count << std::endl;      // 1
    std::cout << "Bools: " << handler.bool_count << std::endl;      // 1
    std::cout << "Numbers: " << handler.number_count << std::endl;  // 1
    std::cout << "Strings: " << handler.string_count << std::endl;  // 1
    
    return 0;
}
```

### 特点

✅ **优点:**
- **极速**: 比nlohmann/json快3-5倍
- **内存高效**: 零拷贝模式
- **SAX支持**: 流式解析大文件
- **SIMD优化**: 硬件加速
- **跨平台**: 支持所有主流平台

❌ **缺点:**
- **API复杂**: 需要手动管理Allocator
- **不友好**: 没有nlohmann/json简洁
- **C++11**: 不支持现代C++特性

📊 **性能数据:**
```
解析速度:   ★★★★★ (约 1000 MB/s)
序列化速度: ★★★★★
内存占用:   ★★★★☆ (每个值 16-24字节)
易用性:     ★★★☆☆
```

---

## 方案4: simdjson (超级解析器)

### 原理

**SIMD并行解析:**

```
传统解析器:  逐字符扫描 "[1,2,3]"
             ↓ ↓ ↓ ↓ ↓ ↓ ↓

simdjson:    一次处理64字节!
             ████████████████
             
使用AVX2/AVX512指令并行处理
```

**关键创新:**
1. **两阶段解析**: 第一阶段找结构, 第二阶段提取值
2. **SIMD加速**: AVX2处理速度是标量的8-16倍
3. **On-Demand API**: 按需解析, 不访问不解析

### 使用示例

```cpp
#include "simdjson.h"
#include <iostream>

using namespace simdjson;

int main() {
    // 1. 基本解析
    ondemand::parser parser;
    padded_string json = R"({
        "name": "Alice",
        "age": 30,
        "scores": [95, 87, 92]
    })"_padded_string;
    
    ondemand::document doc = parser.iterate(json);
    
    // 2. 访问数据
    std::string_view name = doc["name"];
    uint64_t age = doc["age"];
    std::cout << name << ", " << age << std::endl;
    
    // 3. 遍历数组
    for (int64_t score : doc["scores"]) {
        std::cout << score << " ";
    }
    
    // 4. 嵌套访问
    std::string_view city = doc["address"]["city"];
    
    // 5. 解析文件 (超快!)
    auto result = parser.load_many("large.json");
    for (auto doc : result) {
        // 处理每个文档
    }
    
    return 0;
}
```

### 特点

✅ **优点:**
- **超快**: 比RapidJSON还快2-3倍 (GB/s级别)
- **简单API**: 类似nlohmann/json
- **大文件友好**: 支持流式解析
- **On-Demand**: 不访问不解析

❌ **缺点:**
- **只读**: 不支持修改/生成JSON
- **需要padding**: 输入末尾需要额外64字节
- **x86专用**: 依赖SIMD指令 (ARM支持有限)

📊 **性能数据:**
```
解析速度:   ★★★★★ (约 2-3 GB/s!)
序列化速度: ❌ (不支持)
内存占用:   ★★★★★
易用性:     ★★★★☆
```

---

## 方案5: Boost.PropertyTree

### 使用示例

```cpp
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>

using boost::property_tree::ptree;

int main() {
    // 1. 构造
    ptree pt;
    pt.put("name", "Alice");
    pt.put("age", 30);
    pt.put("address.city", "Beijing");
    
    // 2. 数组 (比较麻烦)
    ptree scores;
    ptree score1, score2, score3;
    score1.put("", 95);
    score2.put("", 87);
    score3.put("", 92);
    scores.push_back(std::make_pair("", score1));
    scores.push_back(std::make_pair("", score2));
    scores.push_back(std::make_pair("", score3));
    pt.add_child("scores", scores);
    
    // 3. 访问
    std::string name = pt.get<std::string>("name");
    int age = pt.get<int>("age");
    
    // 4. 序列化
    boost::property_tree::write_json(std::cout, pt);
    
    // 5. 反序列化
    ptree pt2;
    std::istringstream iss(R"({"key": "value"})");
    boost::property_tree::read_json(iss, pt2);
    
    return 0;
}
```

### 特点

✅ **优点:**
- Boost生态的一部分
- 支持XML/INI/INFO多格式

❌ **缺点:**
- **数组难用**: 不是为JSON设计
- **性能差**: 比专业JSON库慢
- **笨重**: 需要整个Boost

📊 **性能数据:**
```
解析速度:   ★★☆☆☆
序列化速度: ★★☆☆☆
易用性:     ★★☆☆☆
```

---

## 方案6: 本项目可能的实现 (XDS + Variant)

### 设计思路

结合 XDS 的统一内存管理 + variant 的类型安全:

```cpp
#include "xoffsetdatastructure2.hpp"
#include <variant>

// 定义JSON值类型
struct XJsonValue;

using XJsonNull = std::monostate;
using XJsonBool = bool;
using XJsonNumber = double;
using XJsonString = XString;  // XDS的XString
using XJsonArray = XVector<XJsonValue>;  // XDS的XVector
using XJsonObject = XMap<XString, XJsonValue>;  // XDS的XMap

struct XJsonValue {
    std::variant<
        XJsonNull,
        XJsonBool,
        XJsonNumber,
        XJsonString,
        XJsonArray,
        XJsonObject
    > value;
    
    // 所有数据存储在同一个XBuffer中!
};

// 使用
XBuffer xbuf(1024 * 1024);  // 1MB缓冲区
auto* json = xbuf.construct<XJsonValue>("root")();
// 所有嵌套数据都在同一个xbuf中管理!
```

### 优势

✅ **统一内存**: 整个JSON树在一个XBuffer中
✅ **类型安全**: variant提供编译期检查
✅ **可序列化**: XBuffer可直接持久化
✅ **零散分配少**: 减少malloc次数

### 挑战

❌ **需要实现**: 序列化/反序列化逻辑
❌ **递归限制**: variant递归需要特殊处理
❌ **API设计**: 需要友好的接口

---

## 性能对比总结

### 解析速度 (相同100MB JSON文件)

```
simdjson:        0.05秒  ██████████████████████████████████████████████████
RapidJSON:       0.10秒  █████████████████████████
nlohmann/json:   0.30秒  ████████
Boost.PropertyTree: 1.2秒 ██
std::variant:    需要手动实现
```

### 内存占用 (每个JSON值)

```
std::variant:    40-48字节  (variant overhead)
nlohmann/json:   32-40字节  (内部union + 类型标记)
RapidJSON:       16-24字节  (紧凑设计)
simdjson:        按需解析, 几乎零拷贝
```

### 易用性排名

```
1. nlohmann/json    ★★★★★  (最简单)
2. simdjson         ★★★★☆  (简单但只读)
3. std::variant     ★★★☆☆  (需要手动封装)
4. RapidJSON        ★★★☆☆  (API复杂)
5. Boost.PropertyTree ★★☆☆☆ (数组难用)
```

---

## 选型建议

### 快速决策树

```
需要修改JSON?
├─ 否 (只读解析)
│   └─ 极致性能? → simdjson ⭐
│       └─ 否 → nlohmann/json
│
└─ 是 (读写都需要)
    ├─ 性能关键? → RapidJSON
    ├─ 简单优先? → nlohmann/json ⭐⭐⭐
    ├─ 已有Boost? → Boost.PropertyTree
    └─ 特殊需求? → 自定义 (XDS + variant)
```

### 典型场景

| 场景 | 推荐方案 | 原因 |
|------|---------|------|
| **Web API客户端** | nlohmann/json | 简单、够用 |
| **配置文件** | nlohmann/json | 易读易写 |
| **大文件解析 (GB级)** | simdjson | 超快、低内存 |
| **游戏网络协议** | RapidJSON | 高性能 |
| **日志分析** | simdjson | 只读、超快 |
| **嵌入式系统** | 自定义 / RapidJSON | 内存可控 |
| **原型开发** | nlohmann/json | 快速上手 |

---

## 核心技术原理总结

### 1. 异构类型存储

**方案1: Union (手动管理)**
```cpp
union Value {
    bool b;
    double n;
    std::string* s;  // 复杂类型用指针
};
enum Type { BOOL, NUMBER, STRING };
Type type_tag;  // 手动记录类型
```

**方案2: std::variant (类型安全)**
```cpp
std::variant<bool, double, std::string> value;
// 编译期类型检查, 自动析构
```

### 2. 动态字段存储

**std::map / std::unordered_map**
```cpp
std::map<std::string, JsonValue> object;
// 支持动态添加/删除字段
```

### 3. 递归数据结构

**前向声明 + 指针/variant**
```cpp
struct JsonValue;  // 前向声明
using JsonArray = std::vector<JsonValue>;  // 递归!
```

### 4. 零拷贝优化

**string_view + 就地解析**
```cpp
// 不拷贝字符串, 直接指向原始缓冲区
std::string_view str = parse_string(buffer);
```

---

## 推荐方案

### 🥇 通用首选: nlohmann/json

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 3行代码搞定
json j = {{"name", "Alice"}, {"age", 30}};
std::cout << j.dump(4) << std::endl;
```

**理由:**
- API最简单
- 功能最全面
- 社区最活跃
- 性能够用 (90%场景)

### 🥈 高性能: RapidJSON

```cpp
#include "rapidjson/document.h"
Document d;
d.Parse(json_str);
```

**理由:**
- 解析速度快3-5倍
- 内存占用更小
- 适合性能敏感场景

### 🥉 超级解析: simdjson

```cpp
#include "simdjson.h"
ondemand::parser parser;
auto doc = parser.iterate(json);
```

**理由:**
- GB/s级解析速度
- 适合大文件、日志分析
- 只读场景完美

---

## 完整示例对比

### nlohmann/json (最推荐)

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

int main() {
    // 创建
    json j = {
        {"name", "Alice"},
        {"age", 30},
        {"scores", {95, 87, 92}}
    };
    
    // 动态添加
    j["email"] = "alice@example.com";
    
    // 访问
    std::string name = j["name"];
    
    // 序列化
    std::string str = j.dump();
    
    // 反序列化
    json j2 = json::parse(str);
    
    return 0;
}
```

### RapidJSON (高性能)

```cpp
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

int main() {
    Document d;
    d.SetObject();
    auto& a = d.GetAllocator();
    
    // 创建
    d.AddMember("name", "Alice", a);
    d.AddMember("age", 30, a);
    
    // 访问
    std::string name = d["name"].GetString();
    
    // 序列化
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
    
    return 0;
}
```

---

**总结: 99%的情况用 nlohmann/json 就够了!**
