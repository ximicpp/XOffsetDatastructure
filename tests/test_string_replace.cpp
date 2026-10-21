// v3 full-string replacement regression and operation gate. No timer.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "xoffsetdatastructure.hpp"

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace dt = xo::detail;

struct ReplaceAttr { xo::XString key; xo::XVector<std::uint8_t> value; };
struct ReplaceRoot {
    xo::XString first, second, blocker;
    xo::XVector<xo::XString> words;
    xo::XVector<ReplaceAttr> attrs;
};
XOFFSET_REGISTER_SCHEMA_NAME(ReplaceRoot, "test.v3.full-string-replacement.v1")

namespace {
void need(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
auto policy(std::size_t cap = 16u * 1024u * 1024u) {
    return xo::XBuffer::maximum_capacity(cap).with_growth(xo::XGrowPolicy::Automatic);
}
auto make_buffer(std::size_t initial = 1024u * 1024u) {
    return xo::XBuffer::create<ReplaceRoot>(initial, policy());
}
template<class T> auto descriptor(const T& value) {
    static_assert(sizeof(T) == 16);
    std::array<unsigned char, 16> result{};
    std::memcpy(result.data(), &value, result.size());
    return result;
}
// V2: a binding is exactly the payload base (no allocator-state pointer).
bool same_binding(fd::allocator_ref a, fd::allocator_ref b) {
    return a.segment == b.segment;
}
// V2 has no allocator operation counters (FreelistIndex diagnostics are gone).
// Allocator operations are observed through the host free table instead:
// live bytes = used_end - free bytes, and a retired block is an exact hole.
using Holes = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
Holes holes(const xo::XBufferCore& b) {
    Holes out; b.for_each_free_block([&](xo::XBufferCore::WireHole h) { out.emplace_back(h.offset, h.bytes); }); return out;
}
std::uint64_t live(const xo::XBufferCore& b) { return b.arena()->used_end - b.free_block_bytes(); }
std::uint32_t extent(std::size_t bytes) { return static_cast<std::uint32_t>((bytes + 7u) & ~std::size_t(7)); }
bool has_hole(const xo::XBufferCore& b, const void* p, std::uint32_t bytes) {
    const auto offset = static_cast<std::uint32_t>(static_cast<const char*>(p) - b.data());
    for (auto [o, n] : holes(b)) if (o == offset && n == bytes) return true;
    return false;
}
void check(const xo::XString& value, std::string_view expected) {
    need(value.view() == expected && value.size() == expected.size(), "string value mismatch");
    need(!value.data() || value.data()[value.size()] == '\0', "missing terminator");
    need(value.capacity() >= value.size(), "invalid string capacity");
}
std::string logical(const ReplaceRoot& root) {
    std::string result;
    auto integer = [&](std::uint64_t n) { result.append(reinterpret_cast<const char*>(&n), sizeof(n)); };
    auto bytes = [&](const void* data, std::size_t n) {
        integer(n); if (n) result.append(static_cast<const char*>(data), n);
    };
    for (auto* text : {&root.first, &root.second, &root.blocker}) bytes(text->data(), text->size());
    integer(root.words.size());
    for (const auto& text : root.words) bytes(text.data(), text.size());
    integer(root.attrs.size());
    for (const auto& attr : root.attrs) { bytes(attr.key.data(), attr.key.size()); bytes(attr.value.data(), attr.value.size()); }
    return result;
}
void roundtrip(xo::XBuffer& buffer) {
    const auto expected = logical(buffer.root<ReplaceRoot>());
    auto loaded = xo::XBuffer::load_verified<ReplaceRoot>(buffer.save_verified<ReplaceRoot>(), policy());
    need(logical(loaded.root<ReplaceRoot>()) == expected, "verified roundtrip contents");
    auto& root = loaded.root<ReplaceRoot>();
    root.first.assign(std::string(333, 'R'));
    root.second.assign(root.first.view().substr(1, 97));
    const auto changed = logical(root);
    auto twice = xo::XBuffer::load_verified<ReplaceRoot>(loaded.save_verified<ReplaceRoot>(), policy());
    need(logical(twice.root<ReplaceRoot>()) == changed, "reload-mutate-reload contents");
}

void unchanged_and_overlapping_capacity() {
    auto buffer = make_buffer(); auto& text = buffer.root<ReplaceRoot>().first;
    text.assign("abcdefghijklmnopqrstuvwxyzABCDE");
    const auto capacity = text.capacity(); auto* original = text.data(); const auto binding = text.binding();
    const std::string expected(text.view().substr(3, 17));
    text.assign(text.view().substr(3, 17)); check(text, expected);
    need(text.data() == original && text.capacity() == capacity, "substring allocated or changed capacity");
    text.assign(text.view()); check(text, expected);
    const std::string nul("a\0b\0c", 5); text.assign(nul); check(text, nul);
    text.assign({}); check(text, {});
    need(text.data() == original && text.capacity() == capacity && same_binding(binding, text.binding()),
         "clear changed storage/binding");
    text.assign("recover"); check(text, "recover"); roundtrip(buffer);
}

void replacement_growth_does_not_add_allocations() {
    auto buffer = make_buffer(); auto& root = buffer.root<ReplaceRoot>();
    root.first.assign(std::string(63, 'a')); root.blocker.assign(std::string(127, 'b'));
    const auto binding = root.first.binding(); auto* old = root.first.data();
    const std::string desired(512, 'x'); const auto live_before = live(buffer);
    root.first.assign(desired);
    // Exactly one exact 513 -> 520-byte allocation and one retire of the old 64-byte block.
    need(live(buffer) == live_before + extent(513) - extent(64) && has_hole(buffer, old, extent(64)),
         "fallback replacement allocation/retirement count");
    need(root.first.data() != old && root.first.capacity() == desired.size(), "fallback/exact capacity not exercised");
    need(same_binding(binding, root.first.binding()), "replacement changed binding");
    check(root.first, desired); check(root.blocker, std::string(127, 'b')); roundtrip(buffer);
}

void physical_tail_still_expands_in_place() {
    auto buffer = make_buffer(); auto& text = buffer.root<ReplaceRoot>().first;
    text.assign(std::string(31, 'a')); auto* old = text.data();
    const std::string desired(200, 'e'); const auto holes_before = holes(buffer);
    const auto used_before = buffer.arena()->used_end; text.assign(desired);
    need(text.data() == old && text.capacity() == 200, "physical-tail expansion lost");
    // In-place tail growth: the free table is untouched and used_end moves by exactly the extent delta.
    need(holes(buffer) == holes_before && buffer.arena()->used_end == used_before + extent(201) - extent(32),
         "tail expansion allocated/retired");
    check(text, desired); roundtrip(buffer);
}

void terminator_alias_survives_retirement() {
    // capacity+1 is a readable range including the live terminator. Its length
    // exceeds capacity, so this exercises growth while rhs aliases old storage.
    for (bool block_tail : {false, true}) {
        auto buffer = make_buffer(); auto& root = buffer.root<ReplaceRoot>();
        root.first.assign(std::string(31, 'a'));
        if (block_tail) root.blocker.assign(std::string(127, 'B'));
        auto* old = root.first.data(); const auto binding = root.first.binding();
        const std::string expected(root.first.data(), root.first.size() + 1);
        root.first.assign(std::string_view(root.first.data(), root.first.capacity() + 1));
        check(root.first, expected);
        need((root.first.data() != old) == block_tail, "alias test did not exercise desired growth path");
        need(same_binding(binding, root.first.binding()) && root.first.capacity() == 32, "alias binding/capacity");
        if (block_tail) check(root.blocker, std::string(127, 'B'));
        roundtrip(buffer);
    }
}

void same_and_cross_arena_sources() {
    auto buffer = make_buffer(); auto donor = make_buffer(); auto& root = buffer.root<ReplaceRoot>();
    auto& foreign = donor.root<ReplaceRoot>();
    root.first.assign("small"); root.second.assign(std::string(257, 's'));
    root.words.resize(2); root.words[0].assign(std::string(97, 'n'));
    root.attrs.resize(1); root.attrs[0].key.assign(std::string(193, 'k'));
    root.attrs[0].value.resize(4, std::uint8_t{73});
    const auto binding = root.first.binding(); root.first.assign(root.second.view());
    check(root.first, std::string(257, 's')); check(root.second, std::string(257, 's'));
    root.words[1].assign(root.attrs[0].key.view()); check(root.words[1], std::string(193, 'k'));
    foreign.first.assign(std::string(401, 'f')); const auto foreign_before = logical(foreign);
    root.first = foreign.first; check(root.first, std::string(401, 'f'));
    need(logical(foreign) == foreign_before && same_binding(binding, root.first.binding()), "cross-arena source/binding changed");
    root.first.assign(root.words[0].view()); check(root.first, std::string(97, 'n'));
    roundtrip(buffer); roundtrip(donor);
}

void failed_growth_preserves_descriptor_and_source() {
    auto buffer = xo::XBuffer::create<ReplaceRoot>(32768, xo::XBuffer::maximum_capacity(32768));
    auto& root = buffer.root<ReplaceRoot>();
    root.first.assign(std::string(31, 'a')); root.blocker.assign(std::string(127, 'b'));
    const auto before = descriptor(root.first); const auto contents = logical(root); const auto binding = root.first.binding();
    const std::string too_large(65536, 'z'); bool caught = false;
    try { root.first.assign(too_large); } catch (const xo::XBadAlloc&) { caught = true; }
    need(caught && descriptor(root.first) == before && logical(root) == contents && same_binding(binding, root.first.binding()),
         "fixed arena OOM changed descriptor/source");
    root.first.assign(std::string(191, 'r')); check(root.first, std::string(191, 'r')); roundtrip(buffer);

    // Both the physical-tail and blocked-tail fresh-allocation paths propagate
    // a real OS enable/preparation failure without retrying the mutation.
    for (bool block_tail : {false, true}) {
        auto vm = make_buffer(4096); auto& value = vm.root<ReplaceRoot>();
        value.first.assign(std::string(31, 'v'));
        if (block_tail) value.blocker.assign(std::string(127, 'b'));
        const auto vd = descriptor(value.first); const auto snapshot = logical(value);
        dt::clear_vm_failures(); dt::fail_next_vm_operation(dt::VmOperation::Enable); caught = false;
        try { value.first.assign(too_large); } catch (const xo::XBadAlloc&) { caught = true; }
        dt::clear_vm_failures();
        need(caught && descriptor(value.first) == vd && logical(value) == snapshot, "VM failure changed string/source");
        value.first.assign(too_large); check(value.first, too_large); roundtrip(vm);
    }
}

void rejection_order_and_layout() {
    static_assert(sizeof(xo::XFixedString) == 16 && sizeof(xo::XString) == 16);
    xo::XFixedString unbound; const auto initial = descriptor(unbound); bool caught = false;
    try { unbound.assign({}); } catch (const xo::XException&) { caught = true; }
    need(caught && descriptor(unbound) == initial, "unbound empty validation changed");
    // A fabricated >MAX_PAYLOAD string_view backed by one byte would itself
    // violate the view's valid-range contract. The patch keeps the existing
    // size rejection byte-for-byte; this test does not manufacture such input.
    auto buffer = make_buffer(); auto& text = buffer.root<ReplaceRoot>().first; text.assign("live");
    check(text, "live");
    roundtrip(buffer);
}

void repeated_full_replacement_operation_gate() {
    // Deterministic operation gate, not a replacement benchmark workload.
    // Every initial payload is followed by another live allocation, so the
    // 128-byte replacement must use a fresh block, exactly as asserted below.
    // I1: initial values of at most 10 bytes are inline and own no block.
    constexpr unsigned n = 4096;
    auto buffer = make_buffer(8u * 1024u * 1024u); auto& root = buffer.root<ReplaceRoot>();
    root.words.resize(n); std::uint64_t old_bytes = 0, old_extents = 0; std::size_t heap_words = 0;
    for (unsigned i = 0; i < n; ++i) {
        const auto size = 8u + i % 57u; old_bytes += size;
        root.words[i].assign(std::string(size, char('a' + i % 26)));
        need(root.words[i].is_inline() == (size <= 10), "initial word placement");
        if (!root.words[i].is_inline()) { ++heap_words; old_extents += extent(root.words[i].owned_extent()); }
    }
    root.blocker.assign(std::string(1024, 'b')); const std::string desired(128, 'X');
    need(buffer.free_block_count() == 0, "operation gate fixture has holes");
    const auto live_before = live(buffer);
#ifdef XOFFSET_STRING_REPLACE_COPY_AUDIT
    fd::reset_string_replace_audit_stats();
#endif
    for (auto& word : root.words) word.assign(desired);
    // One exact 129 -> 136-byte allocation per word and one retire per heap
    // word; every retired block (at most 72 bytes) stays a distinct hole.
    need(live(buffer) == live_before + std::uint64_t(n) * extent(129) - old_extents &&
         buffer.free_block_count() == heap_words && buffer.free_block_bytes() == old_extents,
         "full replacement operation gate changed allocation count");
#ifdef XOFFSET_STRING_REPLACE_COPY_AUDIT
    need(fd::string_replace_audit_stats.fresh_replacements == n &&
         fd::string_replace_audit_stats.old_bytes_not_copied == old_bytes &&
         fd::string_replace_audit_stats.rhs_bytes_copied == std::uint64_t(n) * desired.size() &&
         fd::string_replace_audit_stats.tail_expansions == 0, "copy audit byte gate mismatch");
#endif
    for (const auto& word : root.words) check(word, desired);
    check(root.blocker, std::string(1024, 'b'));
    std::cout << "operation gate: replacements=" << n << " old_bytes_not_copied=" << old_bytes
              << " rhs_bytes=" << std::uint64_t(n) * desired.size() << " (logical bytes, no timer)\n";
    roundtrip(buffer);
}

void compatibility_fill(ReplaceRoot& root) {
    root.first.assign(std::string(31, 'a')); root.blocker.assign(std::string(127, 'b'));
    root.first.assign(std::string(512, 'r'));
    root.second.assign(std::string("embedded\0tail", 13));
    root.words.resize(2); root.words[0].assign("word"); root.words[1].assign(std::string(129, 'W'));
    root.attrs.resize(2);
    for (unsigned i = 0; i < 2; ++i) {
        root.attrs[i].key.assign(i ? "k0001" : "k0000");
        root.attrs[i].value.resize(16, std::uint8_t(41 + i));
    }
}
std::string expected_compatibility() {
    auto buffer = make_buffer(); compatibility_fill(buffer.root<ReplaceRoot>()); return logical(buffer.root<ReplaceRoot>());
}
void write_wire(const char* path) {
    auto buffer = make_buffer(); compatibility_fill(buffer.root<ReplaceRoot>());
    const auto wire = buffer.save_verified<ReplaceRoot>(); std::ofstream out(path, std::ios::binary);
    out.write(wire.data(), std::streamsize(wire.size())); need(bool(out), "wire output failed");
}
void read_wire(const char* path) {
    std::ifstream in(path, std::ios::binary); need(bool(in), "wire input missing");
    const std::string wire{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto loaded = xo::XBuffer::load_verified<ReplaceRoot>(wire, policy());
    need(logical(loaded.root<ReplaceRoot>()) == expected_compatibility(), "old/new reader logical mismatch");
    roundtrip(loaded);
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--write-wire") { write_wire(argv[2]); return 0; }
        if (argc == 3 && std::string_view(argv[1]) == "--read-wire") { read_wire(argv[2]); return 0; }
        need(argc == 1, "unknown argument");
        unchanged_and_overlapping_capacity(); replacement_growth_does_not_add_allocations();
        physical_tail_still_expands_in_place(); terminator_alias_survives_retirement();
        same_and_cross_arena_sources(); failed_growth_preserves_descriptor_and_source(); rejection_order_and_layout();
        repeated_full_replacement_operation_gate();
        std::cout << "string full-replacement: 8 semantic/failure/operation groups passed; no timer\n";
        return 0;
    } catch (const std::exception& error) {
        dt::clear_vm_failures(); std::cerr << "string full-replacement failure: " << error.what() << '\n'; return 1;
    }
}
