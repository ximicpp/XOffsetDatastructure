// Differential test of FlatHeldIndexMap (include/slg/flat_held_index.hpp) against
// std::unordered_map: 200000 random insert / update / find / erase / reserve / clear /
// copy operations over a small key range plus the int32 extremes, then the
// apply_delta pattern (update an existing key while an iterator to another key is
// held, then erase through that iterator). Asserts stay on in every build type.
#undef NDEBUG
#include "flat_held_index.hpp"

#include <cassert>
#include <climits>
#include <cstdint>
#include <iostream>
#include <unordered_map>

namespace {

using Ref = std::unordered_map<std::int32_t, std::int32_t>;

void check_all(slg::FlatHeldIndexMap& a, const Ref& b) {
    assert(a.size() == b.size());
    for (const auto& kv : b) {
        auto* p = a.find(kv.first);
        assert(p && p->second == kv.second);
    }
}

void random_ops() {
    slg::FlatHeldIndexMap a;
    Ref b;
    std::uint64_t seed = 736271;
    auto rng = [&] { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; };
    for (int i = 0; i < 200000; ++i) {
        const auto r = rng();
        std::int32_t key = std::int32_t(r % 1024) - 512;
        if (i % 31 == 0) key = INT32_MIN;
        if (i % 37 == 0) key = INT32_MAX;
        const auto val = std::int32_t(r >> 32);
        switch (r % 5) {
        case 0: {
            const auto x = a.emplace(key, val);
            const auto y = b.emplace(key, val);
            assert(x.second == y.second && x.first->second == y.first->second);
            break;
        }
        case 1: {
            auto* x = a.find(key);
            const auto y = b.find(key);
            assert((x == a.end()) == (y == b.end()));
            if (x) a.erase(x);
            b.erase(key);
            break;
        }
        case 2:
            a[key] = val;
            b[key] = val;
            break;
        case 3:
            a.reserve((r >> 20) % 2048);
            break;
        default: {
            auto* x = a.find(key);
            const auto y = b.find(key);
            assert((x == a.end()) == (y == b.end()));
            if (x) assert(x->second == y->second);
        }
        }
        assert(a.size() == b.size());
        if (i % 257 == 0) {
            check_all(a, b);
            auto c = a;
            check_all(c, b);
        }
        if (i % 10007 == 0) { a.clear(); b.clear(); }
    }
}

// apply_delta (include/slg/view.hpp): keep the removed id's iterator, move the last
// record into its place (update that record's position, an existing key), then erase
// the kept iterator. The update must not grow the table, so the iterator stays valid.
void apply_delta_pattern() {
    slg::FlatHeldIndexMap a;
    Ref b;
    std::uint64_t seed = 99;
    auto rng = [&] { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; };
    for (std::int32_t n = 1; n <= 300; ++n) {
        a.clear(); b.clear();
        // n = 1..300 covers every fill level, among them exactly half full (a new key would grow).
        for (std::int32_t k = 0; k < n; ++k) { a.emplace(k * 7, k); b.emplace(k * 7, k); }
        for (int step = 0; step < 50 && !b.empty(); ++step) {
            const std::int32_t pos = std::int32_t(rng() % b.size()), last = std::int32_t(b.size()) - 1;
            std::int32_t gone = 0, moved = 0;
            for (const auto& kv : b) {
                if (kv.second == pos) gone = kv.first;
                if (kv.second == last) moved = kv.first;
            }
            auto* it = a.find(gone);
            assert(it && it->second == pos);
            if (pos != last) { a[moved] = pos; b[moved] = pos; }
            assert(it->first == gone && it->second == pos);
            a.erase(it);
            b.erase(gone);
            check_all(a, b);
        }
    }
}

} // namespace

int main() {
    random_ops();
    apply_delta_pattern();
    std::cout << "flat index random differential: 200000 operations passed\n";
}
