#include "xoffset_runner.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>

using namespace slg;
using Image = std::array<int64_t, 8>;

template <class V> static std::vector<Image> contents(V& view, HeldIndex& index) {
    std::vector<Image> rows;
    for (auto& march : view.marches) {
        const auto entry = index.marches.find(march.id);
        if (!entry || &view.marches[std::size_t(entry->second)] != &march)
            throw std::runtime_error("march position mismatch");
        rows.push_back({march.id, march.owner, march.x0, march.y0, march.x1, march.y1, march.t0, march.t1});
    }
    if (index.marches.size() != rows.size())
        throw std::runtime_error("stale march index entry");
    std::sort(rows.begin(), rows.end());
    return rows;
}
// Reference: the swap-and-erase march update, one record at a time.
template <class V, class D> static void reference_marches(V& v, HeldIndex& at, const D& d) {
    for (const auto& id : d.removed_marches) {
        const auto it = at.marches.find(id);
        if (it == at.marches.end()) continue;
        const int32_t pos = it->second, last = int32_t(v.marches.size()) - 1;
        if (pos != last) {
            copy_vmarch(v.marches[std::size_t(pos)], v.marches[std::size_t(last)]);
            at.marches[v.marches[std::size_t(pos)].id] = pos;
        }
        v.marches.pop_back();
        at.marches.erase(it);
    }
    for (const auto& m : d.marches) {
        const auto it = at.marches.find(m.id);
        if (it != at.marches.end()) copy_vmarch(v.marches[std::size_t(it->second)], m);
        else {
            at.marches.emplace(m.id, int32_t(v.marches.size()));
            copy_vmarch(v.marches.emplace_back(), m);
        }
    }
}

static void same_arena_held_unit_regression() {
    using XBuffer = XOffsetDatastructure::XBuffer;
    const auto cap = XBuffer::maximum_capacity(256 * 1024);
    auto test = XBuffer::create<xm::ViewSnapshot>(32 * 1024, cap);
    auto updates = XBuffer::create<xm::ViewDelta>(32 * 1024, cap);
    auto& actual = test.root<xm::ViewSnapshot>();
    stdm::ViewSnapshot expected;
    HeldIndex actual_index, expected_index;
    XOffsetRunnerT<XoTrustedModel, false>::BlockOps ops;

    const auto add = [](xm::ViewDelta& delta, int32_t id, char letter) {
        WorldUnit unit;
        unit.id = id;
        unit.hp = id * 23;
        unit.name.assign(72, letter);
        unit.alliance_tag.assign(40, char(letter + 1));
        unit.owner_name.assign(56, char(letter + 2));
        unit.commander = {id, 2, 3, 4};
        for (int32_t i = 0; i < 5; ++i) {
            unit.heroes.push_back({id + i, i, i + 1, i + 5});
            unit.troops.push_back({i, id + i, i + 1});
            unit.buffs.push_back({id - i, 3 * i, 104000 + 2000 * i});
        }
        copy_unit(delta.upserts.emplace_back(), unit);
    };
    const auto apply = [&](xm::ViewDelta& delta) {
        slg::apply_delta(expected, expected_index, delta);
        const auto wire = updates.save_verified<xm::ViewDelta>();
        ops.delta_recv_from(wire);
        slg::apply_delta(actual, actual_index, ops.delta());
    };
    const auto compare = [&](const auto& units) {
        if (units.size() != expected.units.size())
            throw std::runtime_error("held unit count changed");
        for (const auto& expected_unit : expected.units) {
            const auto where = actual_index.units.find(expected_unit.id);
            if (!where || where->second < 0 || std::size_t(where->second) >= units.size())
                throw std::runtime_error("held unit index lost a live unit");
            const auto& unit = units[std::size_t(where->second)];
            if (unit.id != expected_unit.id || query_unit(unit) != query_unit(expected_unit) ||
                slg::sv(unit.name) != slg::sv(expected_unit.name) ||
                slg::sv(unit.alliance_tag) != slg::sv(expected_unit.alliance_tag) ||
                slg::sv(unit.owner_name) != slg::sv(expected_unit.owner_name))
                throw std::runtime_error("held unit or nested storage differs from reference");
        }
    };

    auto& first = updates.root<xm::ViewDelta>();
    for (int32_t id = 1; id <= 4; ++id) add(first, id, char('a' + id));
    apply(first);

    auto& second = updates.reset<xm::ViewDelta>();
    for (int32_t id : {2, 2, 4, 999}) second.removed.push_back(id);
    add(second, 2, 'p');
    apply(second);

    if (actual_index.units.size() != expected.units.size() ||
        actual_index.units.find(4) || actual_index.units.find(999))
        throw std::runtime_error("held unit index kept a removed unit");
    compare(actual.units);

    const auto roundtrip = test.save_verified<xm::ViewSnapshot>();
    auto checked = XBuffer::try_load_verified<xm::ViewSnapshot>(roundtrip, cap);
    if (!checked) throw std::runtime_error("held unit move broke arena ownership");
    compare(checked->root<xm::ViewSnapshot>().units);
}


