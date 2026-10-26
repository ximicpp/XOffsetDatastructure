// reset<T>() clears only [header, used_end) (N2). The claim it rests on is
// that every logical byte at or past used_end is already zero, so the reset
// buffer must be byte-identical to create<T>() of the same size: after random
// mutation runs (growth, holes, tail expansion), after an unverified load
// with bytes past its used_end, and after a failed reload. The emptied free
// table is kept and works exactly as a fresh one.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct alignas(8) Item { std::int32_t id{}; std::int32_t value{}; };

struct ResetRoot {
    std::uint64_t id{};
    XString name;
    XVector<XString> names;
    XVector<Item> items;
    XVector<std::uint64_t> wides;
};
XOFFSET_REGISTER_SCHEMA_NAME(ResetRoot, "test.ResetUsedRange")

namespace {

constexpr std::size_t kMax = std::size_t(16) << 20;
auto cap() { return XBuffer::maximum_capacity(kMax).with_growth(XGrowPolicy::Automatic); }

std::string text(std::mt19937& rng, std::size_t length) {
    std::string out;
    for (std::size_t i = 0; i < length; ++i) out.push_back(static_cast<char>(0x21 + rng() % 90));
    return out;
}

void mutate(ResetRoot& r, std::mt19937& rng, int steps) {
    for (int s = 0; s < steps; ++s) {
        switch (rng() % 9) {
        case 0: r.name = std::string_view(text(rng, rng() % 80)); break;
        case 1: r.names.emplace_back(); r.names.back() = std::string_view(text(rng, rng() % 40)); break;
        case 2: if (r.names.size()) r.names[std::uint32_t(rng() % r.names.size())] = std::string_view(text(rng, rng() % 120)); break;
        case 3: if (r.names.size()) r.names.erase(r.names.begin() + std::ptrdiff_t(rng() % r.names.size())); break;
        case 4: r.items.push_back(Item{std::int32_t(rng()), std::int32_t(rng())}); break;
        case 5: if (r.items.size()) r.items.erase(r.items.begin()); break;
        case 6: for (int k = 0; k < 64; ++k) r.wides.push_back(rng()); break;
        case 7: r.wides.clear(); break;
        default: r.id = rng(); break;
        }
    }
}

// The reset buffer against a fresh one of the same logical size.
void check_fresh(XBuffer& b, ResetRoot& root) {
    assert(b.free_block_count() == 0 && b.free_block_bytes() == 0);
    auto fresh = XBuffer::create<ResetRoot>(b.get_size(), cap());
    assert(fresh.get_size() == b.get_size());
    assert(&root == &b.root<ResetRoot>());
    assert(std::memcmp(fresh.data(), b.data(), b.get_size()) == 0);
}

} // namespace

