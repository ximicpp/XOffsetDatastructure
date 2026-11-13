#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace slg {

// Minimal ID -> position index for HeldIndex, not a general unordered_map.
// Occupancy is separate so every int32_t key is valid. At most half the
// cells are occupied, so probing and backshift deletion always reach a hole.
// Growth and erase may invalidate iterators. Updating an existing key does
// not grow: apply_delta retains the removed key's iterator while updating
// the last element's position, then erases the retained iterator.
// emplace probes once when the table has room for one more key (the usual
// case) and checks for an existing key before it would grow.
class FlatHeldIndexMap {
    struct Cell {
        std::int32_t first = 0;
        std::int32_t second = 0;
        bool occupied = false;
    };

    std::vector<Cell> cells_;
    std::size_t count_ = 0;
    unsigned shift_ = 32;

    std::size_t mask() const { return cells_.size() - 1; }

    std::size_t home(std::int32_t key) const {
        return (std::uint32_t(key) * 0x9E3779B1u) >> shift_;
    }

    std::size_t probe(std::int32_t key) const {
        auto i = home(key);
        while (cells_[i].occupied && cells_[i].first != key)
            i = (i + 1) & mask();
        return i;
    }

public:
    using iterator = Cell*;

#ifdef SLG_XS2_DIAG
    // Diagnostic build only (not in scoring builds): read every line of the cells.
    std::uint64_t diag_touch(std::size_t stride) const {
        std::uint64_t h = 0;
        const auto* c = reinterpret_cast<const volatile unsigned char*>(cells_.data());
        for (std::size_t i = 0; i < cells_.size() * sizeof(Cell); i += stride) h += c[i];
        return h + cells_.size();
    }
#endif
    std::size_t size() const { return count_; }
    iterator end() { return nullptr; }

    iterator find(std::int32_t key) {
        if (cells_.empty()) return nullptr;
        const auto i = probe(key);
        return cells_[i].occupied ? &cells_[i] : nullptr;
    }

    void clear() {
        for (auto& c : cells_) c.occupied = false;
        count_ = 0;
    }

    void reserve(std::size_t n) {
        if (n <= cells_.size() / 2) return;
        if (n > (std::size_t(1) << 29))
            throw std::length_error("flat index limit");
        std::size_t cap = 32;
        while (cap < 2 * n) cap *= 2;
        std::vector<Cell> old(cap);
        old.swap(cells_);
        shift_ = 32;
        for (auto k = cap; k > 1; k >>= 1) --shift_;
        for (const auto& c : old)
            if (c.occupied) cells_[probe(c.first)] = c;
    }

    std::pair<iterator, bool> emplace(std::int32_t key, std::int32_t val) {
        // Grow only for a new key, so updating an existing key never moves cells.
        if (2 * (count_ + 1) > cells_.size()) {
            if (auto* p = find(key)) return {p, false};
            reserve(count_ + 1);
        }
        const auto i = probe(key);
        if (cells_[i].occupied) return {&cells_[i], false};
        cells_[i] = {key, val, true};
        ++count_;
        return {&cells_[i], true};
    }

    std::int32_t& operator[](std::int32_t key) {
        return emplace(key, 0).first->second;
    }

    void erase(iterator p) {
        if (!p) return;
        auto i = std::size_t(p - cells_.data());
        for (auto j = (i + 1) & mask(); cells_[j].occupied; j = (j + 1) & mask()) {
            const auto h = home(cells_[j].first);
            // Move a cell only if this hole lies on its original probe path.
            if (((j - h) & mask()) >= ((j - i) & mask())) {
                cells_[i] = cells_[j];
                i = j;
            }
        }
        cells_[i].occupied = false;
        --count_;
    }
};

} // namespace slg
