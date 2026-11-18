// Single-element inserts of trivially copyable elements stage the element at
// the caller and grow out of line without it (grow_back_slot, grow_gap_slot).
// Checks, against std models: sorted appends and inserts below the last key
// through many growths (XMap, XSet), push_back of elements above 16 bytes,
// emplace_back with arguments, and the aliasing cases the staging must keep
// valid: an argument that refers to an element of the full container, whose
// block the growth retires and zeroes.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace si {
struct Item {
    std::uint32_t id;
    std::uint32_t number;
    std::uint64_t uuid;
    std::uint64_t timestamp;
    bool operator==(const Item&) const = default;
};
// 40 bytes: push_back of a trivially copyable element above 16 bytes.
struct Big {
    std::uint64_t a, b, c, d;
    std::uint32_t e;
    bool operator==(const Big&) const = default;
};
// 12 bytes: the by-value path of push_back stays as it was.
struct Small {
    float x, y, z;
    bool operator==(const Small&) const = default;
};
struct Root {
    xo::XMap<std::uint64_t, Item> items;
    xo::XSet<std::uint64_t> keys;
    xo::XVector<Big> bigs;
    xo::XVector<Small> smalls;
};
} // namespace si
XOFFSET_RECORD(si::Item, id, number, uuid, timestamp)
XOFFSET_RECORD(si::Big, a, b, c, d, e)
XOFFSET_RECORD(si::Small, x, y, z)
XOFFSET_RECORD_DERIVED(si::Root, (), items, keys, bigs, smalls)

