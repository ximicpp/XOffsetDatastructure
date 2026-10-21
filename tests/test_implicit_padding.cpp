// M6 (threat model T5/P3): has_implicit_padding_v finds bytes no field covers,
// in the type itself or anywhere a wire can reach from it; and the library
// zeroes the padding of every container element it builds or moves, so a
// wire carries neither stale arena bytes nor the caller's stack through it.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace xo = XOffsetDatastructure;

namespace pad {
struct Dense { std::uint32_t a{}; std::uint32_t b{}; };
struct Interior { std::uint8_t tag{}; std::uint64_t value{}; };     // 7 bytes after tag
struct Tail { std::uint64_t value{}; std::uint8_t tag{}; };         // 7 bytes at the end
struct Explicit { std::uint8_t tag{}; std::uint8_t reserved[7]{}; std::uint64_t value{}; };
struct HoldsInterior { std::uint64_t id{}; Interior inner; };
struct Strings { xo::XString name; xo::XVector<std::uint32_t> ids; };
enum class Kind : std::uint8_t { a, b };
struct EnumPair { Kind kind{}; Kind other{}; std::uint16_t n{}; };

struct DenseRoot {
    Dense d;
    Explicit e;
    Strings s;
    EnumPair k;
    std::uint16_t arr[6]{};                      // ends at 72, where the 8-aligned map head starts
    xo::XMap<std::uint64_t, Dense> by_id;
    xo::XSet<std::uint32_t> set;
};
struct VectorOfPadded { xo::XVector<Interior> items; };
struct MapWithPaddedPair { xo::XMap<std::uint8_t, std::uint64_t> m; };   // XKeyValue<u8,u64>
struct ArrayOfPadded { Tail items[2]; };
struct BigArray { std::uint32_t n{}; Interior items[4096]; };         // one element reflected, not 4096
struct Grid { ArrayOfPadded rows[64][3]; };                            // array of records holding arrays
} // namespace pad

// A large array costs one element's ranges, repeated (compile-time bound).
static_assert(xo::detail::padding_ranges<pad::Interior[4096]>.size() == 1);
static_assert(xo::detail::padding_ranges<pad::Interior[4096]>[0].count == 4096);
static_assert(xo::detail::padding_ranges<pad::Interior[4096]>[0].stride == sizeof(pad::Interior));
static_assert(xo::has_implicit_padding_v<pad::BigArray>);

static_assert(!xo::has_implicit_padding_v<pad::Dense>);
static_assert(xo::has_implicit_padding_v<pad::Interior>);
static_assert(xo::has_implicit_padding_v<pad::Tail>);
static_assert(!xo::has_implicit_padding_v<pad::Explicit>);
static_assert(xo::has_implicit_padding_v<pad::HoldsInterior>);
static_assert(!xo::has_implicit_padding_v<pad::Strings>);
static_assert(!xo::has_implicit_padding_v<pad::EnumPair>);
static_assert(!xo::has_implicit_padding_v<pad::DenseRoot>);
static_assert(xo::has_implicit_padding_v<pad::VectorOfPadded>);
static_assert(xo::has_implicit_padding_v<pad::MapWithPaddedPair>);
static_assert(xo::has_implicit_padding_v<pad::ArrayOfPadded>);
static_assert(!xo::has_implicit_padding_v<std::uint64_t[3]>);

XOFFSET_REQUIRE_NO_PADDING(pad::DenseRoot);
XOFFSET_REQUIRE_NO_PADDING(pad::Explicit);

namespace padrt {
struct Entry { std::uint8_t tag{}; std::uint64_t value{}; };          // 7 padding bytes
struct WithString { std::uint16_t id{}; xo::XString name; };           // 2 padding bytes
struct Root {
    xo::XVector<Entry> entries;
    xo::XMap<std::int32_t, std::int64_t> resources;                    // 4 bytes per entry
    xo::XVector<WithString> named;
    xo::XVector<std::uint64_t> scratch;
};
} // namespace padrt

XOFFSET_REGISTER_SCHEMA_NAME(padrt::Root, "test.implicit_padding.root")

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_implicit_padding FAILED: " << message << "\n";
    std::exit(1);
}