int main() {
    std::mt19937 rng(20261022);
    std::size_t resets = 0;

    // Random runs, each followed by a reset; the buffer grows across runs.
    {
        auto b = XBuffer::create<ResetRoot>(4096, cap());
        for (int run = 0; run < 200; ++run) {
            mutate(b.root<ResetRoot>(), rng, int(rng() % 400));
            auto& root = b.reset<ResetRoot>();
            check_fresh(b, root);
            ++resets;
        }
        // The kept, emptied table serves holes as a fresh one: the wire of a
        // run after a reset equals the same run in a fresh buffer.
        const auto seed = rng();
        std::mt19937 a_rng(seed), b_rng(seed);
        auto other = XBuffer::create<ResetRoot>(b.get_size(), cap());
        mutate(b.root<ResetRoot>(), a_rng, 500);
        mutate(other.root<ResetRoot>(), b_rng, 500);
        assert(b.free_block_count() == other.free_block_count());
        assert(b.save_verified<ResetRoot>() == other.save_verified<ResetRoot>());
    }

    // An unverified load whose bytes run past used_end: the bytes past it are
    // zeroed at load, so a later reset still exposes nothing.
    {
        auto src = XBuffer::create<ResetRoot>(4096, cap());
        mutate(src.root<ResetRoot>(), rng, 300);
        std::string bytes = src.save();
        const auto used = bytes.size();
        bytes.append(std::string(5000, '\xcd'));
        auto b = XBuffer::load_unverified(bytes, cap());
        assert(b.get_size() == bytes.size());
        for (std::size_t i = used; i < b.get_size(); ++i) assert(b.data()[i] == 0);
        auto& root = b.reset<ResetRoot>();
        check_fresh(b, root);
        ++resets;
    }

    // A failed reload leaves an empty arena; a reset after it is fresh too.
    {
        auto b = XBuffer::create<ResetRoot>(4096, cap());
        mutate(b.root<ResetRoot>(), rng, 300);
        std::string wire = b.save_verified<ResetRoot>();
        wire[wire.size() / 2] ^= 0x5a;
        try { b.reload_verified<ResetRoot>(wire); } catch (const XException&) {}
        auto& root = b.reset<ResetRoot>();
        check_fresh(b, root);
        ++resets;
    }

    // reset empties the bins' word summary too. A hole in bin word 1 before
    // the reset must not leave its summary bit set: the lookup would stop at
    // that empty word and miss a later word's hole, as in a fresh buffer.
    {
        auto b = XBuffer::create<ResetRoot>(1u << 16, cap());
        auto* before = b.arena()->allocate_aligned(1024, 8);   // bin 127, word 1
        b.arena()->allocate_aligned(64, 8);
        b.arena()->deallocate(before, 1024);
        assert(b.free_block_count() == 1);
        b.reset<ResetRoot>();
        auto fresh = XBuffer::create<ResetRoot>(b.get_size(), cap());
        for (auto* x : {&b, &fresh}) {
            auto* hole = x->arena()->allocate_aligned(2048, 8); // bin 255, word 3
            x->arena()->allocate_aligned(64, 8);
            x->arena()->deallocate(hole, 2048);
            assert(x->free_block_count() == 1);
            auto* small = x->arena()->allocate_aligned(16, 8);
            assert(small == hole && "a later word's hole was missed after reset");
        }
        assert(std::memcmp(fresh.data(), b.data(), b.get_size()) == 0);
        ++resets;
    }

    // The same for a reload: reload_verified releases the table to the
    // keep-recycle, whose fast path (RL, taken when no bin grew since the
    // last full recycle) must clear the word summary too. The first reload
    // takes the full path and keeps bin 127's storage; the second hole in
    // word 1 reuses that storage, so the second reload takes the fast path.
    {
        auto b = XBuffer::create<ResetRoot>(1u << 16, cap());
        const auto wire = XBuffer::create<ResetRoot>(b.get_size(), cap()).save_verified<ResetRoot>();
        for (int round = 0; round < 2; ++round) {
            auto* before = b.arena()->allocate_aligned(1024, 8);   // bin 127, word 1
            b.arena()->allocate_aligned(64, 8);
            b.arena()->deallocate(before, 1024);
            assert(b.free_block_count() == 1);
            b.reload_verified<ResetRoot>(wire);
        }
        auto fresh = XBuffer::create<ResetRoot>(b.get_size(), cap());
        fresh.reload_verified<ResetRoot>(wire);
        for (auto* x : {&b, &fresh}) {
            auto* hole = x->arena()->allocate_aligned(2048, 8); // bin 255, word 3
            x->arena()->allocate_aligned(64, 8);
            x->arena()->deallocate(hole, 2048);
            assert(x->free_block_count() == 1);
            auto* small = x->arena()->allocate_aligned(16, 8);
            assert(small == hole && "a later word's hole was missed after reload");
        }
        assert(b.save_verified<ResetRoot>() == fresh.save_verified<ResetRoot>());
    }

    std::cout << "test_reset_used_range: " << resets << " resets byte-identical to fresh buffers\n";
    return 0;
}