static_assert(std::is_trivially_copyable_v<xo::XKeyValue<std::uint64_t, si::Item>>);
static_assert(std::is_trivially_copyable_v<si::Big> && sizeof(si::Big) > 16);

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_staged_insert FAILED: " << message << "\n";
    std::exit(1);
}
void check(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

si::Item item_for(std::uint64_t key) {
    return si::Item{std::uint32_t(key), std::uint32_t(key * 3), key * 7, key * 11};
}

template <typename Map, typename Model>
void check_map(const Map& map, const Model& model, const std::string& what) {
    check(map.size() == model.size(), what + ": size");
    auto it = model.begin();
    for (const auto& entry : map) {
        check(entry.first == it->first && entry.second == it->second, what + ": entry");
        ++it;
    }
}

void sorted_inserts(si::Root& root) {
    std::map<std::uint64_t, si::Item> model;
    // Ascending: every insert after the last key, through every growth.
    for (std::uint64_t k = 10; k < 10 + 200 * 4; k += 4) {
        // A temporary built field by field, as an application converting its data.
        const auto r = root.items.try_emplace(k, si::Item{std::uint32_t(k), std::uint32_t(k * 3), k * 7, k * 11});
        check(r.second && r.first->first == k, "ascending try_emplace result");
        model.emplace(k, item_for(k));
    }
    check_map(root.items, model, "ascending");
    // Below the last key, including in a full map: the gap path.
    for (std::uint64_t k = 12; k < 10 + 200 * 4; k += 40) {
        root.items.try_emplace(k, item_for(k));
        model.emplace(k, item_for(k));
        check_map(root.items, model, "insert below last");
    }
    // Duplicates change nothing.
    check(!root.items.try_emplace(10, si::Item{}).second, "duplicate key");
    check_map(root.items, model, "after duplicate");

    std::set<std::uint64_t> keys;
    for (std::uint64_t k = 1; k < 300; k += 2) {
        check(root.keys.insert(k).second, "set ascending insert");
        keys.insert(k);
    }
    for (std::uint64_t k = 0; k < 300; k += 50) {
        root.keys.insert(k);
        keys.insert(k);
    }
    check(root.keys.size() == keys.size(), "set size");
    auto s = keys.begin();
    for (auto v : root.keys) check(v == *s++, "set order");
}

// The value refers to an element of the full map: the growth retires (and
// zeroes) the block it lies in, so it must be staged first.
void aliasing_map(si::Root& root) {
    auto& m = root.items;
    m.clear();
    std::map<std::uint64_t, si::Item> model;
    std::uint64_t key = 2;   // even keys: the key below the last one is always free
    auto add = [&](std::uint64_t k, const si::Item& value) { m.try_emplace(k, value); model.emplace(k, value); };
    auto fill = [&] { while (m.size() < m.capacity()) { add(key, item_for(key)); key += 2; } };
    add(key, item_for(key));
    key += 2;
    for (int round = 0; round < 3; ++round) {
        fill();
        const si::Item first = m.begin()->second;
        const auto appended = m.try_emplace(key, m.begin()->second);   // after the last key
        check(appended.second && appended.first->second == first, "append aliasing an old element");
        model.emplace(key, first);
        key += 2;
        fill();
        const si::Item last = (m.end() - 1)->second;
        const std::uint64_t below = (m.end() - 1)->first - 1;
        const auto inserted = m.try_emplace(below, (m.end() - 1)->second);   // below the last key
        check(inserted.second && inserted.first->first == below && inserted.first->second == last,
              "insert below last aliasing an old element");
        model.emplace(below, last);
    }
    check_map(m, model, "aliasing");
}

void push_back_big(si::Root& root) {
    auto& v = root.bigs;
    std::vector<si::Big> model;
    // Aliasing: push_back of an element of the full vector, emplace_back from its fields.
    for (int round = 0; round < 3; ++round) {
        while (v.size() < v.capacity() || v.empty()) { v.push_back(si::Big{1, 2, 3, 4, 5}); model.push_back(v.back()); }
        const si::Big front = v[0];
        v.push_back(v[0]);
        check(v.back() == front, "push_back aliasing an old element");
        model.push_back(front);
        while (v.size() < v.capacity()) { v.push_back(si::Big{6, 7, 8, 9, 10}); model.push_back(v.back()); }
        const si::Big second = v[1];
        v.emplace_back(v[1].a, v[1].b, v[1].c, v[1].d, v[1].e);
        check(v.back() == second, "emplace_back aliasing an old element");
        model.push_back(second);
    }
    for (std::uint64_t i = 0; i < 300; ++i) {
        si::Big b{};
        b.a = i; b.b = i * 2; b.c = i * 3; b.d = i * 5; b.e = std::uint32_t(i * 7);
        if (i % 3 == 0) v.push_back(b);
        else if (i % 3 == 1) v.push_back(si::Big{i, i * 2, i * 3, i * 5, std::uint32_t(i * 7)});
        else v.emplace_back(i, i * 2, i * 3, i * 5, std::uint32_t(i * 7));
        model.push_back(b);
    }
    check(v.size() == model.size(), "big size");
    for (std::size_t i = 0; i < model.size(); ++i) check(v[std::uint32_t(i)] == model[i], "big element");
}

void push_back_small(si::Root& root) {
    auto& v = root.smalls;
    for (int round = 0; round < 3; ++round) {
        while (v.size() < v.capacity() || v.empty()) v.push_back(si::Small{float(v.size()), 1, 2});
        const si::Small first = v[0];
        v.emplace_back(v[0].x, v[0].y, v[0].z);
        check(v.back() == first, "small emplace_back aliasing an old element");
    }
    for (int i = 0; i < 100; ++i) v.emplace_back(float(i), float(i + 1), float(i + 2));
    check(v[v.size() - 95] == (si::Small{5, 6, 7}), "small element");
}
} // namespace

int main() {
    xo::XBuffer buffer(1 << 20);
    auto* root = buffer.make<si::Root>();
    sorted_inserts(*root);
    aliasing_map(*root);
    push_back_big(*root);
    push_back_small(*root);

    // The content survives a save and a load.
    const auto bytes = buffer.get_buffer();
    xo::XBuffer loaded(bytes.data(), bytes.size());
    const auto& copy = loaded.unsafe_root<si::Root>();
    check(copy.items.size() == root->items.size() && copy.bigs.size() == root->bigs.size(), "reload sizes");
    auto it = root->items.begin();
    for (const auto& e : copy.items) { check(e.first == it->first && e.second == it->second, "reload entry"); ++it; }

    std::cout << "test_staged_insert: OK\n";
    return 0;
}
