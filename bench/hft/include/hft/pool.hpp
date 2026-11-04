// HFT demo: the hand-written fixed-capacity pool, the way HFT code commonly
// keeps resident book state: per book, price levels in a sorted array and
// orders in a preallocated pool with a free list, each level's queue an
// intrusive doubly linked list, and an order-reference -> slot hash index.
// The memory is plain arrays, so a snapshot is a copy of them; the index is
// rebuilt by the receiver.
#pragma once
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "hft/apply.hpp"
#include "hft/model.hpp"

namespace hft {

// Capacity reserved per book up front (the arrays still grow past it).
inline constexpr std::size_t kPoolOrders = 8192, kPoolLevels = 256;

// Open addressing, linear probing, backward-shift deletion. Key 0 is empty
// (ITCH order references start at 1).
class IdIndex {
public:
    explicit IdIndex(std::size_t expected = 1 << 16) { rehash(expected * 2); }
    void clear() { std::fill(keys_.begin(), keys_.end(), 0); size_ = 0; }
    std::uint32_t& insert(std::uint64_t key) {
        if ((size_ + 1) * 2 > keys_.size()) rehash(keys_.size() * 2);
        std::size_t i = slot(key);
        while (keys_[i] != 0 && keys_[i] != key) i = (i + 1) & mask_;
        if (keys_[i] == 0) { keys_[i] = key; ++size_; }
        return vals_[i];
    }
    std::uint32_t find(std::uint64_t key) const {
        std::size_t i = slot(key);
        while (keys_[i] != key) i = (i + 1) & mask_;
        return vals_[i];
    }
    void erase(std::uint64_t key) {
        std::size_t i = slot(key);
        while (keys_[i] != key) i = (i + 1) & mask_;
        std::size_t j = i;
        for (;;) {
            j = (j + 1) & mask_;
            if (keys_[j] == 0) break;
            const std::size_t home = slot(keys_[j]);
            // Move j back to i when i lies cyclically in [home, j).
            if (((j - home) & mask_) >= ((j - i) & mask_)) {
                keys_[i] = keys_[j];
                vals_[i] = vals_[j];
                i = j;
            }
        }
        keys_[i] = 0;
        --size_;
    }
    std::size_t size() const { return size_; }

private:
    std::size_t slot(std::uint64_t key) const { return std::size_t((key * 0x9e3779b97f4a7c15ull) >> 20) & mask_; }
    void rehash(std::size_t n) {
        std::size_t cap = 16;
        while (cap < n) cap <<= 1;
        std::vector<std::uint64_t> old_keys = std::move(keys_);
        std::vector<std::uint32_t> old_vals = std::move(vals_);
        keys_.assign(cap, 0);
        vals_.assign(cap, 0);
        mask_ = cap - 1;
        size_ = 0;
        for (std::size_t i = 0; i < old_keys.size(); ++i)
            if (old_keys[i]) insert(old_keys[i]) = old_vals[i];
    }
    std::vector<std::uint64_t> keys_;
    std::vector<std::uint32_t> vals_;
    std::size_t mask_ = 0, size_ = 0;
};

struct PoolOrder {
    Order order;
    std::int32_t next;
    std::int32_t prev;
};
struct PoolLevel {
    std::int64_t price;
    std::uint64_t qty;
    std::uint32_t count;
    std::int32_t head;
    std::int32_t tail;
    std::uint32_t pad;
};
struct PoolBook {
    std::uint32_t instrument = 0;
    std::uint64_t symbol = 0;
    std::vector<PoolLevel> lv[2];  // [0] bids, [1] asks; best last
    std::vector<PoolOrder> pool;
    std::int32_t free_head = -1;
};

struct PoolState {
    std::uint64_t seq = 0;
    std::vector<PoolBook> books;
    std::vector<Row> rows;
    // Process-local: rebuilt from the arrays after a late join.
    std::vector<std::int32_t> book_of;
    IdIndex slot_of{1 << 18};

    void init(const std::vector<std::uint64_t>& symbols, const std::vector<std::uint32_t>& instruments,
              std::size_t order_capacity, std::size_t level_capacity) {
        rows.resize(symbols.size());
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            rows[i] = Row{};
            rows[i].symbol = symbols[i];
            rows[i].instrument = std::uint32_t(i);
        }
        for (auto ins : instruments) {
            PoolBook& b = books.emplace_back();
            b.instrument = ins;
            b.symbol = symbols[ins];
            b.pool.reserve(order_capacity);
            b.lv[0].reserve(level_capacity);
            b.lv[1].reserve(level_capacity);
        }
        index();
    }

    void index() {
        book_of.assign(rows.size(), -1);
        for (std::size_t i = 0; i < books.size(); ++i) book_of[books[i].instrument] = std::int32_t(i);
        slot_of.clear();
        for (const PoolBook& b : books)
            for (int s = 0; s < 2; ++s)
                for (const PoolLevel& L : b.lv[s])
                    for (std::int32_t o = L.head; o >= 0; o = b.pool[std::size_t(o)].next)
                        slot_of.insert(b.pool[std::size_t(o)].order.order_id) = std::uint32_t(o);
    }

