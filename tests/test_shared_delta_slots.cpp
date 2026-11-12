// xsync::XBasicSyncSlots (XSyncSlots), the id -> slot index a client of
// XReceiver::apply_sync keeps per keyed table: random enters and leaves
// against a std::unordered_map model, both reuse orders, ids at the edges of
// 32 bits (0, 0x80000000, 0xFFFFFFFF, ids whose hashes collide), growth,
// clear, and that every slot given out is below size() and held by one id.
// No exception of its own: also built as test_shared_delta_slots_noexc.
#include "xoffset_shared_delta.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

namespace {
namespace xs = XOffsetDatastructure::xsync;

[[noreturn]] void fail(const char* what, long step) {
    std::fprintf(stderr, "test_shared_delta_slots: %s (step %ld)\n", what, step);
    std::exit(1);
}

template <xs::XSlotReuse R>
struct Model {
    xs::XBasicSyncSlots<R> slots;
    std::unordered_map<std::uint32_t, std::int64_t> held;
    std::vector<std::int64_t> free;   // the model's free slots
    std::int64_t grown = 0;           // grow calls, in order

    std::int64_t enter(std::uint32_t id, long step) {
        const std::int64_t s = slots.enter(id, [&](std::int64_t n) {
            if (n != grown) fail("grow got a slot out of order", step);
            ++grown;
        });
        if (held.count(id)) {
            if (s != -1) fail("enter of a held id gave a slot", step);
            return -1;
        }
        if (s < 0 || s >= grown) fail("enter gave no slot or one past size", step);
        // The slot the reuse order picks.
        if (!free.empty()) {
            std::int64_t want;
            if constexpr (R == xs::XSlotReuse::lowest) {
                const auto it = std::min_element(free.begin(), free.end());
                want = *it;
                free.erase(it);
            } else {
                want = free.back();
                free.pop_back();
            }
            if (s != want) fail("enter did not reuse the expected free slot", step);
        } else if (s != grown - 1) fail("enter did not take the new slot", step);
        held.emplace(id, s);
        return s;
    }
    void leave(std::uint32_t id, long step) {
        const auto it = held.find(id);
        if (it == held.end()) fail("model: leave of an id not held", step);
        if (slots.find(id) != it->second) fail("find before leave", step);
        slots.leave(id, it->second);
        free.push_back(it->second);
        held.erase(it);
        if (slots.find(id) != -1) fail("find after leave", step);
    }
    void check(long step, const std::vector<std::uint32_t>& probe) {
        if (slots.count() != held.size()) fail("count", step);
        if (slots.size() != std::size_t(grown)) fail("size", step);
        std::set<std::int64_t> used;
        for (const auto& [id, s] : held) {
            if (slots.find(id) != s) fail("find of a held id", step);
            if (!used.insert(s).second) fail("two ids hold one slot", step);
        }
        for (std::int64_t s : free)
            if (!used.insert(s).second) fail("a free slot is held", step);
        if (used.size() != std::size_t(grown)) fail("a slot is neither held nor free", step);
        for (std::uint32_t id : probe)
            if (!held.count(id) && slots.find(id) != -1) fail("find of an id not held", step);
    }
    void clear() {
        slots.clear();
        held.clear();
        free.clear();
        if constexpr (R == xs::XSlotReuse::lowest)
            for (std::int64_t s = 0; s < grown; ++s) free.push_back(s);
        else
            for (std::int64_t s = grown; s-- > 0;) free.push_back(s);
    }
};

// Ids that share a home cell in a table of 2^bits cells: (id * 0x9E3779B1) >> (32 - bits) equal.
std::vector<std::uint32_t> colliding(unsigned bits, std::size_t n) {
    std::vector<std::uint32_t> out;
    const std::uint32_t want = 5;
    for (std::uint32_t id = 1; out.size() < n; ++id)
        if (((id * 0x9E3779B1u) >> (32 - bits)) == want) out.push_back(id);
    return out;
}

template <xs::XSlotReuse R>
void run(std::uint32_t seed) {
    Model<R> m;
    std::mt19937 rng(seed);
    // A pool of ids: edges of 32 bits, ids colliding in small and larger tables, random ones.
    std::vector<std::uint32_t> pool = {0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0x80000001u, 0xFFFFFFFEu, 0xFFFFFFFFu};
    for (std::uint32_t id : colliding(5, 24)) pool.push_back(id);
    for (std::uint32_t id : colliding(9, 24)) pool.push_back(id);
    while (pool.size() < 1200) pool.push_back(std::uint32_t(rng()));
    std::sort(pool.begin(), pool.end());
    pool.erase(std::unique(pool.begin(), pool.end()), pool.end());

    if (m.slots.find(7) != -1 || m.slots.size() != 0 || m.slots.count() != 0) fail("empty index", 0);
    m.clear();   // clear of an empty index
    long step = 0;
    for (int phase = 0; phase < 6; ++phase) {
        // Fill towards a target, then drain, so the table grows and runs wrap.
        const std::size_t target = std::size_t(40) << phase;
        for (int k = 0; k < 4000; ++k, ++step) {
            const std::uint32_t id = pool[rng() % pool.size()];
            const bool grow_phase = m.held.size() < target;
            if (m.held.count(id) ? (rng() % 4 == 0 || !grow_phase) : (grow_phase || rng() % 3 == 0)) {
                if (m.held.count(id)) m.leave(id, step);
                else m.enter(id, step);
            } else {
                m.enter(id, step);   // either a fresh enter or the held case (-1)
            }
            if (k % 97 == 0) m.check(step, pool);
        }
        m.check(step, pool);
        // A held id: enter again gives -1, take gives its slot.
        if (!m.held.empty()) {
            const auto [id, s] = *m.held.begin();
            bool added = true;
            const std::int64_t t = m.slots.take(id, [&](std::int64_t) { fail("take of a held id grew", step); }, added);
            if (added || t != s) fail("take of a held id", step);
        }
        if (phase == 3) {
            const std::size_t before = m.slots.capacity_bytes();
            m.clear();
            m.check(step, pool);
            if (m.slots.capacity_bytes() < before) fail("clear dropped storage", step);
        }
    }
    // Drain all.
    std::vector<std::uint32_t> ids;
    for (const auto& e : m.held) ids.push_back(e.first);
    for (std::uint32_t id : ids) m.leave(id, ++step);
    m.check(step, pool);
    if (m.slots.count() != 0) fail("not empty after draining", step);
}

// take on an id not held: added, same slot as enter would give.
void take_basics() {
    xs::XSyncSlots s;
    std::size_t grows = 0;
    auto g = [&](std::int64_t) { ++grows; };
    bool aa = false, ba = false, ca = true;
    const std::int64_t a = s.take(0x80000000u, g, aa);
    const std::int64_t b = s.take(0u, g, ba);
    const std::int64_t c = s.take(0x80000000u, g, ca);
    if (!aa || a != 0 || !ba || b != 1 || ca || c != 0 || grows != 2) fail("take", 0);
    s.leave(0x80000000u, 0);
    if (s.enter(0xFFFFFFFFu, g) != 0 || grows != 2) fail("reuse after leave", 0);
    if (s.find(0x80000000u) != -1 || s.find(0u) != 1 || s.find(0xFFFFFFFFu) != 0) fail("find", 0);
}
} // namespace

int main() {
    take_basics();
    for (std::uint32_t seed : {1u, 2u, 3u, 20261112u}) {
        run<xs::XSlotReuse::last>(seed);
        run<xs::XSlotReuse::lowest>(seed);
    }
    std::puts("test_shared_delta_slots: ok");
    return 0;
}
