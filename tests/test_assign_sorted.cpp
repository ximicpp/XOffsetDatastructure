// XMap / XSet assign_sorted: the sorted bulk fill (one exact block, no
// lookup). Checks, against std models: contiguous same-layout ranges (one
// copy), std::pair ranges, std::map, a transform view, string keys and
// values, elements with heads, an empty input, a non-empty target (a fresh
// block), a source that is the target itself, and, under
// XOFFSET_CHECK_SORTED (on in these test builds, which keep assertions), that
// an unsorted or duplicated input throws and leaves the container unchanged.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace as {
struct Item {
    std::uint32_t id;
    std::uint32_t number;
    std::uint64_t uuid;
    std::uint64_t timestamp;
    bool operator==(const Item&) const = default;
};
struct Root {
    xo::XMap<std::uint64_t, Item> items;
    xo::XSet<std::uint32_t> ids;
    xo::XMap<xo::XString, xo::XString> names;
    xo::XSet<xo::XString> tags;
};
} // namespace as
XOFFSET_RECORD_DERIVED(as::Root, (), items, ids, names, tags)

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_assign_sorted FAILED: " << message << "\n";
    std::exit(1);
}
void check(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

as::Item item_for(std::uint64_t key) {
    return as::Item{std::uint32_t(key), std::uint32_t(key + 1), key * 7, key * 11};
}

template <typename Map, typename Model>
void check_items(const Map& map, const Model& model, const std::string& what) {
    check(map.size() == model.size(), what + ": size");
    auto it = model.begin();
    for (const auto& entry : map) {
        check(entry.first == it->first && entry.second == it->second, what + ": entry");
        ++it;
    }
    for (const auto& [key, value] : model) {
        const auto found = map.find(key);
        check(found != map.end() && found->second == value, what + ": find");
    }
}

void trivially_copyable_entries(as::Root& root) {
    std::vector<std::pair<std::uint64_t, as::Item>> pairs;
    for (std::uint64_t k = 1; k <= 64; ++k) pairs.emplace_back(k * 3, item_for(k));
    std::map<std::uint64_t, as::Item> model(pairs.begin(), pairs.end());

    root.items.assign_sorted(pairs);                       // std::pair entries, built in place
    check_items(root.items, model, "pairs");
    check(root.items.capacity() == 64, "one exact block");

    std::vector<xo::XKeyValue<std::uint64_t, as::Item>> same;
    for (const auto& [k, v] : pairs) same.push_back({k + 1, v});
    root.items.assign_sorted(same);                        // same layout: one copy, in place
    std::map<std::uint64_t, as::Item> shifted;
    for (const auto& [k, v] : pairs) shifted.emplace(k + 1, v);
    check_items(root.items, shifted, "same layout");

    root.items.assign_sorted(std::span<const xo::XKeyValue<std::uint64_t, as::Item>>(same).first(3));
    check_items(root.items, std::map<std::uint64_t, as::Item>(std::next(shifted.begin(), 0), std::next(shifted.begin(), 3)),
                "span prefix");

    root.items.assign_sorted(model);                       // std::map, non-empty target: fresh block
    check_items(root.items, model, "std::map");

    // A transform view: the application's entries converted one by one.
    root.items.assign_sorted(pairs | std::views::transform([](const auto& p) {
        return std::pair<std::uint64_t, as::Item>{p.first, as::Item{p.second.id, 9, 9, 9}};
    }));
    for (const auto& e : root.items) check(e.second.number == 9, "transform view value");
    check(root.items.size() == 64, "transform view size");

    // The target's own entries (a prefix) as the source.
    root.items.assign_sorted(root.items.begin(), root.items.begin() + 5);
    check(root.items.size() == 5 && root.items.begin()->first == 3, "self source");

    root.items.assign_sorted(std::vector<std::pair<std::uint64_t, as::Item>>{});
    check(root.items.empty(), "empty input clears");

    // Lookups and inserts keep working on the filled map.
    root.items.assign_sorted(pairs);
    check(root.items.try_emplace(4, item_for(4)).second && root.items.find(4)->second == item_for(4), "insert after fill");
    check(!root.items.try_emplace(6, item_for(0)).second, "duplicate after fill");

    std::vector<std::uint32_t> ids{1, 4, 9, 16, 25};
    root.ids.assign_sorted(ids);
    check(root.ids.size() == 5 && root.ids.contains(9) && !root.ids.contains(10), "set from vector");
    root.ids.assign_sorted(std::views::iota(0u, 100u));
    check(root.ids.size() == 100 && root.ids.contains(99), "set from iota");
    root.ids.assign_sorted(std::span<const std::uint32_t>(ids));
    check(root.ids.size() == 5 && *root.ids.begin() == 1, "set from span");
}

void entries_with_heads(as::Root& root) {
    const std::map<std::string, std::string> names{
        {"alpha", "short"}, {"beta", "a value longer than the inline limit"}, {"gamma", ""}};
    root.names.assign_sorted(names);
    check(root.names.size() == 3, "string map size");
    for (const auto& [k, v] : names) {
        const auto it = root.names.find(std::string_view(k));
        check(it != root.names.end() && it->second == v.c_str(), "string map entry");
    }
    root.names.assign_sorted(names);                       // non-empty target
    check(root.names.size() == 3 && root.names.find(std::string_view("beta"))->second ==
          "a value longer than the inline limit", "string map refill");

    std::vector<std::string> tags{"a", "bb", "c, a tag longer than the inline limit"};
    root.tags.assign_sorted(tags);
    check(root.tags.size() == 3 && root.tags.contains(std::string_view("bb")), "string set");
    root.tags.assign_sorted(root.tags | std::views::drop(1));   // the target's own elements
    check(root.tags.size() == 2 && !root.tags.contains(std::string_view("a")), "string set self source");
}

void checked_order(as::Root& root) {
#if XOFFSET_CHECK_SORTED
    auto expect_reject = [](auto&& call, const std::string& what) {
        bool threw = false;
        try {
            call();
        } catch (const xo::XException& e) {
            threw = e.code() == xo::XErrc::invalid_argument;
        }
        check(threw, what + ": rejected");
    };
    root.ids.assign_sorted(std::vector<std::uint32_t>{1, 2, 3});
    expect_reject([&] { root.ids.assign_sorted(std::vector<std::uint32_t>{3, 2}); }, "descending set");
    expect_reject([&] { root.ids.assign_sorted(std::vector<std::uint32_t>{2, 2}); }, "duplicate set");
    check(root.ids.size() == 3, "set unchanged after rejection");

    std::vector<xo::XKeyValue<std::uint64_t, as::Item>> dup{{1, {}}, {1, {}}};
    const auto before = root.items.size();
    expect_reject([&] { root.items.assign_sorted(dup); }, "duplicate map keys (same layout)");
    std::vector<std::pair<std::uint64_t, as::Item>> down{{2, {}}, {1, {}}};
    expect_reject([&] { root.items.assign_sorted(down); }, "descending map keys");
    check(root.items.size() == before, "map unchanged after rejection");
#else
    (void)root;
#endif
}
} // namespace

int main() {
    xo::XBuffer buffer(1 << 20);
    auto* root = buffer.make<as::Root>();
    trivially_copyable_entries(*root);
    entries_with_heads(*root);
    checked_order(*root);

    const auto bytes = buffer.get_buffer();
    xo::XBuffer loaded(bytes.data(), bytes.size());
    const auto& copy = loaded.unsafe_root<as::Root>();
    check(copy.items.size() == root->items.size() && copy.names.size() == root->names.size(), "reload sizes");

    std::cout << "test_assign_sorted: OK\n";
    return 0;
}