    void apply(const Update& u) {
        Row& r = rows[u.instrument];
        update_row(r, u);
        const std::int32_t bi = book_of[u.instrument];
        if (bi >= 0 && u.action != 'T') {
            PoolBook& b = books[std::size_t(bi)];
            apply_book(b, u);
            r.best_bid = b.lv[0].empty() ? 0 : b.lv[0].back().price;
            r.best_ask = b.lv[1].empty() ? 0 : b.lv[1].back().price;
        }
        seq = u.seq;
    }

    void apply_book(PoolBook& b, const Update& u) {
        const Order& o = u.order;
        const bool bid = o.side == 'B';
        std::vector<PoolLevel>& lv = b.lv[bid ? 0 : 1];
        long li = find_level(lv, o.price, bid);
        if (u.action == 'A') {
            if (li < 0) {
                li = ~li;
                lv.insert(lv.begin() + li, PoolLevel{o.price, 0, 0, -1, -1, 0});
            }
            std::int32_t s;
            if (b.free_head >= 0) {
                s = b.free_head;
                b.free_head = b.pool[std::size_t(s)].next;
            } else {
                s = std::int32_t(b.pool.size());
                b.pool.push_back({});
            }
            PoolLevel& L = lv[std::size_t(li)];
            b.pool[std::size_t(s)] = PoolOrder{o, -1, L.tail};
            if (L.tail >= 0) b.pool[std::size_t(L.tail)].next = s;
            else L.head = s;
            L.tail = s;
            L.qty += o.qty;
            ++L.count;
            slot_of.insert(o.order_id) = std::uint32_t(s);
            return;
        }
        const auto s = std::int32_t(slot_of.find(o.order_id));
        PoolLevel& L = lv[std::size_t(li)];
        PoolOrder& po = b.pool[std::size_t(s)];
        L.qty -= o.qty;
        if (!(u.flags & kRemovesOrder)) {
            po.order.qty -= o.qty;
            return;
        }
        if (po.prev >= 0) b.pool[std::size_t(po.prev)].next = po.next;
        else L.head = po.next;
        if (po.next >= 0) b.pool[std::size_t(po.next)].prev = po.prev;
        else L.tail = po.prev;
        po.next = b.free_head;
        b.free_head = s;
        slot_of.erase(o.order_id);
        if (--L.count == 0) lv.erase(lv.begin() + li);
    }

    std::uint64_t digest() const {
        std::uint64_t h = mix(1469598103934665603ull, seq);
        for (const PoolBook& b : books) {
            h = mix(mix(h, b.instrument), b.symbol);
            for (int s = 0; s < 2; ++s) {
                h = mix(h, b.lv[s].size());
                for (long i = long(b.lv[s].size()) - 1; i >= 0; --i) {
                    const PoolLevel& L = b.lv[s][std::size_t(i)];
                    h = mix(mix(mix(h, std::uint64_t(L.price)), L.qty), L.count);
                    for (std::int32_t o = L.head; o >= 0; o = b.pool[std::size_t(o)].next)
                        h = digest_order(h, b.pool[std::size_t(o)].order);
                }
            }
        }
        for (const Row& r : rows) h = digest_row(h, r);
        return h;
    }

    // Snapshot: the arrays as they are (levels and the used part of each
    // pool, free slots included), with a small header per book.
    void snapshot(std::string& out) const {
        auto put = [&](const void* p, std::size_t n) { out.append(static_cast<const char*>(p), n); };
        out.clear();
        const std::uint64_t head[3] = {seq, books.size(), rows.size()};
        put(head, sizeof head);
        for (const PoolBook& b : books) {
            const std::uint64_t bh[6] = {b.instrument, b.symbol, b.lv[0].size(), b.lv[1].size(), b.pool.size(),
                                         std::uint64_t(std::int64_t(b.free_head))};
            put(bh, sizeof bh);
            put(b.lv[0].data(), b.lv[0].size() * sizeof(PoolLevel));
            put(b.lv[1].data(), b.lv[1].size() * sizeof(PoolLevel));
            put(b.pool.data(), b.pool.size() * sizeof(PoolOrder));
        }
        put(rows.data(), rows.size() * sizeof(Row));
    }

    // Trusts its input (a hand-written format has no validator).
    void restore(const std::string& in, std::size_t order_capacity, std::size_t level_capacity) {
        const char* p = in.data();
        auto get = [&](void* dst, std::size_t n) { std::memcpy(dst, p, n); p += n; };
        std::uint64_t head[3];
        get(head, sizeof head);
        seq = head[0];
        books.assign(head[1], PoolBook{});
        for (PoolBook& b : books) {
            std::uint64_t bh[6];
            get(bh, sizeof bh);
            b.instrument = std::uint32_t(bh[0]);
            b.symbol = bh[1];
            b.lv[0].reserve(std::max<std::size_t>(level_capacity, bh[2]));
            b.lv[1].reserve(std::max<std::size_t>(level_capacity, bh[3]));
            b.pool.reserve(std::max<std::size_t>(order_capacity, bh[4]));
            b.lv[0].resize(bh[2]);
            b.lv[1].resize(bh[3]);
            b.pool.resize(bh[4]);
            b.free_head = std::int32_t(std::int64_t(bh[5]));
            get(b.lv[0].data(), bh[2] * sizeof(PoolLevel));
            get(b.lv[1].data(), bh[3] * sizeof(PoolLevel));
            get(b.pool.data(), bh[4] * sizeof(PoolOrder));
        }
        rows.resize(head[2]);
        get(rows.data(), head[2] * sizeof(Row));
        index();
    }
};

}  // namespace hft
