# Examples

Quick-start examples for XOffsetDatastructure.

| File | Purpose |
|------|---------|
| `helloworld.cpp` | Minimal introduction — create, serialize, compact |
| `demo.cpp` | Comprehensive tour of all major features |
| `player.hpp` | Simple data structure (used by helloworld) |
| `game_data.hpp` | Complex nested data structure (used by demo) |

## Build & Run

Examples are built automatically by the project build script:

```bash
# From repository root
./build.sh

# Or in Docker (Apple Silicon: add --platform linux/amd64)
docker run --rm -v $(pwd):/workspace -w /workspace \
  ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```

After building, run the examples directly:

```bash
./build/bin/helloworld
./build/bin/xoffsetdatastructure_demo
```

## Start Here: helloworld.cpp

The hello world example walks through the core workflow in ~120 lines:

1. **Create** a buffer and allocate an object
2. **Populate** fields and containers (XString, XVector)
3. **Serialize** to binary and deserialize back
4. **Compact** memory using C++26 reflection
5. **Inspect** the compile-time type signature

```cpp
XBufferExt xbuf(4096);
auto* player = xbuf.make<Player>();
player->name = "Alice";
player->items.push_back(101);

// Serialize → deserialize (zero-encoding)
auto data = xbuf.save_to_string();
XBufferExt loaded = XBufferExt::load_from_string(data);
auto& p = loaded.root<Player>();
```

## Defining Your Own Types

All types stored in XBuffer must follow the **allocator-aware protocol**:

```cpp
class MyType {
public:
    using allocator_type = XAllocator;  // ← required

    // Allocator constructor (suffix mode)
    template <typename Allocator>
        requires (!std::is_same_v<std::decay_t<Allocator>, std::allocator_arg_t>)
    MyType(Allocator allocator) : name(allocator), items(allocator) {}

    // Move + allocator constructor (required for vector reallocation)
    template <typename Allocator>
    MyType(MyType&& other, Allocator allocator)
        : id(other.id)
        , name(std::move(other.name), allocator)
        , items(std::move(other.items), allocator)
    {}

    int32_t id{0};
    XString name;
    XVector<int32_t> items;
};
```

Key points:
- `allocator_type` typedef enables automatic allocator injection by `scoped_allocator_adaptor`
- The `requires` constraint prevents `allocator_arg_t` from matching the single-arg constructor
- The move+allocator constructor is needed when vectors reallocate their elements

See `player.hpp` and `game_data.hpp` for complete working examples.

## Buffer Sizing

How much space do you need? Use `estimate_buffer_size()`:

```cpp
// Estimate for ~500 bytes of user data
std::size_t size = XBufferExt::estimate_buffer_size(500);  // ~720 bytes
XBufferExt xbuf(size);
```

If unsure, start with 4096 (4KB). The buffer can be grown later with `grow()`.

**What happens when the buffer is full?** Container operations (`push_back`, `emplace`, etc.)
throw `boost::interprocess::bad_alloc`. Handle it by growing the buffer:

```cpp
try {
    data->items.push_back(42);
} catch (const boost::interprocess::bad_alloc&) {
    xbuf.grow(4096);
    data = &xbuf.root<MyType>();  // re-acquire after grow
    data->items.push_back(42);    // retry
}
```

## Critical Safety Rules

These rules apply to all code using XBufferExt. See the project README for full details.

### Pointer Invalidation

`grow()`, `shrink_to_fit()`, and `compact_automatic()` may relocate the buffer.
**All existing pointers become invalid.** Re-acquire through `root<T>()`,
or use `XHandle` to avoid manual re-acquisition entirely:

```cpp
auto* obj = xbuf.make<MyType>();
xbuf.grow(8192);
// ⚠ 'obj' is DANGLING here
obj = &xbuf.root<MyType>();  // ✅ manual re-acquire

// ✅ BETTER — XHandle auto-recovers after any buffer relocation
auto h = xbuf.handle<MyType>();
xbuf.grow(8192);
h->field = 42;  // safe — handle detects the epoch change
```

> `compact_automatic<T>()` returns a **new** `XBufferExt` (not the original buffer).
> Use the returned object directly — no wrapper needed:
> ```cpp
> XBufferExt compacted = XBufferCompactor::compact_automatic<MyType>(xbuf);
> auto& obj = compacted.root<MyType>();  // ✅ direct access
> ```

### Bulk Deallocation

There is no per-object `delete`. The buffer is a memory arena — all objects
are freed together when the buffer is destroyed.

### Thread Safety

XBufferExt is **not** thread-safe. Concurrent writes require external synchronization.
