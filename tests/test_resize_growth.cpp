// P2 semantic/failure/count gate. No timer. Parent owns all compilation/execution.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "xoffsetdatastructure.hpp"

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace dt = xo::detail;

// Actual class-specific operator-new injection in a trivial tail constructor.
// No global replacement: the allocated block remains ASan/LSan compatible.
struct HostTail {
    std::uint32_t value;
    inline static int before_failure = -1;
    inline static unsigned attempts = 0, failures = 0;
    struct HostBlock {
        unsigned char value;
        static void* operator new(std::size_t n) {
            if (before_failure >= 0) {
                ++attempts;
                if (before_failure-- == 0) {
                    ++failures;
                    throw std::bad_alloc();
                }
            }
            return ::operator new(n);
        }
        static void operator delete(void* p) noexcept { ::operator delete(p); }
    };
    HostTail() : value(91) {
        std::unique_ptr<HostBlock> allocation(new HostBlock);
        allocation->value = 1;
    }
};
struct LateTail {
    std::uint32_t value;
    inline static int before_failure = -1;
    inline static unsigned attempts = 0;
    LateTail() : value(77) {
        if (before_failure >= 0) {
            ++attempts;
            if (before_failure-- == 0) throw std::runtime_error("late-tail");
        }
    }
};
struct ResizeAttr { xo::XString key; xo::XVector<std::uint8_t> value; };
struct ResizeRecord { xo::XVector<ResizeAttr> attrs; };
struct ResizeRoot {
    xo::XVector<ResizeAttr> attrs;
    xo::XVector<ResizeRecord> records;
    xo::XVector<xo::XString> words;
    xo::XVector<std::uint32_t> numbers;
#if XOFFSET_SCHEMA_REFLECTION
    // Elements with throwing user constructors: a C++26-only record shape.
    xo::XVector<LateTail> late;
    xo::XVector<HostTail> host;
#endif
};
XOFFSET_RECORD_DERIVED(ResizeAttr, (), key, value)
XOFFSET_RECORD_DERIVED(ResizeRecord, (), attrs)
#if XOFFSET_SCHEMA_REFLECTION
XOFFSET_RECORD_DERIVED(ResizeRoot, (), attrs, records, words, numbers, late, host)
#else
XOFFSET_RECORD_DERIVED(ResizeRoot, (), attrs, records, words, numbers)
#endif
XOFFSET_REGISTER_SCHEMA_NAME(ResizeRoot, "test.P2.resize-growth.v1")

// Explicit non-relocatable fallback: destructor may throw by type, but the
// enumerated test destructor does not. The same reflected string is copied.
struct Fallback { xo::XString text; ~Fallback() noexcept(false) {} };
XOFFSET_RECORD_DERIVED(Fallback, (), text)
namespace XOffsetDatastructure {
template<> struct is_byte_copy_safe<Fallback> : std::true_type {};
}
static_assert(fd::can_relocate_without_allocation<ResizeAttr>());
#if XOFFSET_SCHEMA_REFLECTION
static_assert(fd::can_relocate_without_allocation<LateTail>());
static_assert(fd::can_relocate_without_allocation<HostTail>());
#endif
static_assert(!fd::can_relocate_without_allocation<Fallback>());

