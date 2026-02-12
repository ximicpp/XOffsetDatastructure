# XOffsetDatastructure

[![CI](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml)

### Introduction
XOffsetDatastructure is a serialization library designed to reduce or even eliminate the performance consumption of serialization and deserialization by utilizing zero-encoding and zero-decoding. It is also a collection of high-performance data structures designed for efficient read and in-place/non-in-place write, with performance comparable to STL.  

### CppCon 2024
[CppCon 2024: Using Modern C++ to Build XOffsetDatastructure: A Zero-Encoding and Zero-Decoding High-Performance Serialization Library](https://github.com/CppCon/CppCon2024/blob/main/Presentations/Using_Modern_Cpp_to_Build_XOffsetDatastructure.pdf)

### CppCon 2025
[CppCon 2025: Cross-platform XOffsetDatastructure: Ensuring Zero-encoding/Zero-decoding Serialization Compatibility Through Compile-time Type Signatures](https://github.com/ximicpp/XOffsetDatastructure/blob/main/docs/Compile-timeTypeSignatures.pdf)

### Formal Correctness Model

XOffsetDatastructure's zero-encoding approach is backed by a formal correctness
model that answers: **under what conditions is direct byte-copy semantically
equivalent to full serialization/deserialization?**

The answer: **two conditions, necessary and sufficient.**

| Condition | Role | Mechanism |
|-----------|------|-----------|
| **C1: Layout Determinism** | Value preservation — same bytes = same values | Architecture constraints + TypeLayout signature verification |
| **C2: Referential Integrity** | Reference preservation — all pointers survive copy | `offset_ptr` (relative addressing) + Safe Type Set |

```
C1 (value preservation) + C2 (reference preservation) = semantic equivalence
```

Both conditions operate within a **Safe Type Set S** (only fixed-width types,
no raw pointers, no virtual classes) and are verified at compile time. The full
formal model, including theorem, proof, and boundary conditions, is in
[`docs/CORE_FORMAL_MODEL.md`](docs/CORE_FORMAL_MODEL.md).

### Critical Safety Rules

> ⚠️ **Read this before writing any application code.** Violating these rules leads to silent data corruption.

#### Rule 1: Pointer Invalidation

**Any operation that resizes the buffer invalidates ALL existing pointers into it.**

```cpp
auto* player = buffer.make<Player>();

// ❌ DANGEROUS — pointer 'player' is now INVALID
buffer.grow(new_size);
player->level = 10;  // undefined behavior!

// ✅ CORRECT — re-acquire via root() after resize
buffer.grow(new_size);
auto& player = buffer.root<Player>();
player.level = 10;  // safe

// ✅ BEST — use XHandle for automatic safety
auto player = buffer.make_handle<Player>();
buffer.grow(new_size);
player->level = 10;  // handle auto re-finds, always safe
```

Operations that invalidate pointers:
| Operation | Invalidates All Pointers? |
|-----------|:------------------------:|
| `grow()` | ✅ Yes |
| `shrink_to_fit()` | ✅ Yes |
| `compact()` / `compact_automatic<T>()` | ✅ Yes |
| `make<T>()` | ❌ No (but may fail if full) |
| `root<T>()` / `has_root<T>()` | ❌ No |
| Read/write to existing objects | ❌ No |

#### Rule 2: Bulk Deallocation

**There is no per-object `delete`.** The buffer is a memory arena — all objects are freed together when the buffer is destroyed or reset.

```cpp
// ❌ NOT AVAILABLE — no individual deallocation
// buffer.deallocate(player);

// ✅ The buffer owns all memory; it is freed when the buffer goes out of scope
{
    XBufferExt buffer(4096);
    auto* p = buffer.make<Player>();
    p->name = "Alice";
    // ... use p ...
}  // ← all memory freed here, including Player and its XString/XVector contents
```

#### Rule 3: Thread Safety

**XBufferExt is NOT thread-safe.** Concurrent reads are safe, but any write (including container modifications like `push_back`) requires external synchronization.

```cpp
// ❌ DATA RACE — concurrent writes
std::thread t1([&]{ player->items.push_back(item1); });
std::thread t2([&]{ player->items.push_back(item2); });

// ✅ CORRECT — external mutex
std::mutex mtx;
std::thread t1([&]{ std::lock_guard lk(mtx); player->items.push_back(item1); });
std::thread t2([&]{ std::lock_guard lk(mtx); player->items.push_back(item2); });
```

#### Rule 4: Safe Type Constraints

Only types satisfying the **Safe Type Set** can be stored in the buffer. The compiler enforces this via `is_xbuffer_safe<T>`:

| ✅ Safe | ❌ Unsafe |
|---------|----------|
| `int`, `float`, `double` | `std::string` (uses heap pointers) |
| `XString`, `XVector<T>`, `XMap<K,V>`, `XSet<T>` | `std::vector<T>` (uses heap pointers) |
| Fixed-size arrays `T[N]` | `T*` (raw pointers) |
| Structs of the above | Classes with `virtual` functions |

```cpp
// Compile-time validation
static_assert(is_xbuffer_safe<Player>::value,
              "Player contains unsafe types for XBuffer");
```

#### Rule 5: Automatic Allocator Propagation

**Container operations automatically inject the buffer's allocator.** You never need to pass `get_segment_manager()` when using XVector, XMap, or XSet.

```cpp
XBufferExt xbuf(4096);
auto* data = xbuf.make<MyData>();

// ✅ Just use containers like STL — allocator is injected automatically
data->names.push_back("Alice");          // XVector<XString>
data->names.insert(it, "Bob");
data->scores.emplace("Alice", 95);       // XMap<XString, int>
data->scores["Bob"] = 88;
data->scores.erase("Alice");
data->tags.insert("vip");                // XSet<XString>
```

For **user-defined types** stored in containers, add `allocator_type` to enable automatic allocator injection:

```cpp
struct InnerObject {
    using allocator_type = XAllocator;   // ← enables automatic injection

    template <typename Allocator>
        requires (!std::is_same_v<std::decay_t<Allocator>, std::allocator_arg_t>)
    InnerObject(Allocator alloc) : data(alloc) {}

    // Move + allocator constructor (required for vector reallocation)
    template <typename Allocator>
    InnerObject(InnerObject&& other, Allocator alloc)
        : data(std::move(other.data), alloc) {}

    XVector<int> data;
};

// Now emplace_back() works without manually passing the allocator
XVector<InnerObject> vec(sm);
vec.emplace_back();  // allocator auto-injected ✅
```

### Type Signature System (powered by TypeLayout)

XOffsetDatastructure uses [TypeLayout](https://github.com/ximicpp/TypeLayout) as its type-signature engine. TypeLayout provides a two-layer compile-time signature system:

- **Definition Signatures** — full structural identity including field names, inheritance, and platform info
- **Layout Signatures** — pure byte-layout comparison for binary compatibility checks

```cpp
// Compile-time type safety: detect layout changes at build time
static_assert(boost::typelayout::get_definition_signature<Player>() ==
    "[64-le]record[s:72,a:8]{@0[id]:i32[s:4,a:4],@4[level]:i32[s:4,a:4],...}",
    "Binary layout changed! This breaks serialization compatibility.");

// Check binary compatibility between two types
static_assert(boost::typelayout::layout_signatures_match<StructA, StructB>());
```

See `docs/MIGRATION_TYPELAYOUT.md` for the full API reference and migration guide.

### Requirements

| Requirement | Status | Notes |
|-------------|--------|-------|
| **C++26 Reflection (P2996)** | ✅ Required | Library depends on `<experimental/meta>` |
| **Clang P2996 Fork** | ✅ Required | Standard compilers not supported |
| **64-bit Architecture** | ✅ Required | 32-bit not supported |
| **Little-endian** | ✅ Required | Big-endian not supported |

> ⚠️ **Non-Reflection Mode**: This library does **NOT** support a non-reflection fallback mode. The C++26 P2996 reflection feature is integral to the type signature system and cannot be disabled. Use the provided Docker image or build Clang P2996 manually.

### Build and Test

> **Important:** This project uses Git submodules. Always clone with `--recursive`:
> ```bash
> git clone --recursive https://github.com/ximicpp/XOffsetDatastructure.git
> ```
> If you already cloned without `--recursive`, run: `git submodule update --init --recursive`

#### Option 1: Docker (Recommended for CI and Quick Start)

**Requirements:** Docker installed on your system

**Quick Start:**
```bash
# Build Docker image (first time only, takes 1-3 hours)
./scripts/docker-build.sh

# Or use docker-compose
docker-compose build

# Run tests in container
docker run -it -v $(pwd):/workspace xoffset-clang-p2996:latest ./build.sh

# Or with docker-compose
docker-compose run --rm xoffset-dev ./build.sh

# Interactive development
docker-compose run --rm xoffset-dev bash
```

**Advantages:**
- No need to manually build Clang P2996
- Consistent environment across all platforms
- Same environment as CI
- Easy onboarding for new contributors

#### Option 2: WSL/Linux with Manual Clang P2996 Build

**Requirements:**
- WSL2 (for Windows) or native Linux
- 8+ GB RAM, 50+ GB free disk space
- 1-3 hours for Clang build

**Setup:**
```bash
# 1. Build Clang P2996 (one-time setup)
cd scripts
./build_clang_p2996_wsl.sh

# 2. Build and test the project
cd ..
./build.sh

# Build without reflection tests
./build.sh --no-reflection

# Debug build
./build.sh --debug
```

See `AGENTS.md` for detailed build instructions and troubleshooting.

### Development Workflows

**Docker Workflow:**
- Mount source code into container
- Edit code on host, build in container
- All dependencies pre-installed

**WSL Workflow:**
- Direct native builds
- Full control over compiler installation
- Potential for slightly faster builds

### PS:
Benchmark code and results: see the tag ["v1.0.0: CppCon 2024 Milestone Release (Latest)"](https://github.com/ximicpp/XOffsetDatastructure/releases/tag/v1.0.0)