int main() {
    using XBuffer = XOffsetDatastructure::XBuffer;
    using XOffsetDatastructure::XGrowPolicy;
    const auto cap = XBuffer::maximum_capacity(512 * 1024).with_growth(XGrowPolicy::Automatic);
    auto test = XBuffer::create<xm::ViewSnapshot>(4096, cap);
    auto control = XBuffer::create<xm::ViewSnapshot>(4096, cap);
    auto updates = XBuffer::create<xm::ViewDelta>(4096, cap);
    auto& actual = test.root<xm::ViewSnapshot>();
    auto& expected = control.root<xm::ViewSnapshot>();
    auto& delta = updates.root<xm::ViewDelta>();
    HeldIndex a, b;
    stdm::ViewSnapshot std_view;
    HeldIndex si;
    Rng random(20261115);
    for (int i = 0; i < 4000; ++i) {
        delta.removed_marches.clear();
        delta.marches.clear();
        const int removed = random.below(30);
        const int written = random.below(30);
        for (int j = 0; j < removed; ++j)
            delta.removed_marches.push_back(random.below(128) - 64);
        for (int j = 0; j < written; ++j) {
            auto& record = delta.marches.emplace_back();
            record.id = random.below(128) - 64;
            record.owner = j;
            record.x0 = i;
            record.y0 = -i;
            record.x1 = j + 1;
            record.y1 = -j;
            record.t0 = i * 11;
            record.t1 = i * 13 + j;
        }
        if (i == 0) {
            // Explicit remove-and-reinsert and duplicated IDs in both lists.
            delta.removed_marches.push_back(5);
            delta.removed_marches.push_back(5);
            delta.marches.emplace_back().id = 5;
            delta.marches.emplace_back().id = 5;
        }
        reference_marches(expected, b, delta);
        slg::apply_delta(actual, a, delta);
        slg::apply_delta(std_view, si, delta);
        if (contents(actual, a) != contents(expected, b) || contents(std_view, si) != contents(expected, b))
            throw std::runtime_error("packed march state differs from reference");
    }
    // Exercise a deletion batch larger than the expected number of vacancies.
    delta.removed_marches.clear();
    delta.marches.clear();
    for (int id = 1000; id < 1100; ++id)
        delta.marches.emplace_back().id = id;
    reference_marches(expected, b, delta);
    slg::apply_delta(actual, a, delta);
    delta.removed_marches.clear();
    delta.marches.clear();
    for (int id = 1000; id < 1065; ++id)
        delta.removed_marches.push_back(id);
    for (int id = 1030; id < 1085; ++id)
        delta.marches.emplace_back().id = id;
    reference_marches(expected, b, delta);
    slg::apply_delta(actual, a, delta);
    if (contents(actual, a) != contents(expected, b))
        throw std::runtime_error("large deletion batch differs from reference");

    // Force the append to fail in a fixed-size arena once the vector is full.
    const auto fixed = XBuffer::maximum_capacity(4096).with_growth(XGrowPolicy::Explicit);
    auto tight = XBuffer::create<xm::ViewSnapshot>(4096, fixed);
    auto& view = tight.root<xm::ViewSnapshot>();
    HeldIndex index;
    bool rejected = false;
    int32_t live = 0;
    int32_t next = 1;
    for (int i = 0; i < 4096; ++i) {
        delta.removed_marches.clear();
        delta.marches.clear();
        const auto size_before = view.marches.size();
        const bool recycle = size_before > 16;
        if (recycle) {
            delta.removed_marches.push_back(live);
            delta.marches.emplace_back().id = next++;
        }
        const auto append = next++;
        delta.marches.emplace_back().id = append;
        try {
            slg::apply_delta(view, index, delta);
        } catch (const std::exception&) {
            rejected = true;
            if (!recycle || index.marches.find(live) || !index.marches.find(append - 1) ||
                index.marches.find(append) || view.marches.size() != size_before)
                throw std::runtime_error("failed append broke the recycled view");
            (void)contents(view, index);
            break;
        }
        live = append;
    }
    if (!rejected) throw std::runtime_error("fixed-size view never reached capacity");
    same_arena_held_unit_regression();
    std::puts("packed marches: 4000 differential batches and append failure passed");
}