namespace {
unsigned cases = 0;
void need(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
auto policy(std::size_t cap = 64u * 1024u * 1024u) {
    return xo::XBuffer::maximum_capacity(cap).with_growth(xo::XGrowPolicy::Automatic);
}
auto make_buffer(std::size_t initial = 1024u * 1024u) {
    return xo::XBuffer::create<ResizeRoot>(initial, policy());
}
template<class T> auto descriptor(const T& value) {
    static_assert(sizeof(T) == 16);
    std::array<unsigned char, 16> bytes{};
    std::memcpy(bytes.data(), &value, bytes.size());
    return bytes;
}
// V2: a binding is exactly the payload base (no allocator-state pointer).
bool same_binding(fd::allocator_ref a, fd::allocator_ref b) {
    return a.segment == b.segment;
}
// V2 has no allocator operation counters (FreelistIndex diagnostics are gone).
// Live bytes = used_end - free bytes, observed through the host free table.
std::uint64_t live_bytes(const xo::XBufferCore& b) { return b.arena()->used_end - b.free_block_bytes(); }
void fill(ResizeAttr& a, std::string_view key, std::uint8_t byte) {
    a.key.assign(key); a.value.resize(16);
    std::fill(a.value.begin(), a.value.end(), byte);
}
void check(const ResizeAttr& a, std::string_view key, std::uint8_t byte) {
    need(a.key.view() == key && a.value.size() == 16, "nested value mismatch");
    for (auto x : a.value) need(x == byte, "nested byte mismatch");
}
std::string logical_contents(const ResizeRoot& r) {
    std::string result;
    auto number = [&](std::uint64_t x) {
        result.append(reinterpret_cast<const char*>(&x), sizeof(x));
    };
    auto bytes = [&](const void* p, std::size_t n) {
        number(n); if (n) result.append(static_cast<const char*>(p), n);
    };
    auto attrs = [&](const auto& values) {
        number(values.size());
        for (const auto& a : values) {
            bytes(a.key.data(), a.key.size()); bytes(a.value.data(), a.value.size());
        }
    };
    attrs(r.attrs); number(r.records.size()); for (const auto& row : r.records) attrs(row.attrs);
    number(r.words.size()); for (const auto& word : r.words) bytes(word.data(), word.size());
    number(r.numbers.size()); for (const auto value : r.numbers) number(value);
#if XOFFSET_SCHEMA_REFLECTION
    number(r.late.size()); for (const auto& value : r.late) number(value.value);
    number(r.host.size()); for (const auto& value : r.host) number(value.value);
#endif
    return result;
}
void roundtrip(xo::XBuffer& b) {
    const auto expected = logical_contents(b.root<ResizeRoot>());
    auto bytes = b.save_verified<ResizeRoot>();
    auto copy = xo::XBuffer::load_verified<ResizeRoot>(bytes, policy());
    auto& r = copy.root<ResizeRoot>();
    need(logical_contents(r) == expected, "roundtrip full logical contents mismatch");
    // Continue using public APIs after reloading the changed physical layout.
    r.numbers.resize(std::size_t(r.numbers.size()) + 1, std::uint32_t{123});
    r.words.resize(std::size_t(r.words.size()) + 1, std::string_view("after-reload"));
    if (!r.attrs.empty()) r.attrs.resize(std::size_t(r.attrs.size()) + 1, r.attrs[0]);
    const auto changed = logical_contents(r);
    auto bytes2 = copy.save_verified<ResizeRoot>();
    auto copy2 = xo::XBuffer::load_verified<ResizeRoot>(bytes2, policy());
    need(logical_contents(copy2.root<ResizeRoot>()) == changed, "reload-mutate-reload contents mismatch");
}

void default_growth_preserves_prefix_storage() {
    auto b = make_buffer(); auto& v = b.root<ResizeRoot>().attrs;
    // I1: a key of at most 10 bytes is inline (its data() moves with the head),
    // so the out-of-line prefix check uses a longer key; "beta" stays inline.
    v.resize(2); fill(v[0], "alpha-heap-key", 10); fill(v[1], "beta", 11);
    need(!v[0].key.is_inline() && v[1].key.is_inline(), "fixture key placement");
    auto* old_parent = v.data(); auto* p0 = v[0].key.data(); auto* p1 = v[1].value.data();
    const auto bind = v.binding();
    v.resize(5);
    need(v.size() == 5 && v.capacity() == 5 && v.data() != old_parent, "growth extent/strategy");
    need(v[0].key.data() == p0 && v[1].value.data() == p1, "old prefix payload copied");
    need(same_binding(v.binding(), bind) && same_binding(v[0].key.binding(), bind), "binding changed");
    check(v[0], "alpha-heap-key", 10); check(v[1], "beta", 11);
    need(v[1].key.is_inline(), "relocated inline key left inline mode");
    for (unsigned i = 2; i < 5; ++i) need(v[i].key.empty() && v[i].value.empty(), "new tail not default");
    roundtrip(b); ++cases;
}

void same_arena_fill_alias() {
    auto b = make_buffer(); auto& v = b.root<ResizeRoot>().attrs;
    // Out-of-line key (> 10 bytes, I1) so its payload address is meaningful.
    v.resize(2); fill(v[0], "aliased-heap-key", 33); fill(v[1], "second", 44);
    auto* p = v[0].key.data(); auto* bytes = v[0].value.data();
    v.resize(5, v[0]);
    need(v[0].key.data() == p && v[0].value.data() == bytes, "fill moved prefix too early");
    for (unsigned i = 2; i < 5; ++i) {
        check(v[i], "aliased-heap-key", 33);
        need(v[i].key.data() != p && v[i].value.data() != bytes, "fill incorrectly shares ownership");
    }
    v[0].key.assign("changed"); check(v[2], "aliased-heap-key", 33);
    roundtrip(b); ++cases;
}

void subobject_and_cross_arena_fill() {
    auto b = make_buffer(); auto& root = b.root<ResizeRoot>();
    root.words.resize(2); root.words[0].assign("prefix-subobject"); root.words[1].assign("other");
    auto* old = root.words[0].data();
    std::string_view sub(root.words[0].data() + 7, root.words[0].size() - 7);
    root.words.resize(5, sub);
    need(root.words[0].data() == old, "subobject fill copied old payload");
    for (unsigned i = 2; i < 5; ++i) need(root.words[i].view() == "subobject", "subobject fill lost alias");
    auto source = make_buffer(); auto& a = source.root<ResizeRoot>().attrs;
    a.resize(1); fill(a[0], "external", 66); const auto src = descriptor(a[0].key);
    root.attrs.resize(1); fill(root.attrs[0], "old-heap-prefix", 7); auto* ptr = root.attrs[0].key.data();
    root.attrs.resize(4, a[0]);
    need(descriptor(a[0].key) == src && root.attrs[0].key.data() == ptr, "cross-arena fill mutated source/prefix");
    for (unsigned i = 1; i < 4; ++i) {
        check(root.attrs[i], "external", 66);
        need(same_binding(root.attrs[i].key.binding(), root.attrs.binding()), "fill kept foreign binding");
        need(root.attrs[i].key.data() != a[0].key.data(), "cross-arena fill shared source payload");
    }
    roundtrip(b); roundtrip(source); ++cases;
}

void zero_shrink_and_in_capacity() {
    xo::XVector<std::uint32_t> unbound; const auto empty_descriptor = descriptor(unbound);
    unbound.resize(0); need(descriptor(unbound) == empty_descriptor, "unbound zero resize changed head");
    auto b = make_buffer(); auto& v = b.root<ResizeRoot>().words;
    v.reserve(8); v.resize(2); v[0].assign("stable"); v[1].assign("fill");
    auto* parent = v.data(); auto* child = v[0].data();
    v.resize(5, v[1]);
    need(v.data() == parent && v.capacity() == 8 && v[0].data() == child, "in-capacity path changed");
    for (unsigned i = 2; i < 5; ++i) need(v[i].view() == "fill", "in-capacity fill mismatch");
    v.resize(1); need(v.size() == 1 && v.capacity() == 8 && v[0].data() == child, "shrink changed retained element");
    v.resize(0); need(v.empty() && v.data() == parent && v.capacity() == 8, "zero resize releases capacity");
    v.resize(3); for (const auto& s : v) need(s.empty(), "regrow default values");
    auto& numbers = b.root<ResizeRoot>().numbers; numbers.resize(2); numbers[0] = 15;
    const auto before = descriptor(numbers); bool caught = false;
    try { numbers.resize(std::numeric_limits<std::size_t>::max()); }
    catch (const xo::XBadAlloc&) { caught = true; }
    need(caught && descriptor(numbers) == before && numbers[0] == 15, "count overflow changed old vector");
    roundtrip(b); ++cases;
}

struct CountedView {
    std::string_view text; unsigned* calls; unsigned throw_at;
    operator std::string_view() const {
        if (++*calls == throw_at) throw std::runtime_error("late-fill");
        return text;
    }
};
void late_fill_and_default_failure() {
    auto b = make_buffer(); auto& root = b.root<ResizeRoot>();
    root.words.resize(2); root.words[0].assign("old-prefix"); root.words[1].assign("keep");
    const auto desc = descriptor(root.words); auto* child = root.words[0].data();
    unsigned calls = 0; bool caught = false;
    try { root.words.resize(6, CountedView{root.words[0].view(), &calls, 3}); }
    catch (const std::runtime_error& e) { caught = std::string_view(e.what()) == "late-fill"; }
    need(caught && calls == 3 && descriptor(root.words) == desc && root.words[0].data() == child,
         "late fill failure changed descriptor/prefix");
    need(root.words[0].view() == "old-prefix" && root.words[1].view() == "keep", "late fill failure content");
    root.words.reserve(8); const auto reserved = descriptor(root.words); child = root.words[0].data();
    calls = 0; caught = false;
    try { root.words.resize(6, CountedView{root.words[0].view(), &calls, 3}); }
    catch (const std::runtime_error& e) { caught = std::string_view(e.what()) == "late-fill"; }
    need(caught && calls == 3 && descriptor(root.words) == reserved && root.words[0].data() == child,
         "in-capacity late failure changed original state");
#if XOFFSET_SCHEMA_REFLECTION
    root.late.resize(2); root.late[0].value = 12; root.late[1].value = 13;
    const auto ld = descriptor(root.late); LateTail::before_failure = 2; LateTail::attempts = 0; caught = false;
    try { root.late.resize(7); } catch (const std::runtime_error& e) { caught = std::string_view(e.what()) == "late-tail"; }
    LateTail::before_failure = -1;
    need(caught && LateTail::attempts == 3 && descriptor(root.late) == ld && root.late[0].value == 12 && root.late[1].value == 13,
         "late default failure changed original state");
    root.late.resize(7); need(root.late[6].value == 77, "default recovery failed");
#endif
    roundtrip(b); ++cases;
}

#if XOFFSET_SCHEMA_REFLECTION
void host_bad_alloc_tail_sweep() {
    for (int point = 0; point < 5; ++point) {
        auto b = make_buffer(); auto& v = b.root<ResizeRoot>().host;
        v.resize(2); v[0].value = 21; v[1].value = 22; const auto before = descriptor(v);
        HostTail::before_failure = point; HostTail::attempts = HostTail::failures = 0;
        bool caught = false; try { v.resize(7); } catch (const std::bad_alloc&) { caught = true; }
        HostTail::before_failure = -1;
        need(caught && HostTail::attempts == unsigned(point + 1) && HostTail::failures == 1,
             "host-tail failure point not reached");
        need(descriptor(v) == before && v[0].value == 21 && v[1].value == 22, "host-tail failure changed prefix");
        v.resize(7); need(v[6].value == 91, "host-tail failure recovery"); roundtrip(b);
    }
    ++cases;
}
#endif

void arena_bad_alloc_and_vm_failure() {
    auto b = xo::XBuffer::create<ResizeRoot>(32768, xo::XBuffer::maximum_capacity(32768));
    auto& v = b.root<ResizeRoot>().words;
    v.resize(2); v[0].assign("live-one"); v[1].assign("live-two");
    const auto before = descriptor(v); auto* prefix = v[0].data();
    const std::string big(10000, 'z'); unsigned calls = 0; bool caught = false;
    try { v.resize(10, CountedView{big, &calls, 0}); } catch (const xo::XBadAlloc&) { caught = true; }
    need(caught && calls >= 2, "arena OOM did not reach partial tail");
    need(descriptor(v) == before && v[0].data() == prefix && v[0].view() == "live-one" && v[1].view() == "live-two",
         "arena tail OOM changed live state");
    auto& fixed_numbers = b.root<ResizeRoot>().numbers; fixed_numbers.resize(2); fixed_numbers[0] = 51;
    const auto nd = descriptor(fixed_numbers); caught = false;
    try { fixed_numbers.resize(65536); } catch (const xo::XBadAlloc&) { caught = true; }
    need(caught && descriptor(fixed_numbers) == nd && fixed_numbers[0] == 51,
         "fresh-buffer arena OOM changed original state");
    roundtrip(b);
    auto vm = make_buffer(4096); auto& values = vm.root<ResizeRoot>().numbers;
    values.resize(2); values[0] = 7; values[1] = 8; auto vd = descriptor(values); caught = false;
    dt::clear_vm_failures(); dt::fail_next_vm_operation(dt::VmOperation::Enable);
    try { values.resize(262144); } catch (const xo::XBadAlloc&) { caught = true; }
    dt::clear_vm_failures();
    need(caught && descriptor(values) == vd && values[0] == 7 && values[1] == 8, "VM growth failure changed descriptor");
    roundtrip(vm); ++cases;
}

void nonrelocatable_fallback() {
    auto b = make_buffer();
    auto* v = std::construct_at(static_cast<xo::XVector<Fallback>*>(
        b.arena()->allocate_aligned(sizeof(xo::XVector<Fallback>), alignof(xo::XVector<Fallback>))),
        fd::allocator_ref_of(b.arena()));
    v->resize(2); (*v)[0].text.assign("fallback-old"); (*v)[1].text.assign("second");
    const char* original = (*v)[0].text.data();
    v->resize(4);
    need((*v)[0].text.view() == "fallback-old" && (*v)[1].text.view() == "second", "fallback content changed");
    need((*v)[0].text.data() != original, "fallback unexpectedly used prefix relocation");
    std::destroy_at(v); ++cases;
}

void stage6_mechanism_gate() {
    constexpr unsigned n = 4096;
    auto b = make_buffer(16u * 1024u * 1024u); auto& rows = b.root<ResizeRoot>().records;
    rows.resize(n);
    std::vector<std::array<const void*, 4>> pointers(n);
    constexpr const char* keys[] = {"k0000", "k0001", "k0002", "k0003"};
    for (unsigned i = 0; i < n; ++i) {
        rows[i].attrs.resize(4);
        for (unsigned j = 0; j < 4; ++j) {
            fill(rows[i].attrs[j], keys[j], std::uint8_t(i + j));
            // I1: "k000x" keys are inline and own no payload; values are out of line.
            pointers[i][j] = rows[i].attrs[j].value.data();
        }
    }
    const auto live_before = live_bytes(b);
    for (unsigned i = 0; i < n; ++i) {
        auto& attrs = rows[i].attrs; attrs.resize(5);
        attrs[4].key.assign("k0004"); attrs[4].value.resize(32);
        std::fill(attrs[4].value.begin(), attrs[4].value.end(), std::uint8_t(i));
        // I1: a 5-byte key is stored inline and allocates nothing.
        need(attrs[4].key.is_inline() && attrs[4].key.owned_extent() == 0, "stage6 short key allocated");
    }
    // V1 goal was 3 arena allocations per row (array, key, value). V2: the key
    // is inline, so each row is exactly one 5 x 32-byte array allocation, one
    // 32-byte value allocation and one retire of the old 4 x 32-byte array.
    constexpr std::uint64_t row_delta = 5 * sizeof(ResizeAttr) + 32 - 4 * sizeof(ResizeAttr);
    const auto count = live_bytes(b) - live_before;
    need(count == row_delta * n, "stage6 mechanism goal (2 exact allocations + 1 retire per row) not met");
    for (unsigned i = 0; i < n; ++i) for (unsigned j = 0; j < 4; ++j) {
        const auto& a = rows[i].attrs[j]; check(a, keys[j], std::uint8_t(i + j));
        need(a.key.is_inline() && a.value.data() == pointers[i][j], "stage6 old nested storage copied");
    }
    roundtrip(b);
    std::cout << "{\"case\":\"stage6-resize-mechanism\",\"operations\":" << n
              << ",\"live_bytes_delta\":" << count
              << ",\"expected\":" << row_delta * n << ",\"old_prefix_nested_payload_addresses_preserved\":true,\"performance_sample\":false}\n";
    ++cases;
}

void make_compatibility_fixture(xo::XBuffer& b) {
    auto& r = b.root<ResizeRoot>();
    r.numbers.resize(3); r.numbers[0] = 10; r.numbers[1] = 20; r.numbers[2] = 30;
    r.attrs.resize(2); fill(r.attrs[0], "compat-first", 42); fill(r.attrs[1], "compat-second", 43);
    r.attrs.resize(5, r.attrs[0]);
    r.words.resize(1); r.words[0].assign("alias-suffix");
    r.words.resize(4, std::string_view(r.words[0].data() + 6, 6));
}
void check_compatibility_fixture(const ResizeRoot& r) {
    need(r.numbers.size() == 3 && r.numbers[0] == 10 && r.numbers[2] == 30, "compat scalar mismatch");
    need(r.attrs.size() == 5, "compat nested length");
    check(r.attrs[0], "compat-first", 42); check(r.attrs[1], "compat-second", 43);
    for (unsigned i = 2; i < 5; ++i) check(r.attrs[i], "compat-first", 42);
    need(r.words.size() == 4 && r.words[0].view() == "alias-suffix", "compat words");
    for (unsigned i = 1; i < 4; ++i) need(r.words[i].view() == "suffix", "compat aliased suffix");
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--write-wire") {
            auto b = make_buffer(); make_compatibility_fixture(b); check_compatibility_fixture(b.root<ResizeRoot>());
            const auto wire = b.save_verified<ResizeRoot>(); std::ofstream out(argv[2], std::ios::binary);
            out.write(wire.data(), std::streamsize(wire.size())); if (!out) throw std::runtime_error("write-wire failed");
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--read-wire") {
            std::ifstream in(argv[2], std::ios::binary); if (!in) throw std::runtime_error("read-wire failed");
            std::string wire((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            auto b = xo::XBuffer::load_verified<ResizeRoot>(wire, policy()); check_compatibility_fixture(b.root<ResizeRoot>());
            return 0;
        }
        if (argc != 1) throw std::runtime_error("expected no arguments or --write-wire/--read-wire PATH");
        default_growth_preserves_prefix_storage(); same_arena_fill_alias(); subobject_and_cross_arena_fill();
        zero_shrink_and_in_capacity(); late_fill_and_default_failure();
#if XOFFSET_SCHEMA_REFLECTION
        host_bad_alloc_tail_sweep();
#endif
        arena_bad_alloc_and_vm_failure(); nonrelocatable_fallback(); stage6_mechanism_gate();
        std::cout << "P2 resize growth cases=" << cases << " passed (no timing)\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "P2 resize growth failure: " << e.what() << '\n'; return 1; }
}
