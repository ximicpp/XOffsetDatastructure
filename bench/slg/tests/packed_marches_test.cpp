#include "xoffset_runner.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>

using namespace slg;
using Image = std::array<int64_t, 8>;

static std::vector<Image> contents(xm::ViewSnapshot& view, HeldIndex& index) {
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
    XOffsetRunnerT<XoTrustedModel, false>::BlockOps ops;
    ops.dview = &delta;
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
        slg::apply_delta(expected, b, delta);
        ops.apply_held_delta(actual, a);
        if (contents(actual, a) != contents(expected, b))
            throw std::runtime_error("packed march state differs from reference");
    }
    // Exercise a deletion batch larger than the expected number of vacancies.
    delta.removed_marches.clear();
    delta.marches.clear();
    for (int id = 1000; id < 1100; ++id)
        delta.marches.emplace_back().id = id;
    slg::apply_delta(expected, b, delta);
    ops.apply_held_delta(actual, a);
    delta.removed_marches.clear();
    delta.marches.clear();
    for (int id = 1000; id < 1065; ++id)
        delta.removed_marches.push_back(id);
    for (int id = 1030; id < 1085; ++id)
        delta.marches.emplace_back().id = id;
    slg::apply_delta(expected, b, delta);
    ops.apply_held_delta(actual, a);
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
            ops.apply_held_delta(view, index);
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
    std::puts("packed marches: 4000 differential batches and append failure passed");
}