template <class T>
void expect_zero_padding(const T& object, const char* what) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&object);
    for (const auto& r : xo::detail::padding_ranges<T>)
        for (std::size_t k = 0; k < r.count; ++k)
            for (std::size_t i = 0; i < r.bytes; ++i)
                if (bytes[r.offset + k * r.stride + i] != 0)
                    fail(std::string(what) + ": padding byte at offset " +
                         std::to_string(r.offset + k * r.stride + i) + " is not zero");
}

// zero_implicit_padding on a dirty object clears exactly the bytes in `pad`
// (offset within each `period`-byte element after `lead`) and nothing else.
template <class T>
void expect_exact_zeroing(const char* what, std::size_t lead, std::size_t period,
                          std::size_t pad_begin, std::size_t pad_end) {
    auto* object = static_cast<T*>(::operator new(sizeof(T)));
    auto* bytes = reinterpret_cast<unsigned char*>(object);
    std::memset(bytes, 0xAB, sizeof(T));
    xo::detail::zero_implicit_padding(object);
    for (std::size_t i = lead; i < sizeof(T); ++i) {   // bytes before `lead` are checked by the caller
        const bool padding = i >= lead && (i - lead) % period >= pad_begin && (i - lead) % period < pad_end;
        if (bytes[i] != (padding ? 0 : 0xAB)) {
            ::operator delete(object);
            fail(std::string(what) + ": byte " + std::to_string(i) + (padding ? " not zeroed" : " clobbered"));
        }
    }
    ::operator delete(object);
}

// Builds the same logical content with `garbage` in the caller's
// temporaries and in freed arena blocks that the elements then reuse.
std::string build(unsigned char garbage) {
    auto b = xo::TypedXBuffer<padrt::Root>::create(
        4096, xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.root();
    r.scratch.assign(512, 0x0101010101010101ull * garbage);
    r.scratch.reserve(8192);                           // the dirty 4 KiB block goes to the free table
    r.scratch.clear();
    for (int i = 0; i < 16; ++i) {
        padrt::Entry t;
        std::memset(static_cast<void*>(&t), garbage, sizeof t);   // stack garbage in the padding
        t.tag = std::uint8_t(i);
        t.value = std::uint64_t(i) * 3;
        r.entries.push_back(t);
        r.resources.insert_or_assign(std::int32_t(i), std::int64_t(i) * 7);
        auto& n = r.named.emplace_back();
        n.id = std::uint16_t(i);
        n.name = "name-" + std::to_string(i);
    }
    r.resources.erase(3);                              // shifts entries (relocation)
    r.entries.erase(r.entries.begin() + 2);
    for (const auto& e : r.entries) expect_zero_padding(e, "vector element");
    for (const auto& kv : r.resources) expect_zero_padding(kv, "map entry");
    for (const auto& n : r.named) expect_zero_padding(n, "element with string");
    return b.save_verified();
}

void elements_have_zero_padding_and_wires_are_deterministic() {
    const auto a = build(0xAB);
    const auto c = build(0xCD);
    if (a != c) fail("the same content with different garbage produced different wires");
    if (a.find(std::string(4, char(0xAB))) != std::string::npos)
        fail("caller or arena garbage reached the wire");
}
} // namespace

int main() {
    // BigArray: n (4 bytes) + 4 padding, then 4096 Interior {u8; 7 pad; u64}.
    expect_exact_zeroing<pad::BigArray>("BigArray", 8, sizeof(pad::Interior), 1, 8);
    {
        // n's own padding (bytes 4..7) lies before `lead`; check it separately.
        pad::BigArray probe;
        std::memset(static_cast<void*>(&probe), 0xAB, sizeof probe);
        xo::detail::zero_implicit_padding(&probe);
        const auto* p = reinterpret_cast<const unsigned char*>(&probe);
        for (std::size_t i = 0; i < 8; ++i)
            if (p[i] != (i < 4 ? 0xAB : 0)) fail("BigArray: bytes before items wrong at " + std::to_string(i));
    }
    // Grid: 192 ArrayOfPadded = 384 Tail {u64; u8; 7 pad}.
    expect_exact_zeroing<pad::Grid>("Grid", 0, sizeof(pad::Tail), 9, 16);

    elements_have_zero_padding_and_wires_are_deterministic();
    std::cout << "implicit padding: 12 compile-time checks, element padding zeroed, wires deterministic\n";
    return 0;
}
