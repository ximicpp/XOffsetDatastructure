// Stable-contract checks that no other test states directly (docs/STABILITY_TESTS.md).
//   K3: save_verified() output is a copy the caller owns. Mutating, growing,
//       reloading or destroying the source afterwards never changes it.
//   K5: a received buffer gets the sender's free space through the hole
//       table. Freeing and re-allocating after reload reuses holes instead of
//       growing the tail, and the reload makes the same allocation decisions
//       as the sender would have.
// No timer; counts and bytes only.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace xo = XOffsetDatastructure;

namespace sc {
struct Item {
    std::uint64_t id{};
    xo::XString label;
};
struct Root {
    std::uint32_t epoch{};
    xo::XVector<Item> items;
    xo::XMap<std::uint32_t, xo::XString> notes;
};
} // namespace sc

XOFFSET_REGISTER_SCHEMA_NAME(sc::Root, "test.stability_contract.root")

namespace {
using Buffer = xo::TypedXBuffer<sc::Root>;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_stability_contract FAILED: " << message << "\n";
    std::exit(1);
}

xo::XBufferCore::MaximumCapacity cap() {
    return xo::XBuffer::maximum_capacity(16u << 20).with_growth(xo::XGrowPolicy::Automatic);
}

std::string label(std::uint64_t i) { return "item-label-long-enough-for-heap-" + std::to_string(i); }

void fill(sc::Root& r, std::uint32_t n) {
    r.epoch = 1;
    for (std::uint32_t i = 0; i < n; ++i) {
        auto& it = r.items.emplace_back();
        it.id = i;
        it.label = label(i);
        r.notes.insert_or_assign(i, label(i + 1000));
    }
}

void check_contents(const sc::Root& r, std::uint32_t n, const char* what) {
    if (r.epoch != 1 || r.items.size() != n || r.notes.size() != n) fail(std::string(what) + ": sizes");
    for (std::uint32_t i = 0; i < n; ++i) {
        if (r.items[i].id != i || std::string_view(r.items[i].label) != label(i))
            fail(std::string(what) + ": item " + std::to_string(i));
    }
}

void k3_output_is_independent() {
    constexpr std::uint32_t n = 64;
    std::optional<Buffer> source(Buffer::create(4096, cap()));
    fill(source->root(), n);
    const std::string wire = source->save_verified();
    const std::string copy = wire;
    std::string into;
    source->save_verified_into(into);
    if (into != wire) fail("K3: save_verified_into differs from save_verified");

    // Mutate in place, grow past the current reservation, erase, clear.
    auto& r = source->root();
    r.epoch = 99;
    for (auto& it : r.items) it.label = std::string(200, 'x');
    for (std::uint32_t i = 0; i < 4096; ++i) r.items.emplace_back().id = 1u << 20;
    r.notes.clear();
    if (wire != copy || into != copy) fail("K3: mutating the source changed an emitted wire");

    source.reset();  // destroy the source
    if (wire != copy || into != copy) fail("K3: destroying the source changed an emitted wire");
    auto loaded = Buffer::load(wire, cap());
    check_contents(loaded.root(), n, "K3 reload after source destroyed");
}

void k5_free_space_travels_with_wire() {
    constexpr std::uint32_t n = 256;
    auto sender = Buffer::create(4096, cap());
    fill(sender.root(), n);
    // Retire every other label: their heap blocks become holes.
    for (std::uint32_t i = 0; i < n; i += 2) sender.root().items[i].label = "s";
    const auto wire = sender.save_verified();
    const auto holes = sender.free_block_count();
    if (holes == 0) fail("K5: fixture left no holes");

    auto receiver = Buffer::load(wire, cap());
    if (receiver.free_block_count() != holes) fail("K5: reload lost hole-table entries");
    const auto tail = receiver.arena()->used_end;
    if (sender.arena()->used_end != tail) fail("K5: sender and receiver used_end differ");

    // Refill the retired labels on both sides: holes are reused, the tail
    // does not move, and both sides place every block at the same offset.
    for (std::uint32_t i = 0; i < n; i += 2) {
        sender.root().items[i].label = label(i);
        receiver.root().items[i].label = label(i);
    }
    if (receiver.arena()->used_end != tail) fail("K5: receiver grew the tail instead of reusing holes");
    if (receiver.save_verified() != sender.save_verified())
        fail("K5: receiver made different allocation decisions from the sender");

    // A second hop keeps the property (no degradation across hops).
    auto third = Buffer::load(receiver.save_verified(), cap());
    for (std::uint32_t i = 1; i < n; i += 2) third.root().items[i].label = "t";
    const auto tail3 = third.arena()->used_end;
    for (std::uint32_t i = 1; i < n; i += 2) third.root().items[i].label = label(i);
    if (third.arena()->used_end != tail3) fail("K5: second hop grew the tail instead of reusing holes");
    check_contents(third.root(), n, "K5 second hop");
}
} // namespace

int main() {
    k3_output_is_independent();
    k5_free_space_travels_with_wire();
    std::cout << "stability contract: K3 independent output, K5 free space across hops: passed\n";
    return 0;
}
