// HFT demo: applying normalized updates to the state, for every state model.
//
// The XOffset state, the std model and the cista model have the same shape
// (books of levels of order queues, plus the metrics table), so one template
// applies an update to any of them: the difference measured is the storage.
// The hand-written pool (pool.hpp) has its own code.
//
// The book of one instrument is found through a process-local index
// (instrument -> book), rebuilt from the state after a late join. An order is
// found by scanning its price level.
#pragma once
#include <cstdint>
#include <vector>

#include "hft/model.hpp"

namespace hft {

// Levels are sorted best-last (bids ascending, asks descending): updates
// cluster at the best prices, so the scan from the end is short and an
// insertion near the top moves few levels. Returns the level's index, or
// ~(insertion index) when the price has no level.
template <class Levels>
inline long find_level(const Levels& lv, std::int64_t px, bool bid) {
    long i = long(lv.size()) - 1;
    for (; i >= 0; --i) {
        const std::int64_t p = lv[std::size_t(i)].price;
        if (p == px) return i;
        if (bid ? p < px : p > px) break;
    }
    return ~(i + 1);
}

inline void update_row(Row& r, const Update& u) {
    const Order& o = u.order;
    switch (u.action) {
    case 'A':
        ++r.adds;
        ++r.live_orders;
        r.live_shares += o.qty;
        break;
    case 'F':
    case 'T':
        if (u.action == 'F') r.live_shares -= o.qty;
        if (u.action == 'F' && (u.flags & kRemovesOrder)) --r.live_orders;
        r.volume += o.qty;
        ++r.trades;
        r.last_price = o.price;
        if (o.price > r.high) r.high = o.price;
        if (r.low == 0 || o.price < r.low) r.low = o.price;
        break;
    default:  // 'C', 'D'
        ++r.removes;
        r.live_shares -= o.qty;
        if (u.flags & kRemovesOrder) --r.live_orders;
        break;
    }
    r.last_ts = o.ts;
}

// One book; Book is hft::Book, StdBook or the cista book.
template <class B>
inline void apply_book(B& b, const Update& u) {
    const Order& o = u.order;
    const bool bid = o.side == 'B';
    auto& lv = bid ? b.bids : b.asks;
    long li = find_level(lv, o.price, bid);
    if (u.action == 'A') {
        if (li < 0) {
            li = ~li;
            typename std::remove_cvref_t<decltype(lv[0])> fresh{};
            fresh.price = o.price;
            lv.insert(lv.begin() + li, fresh);
        }
        auto& L = lv[std::size_t(li)];
        L.orders.push_back(o);  // the order arrives as the record the book keeps
        L.qty += o.qty;
        ++L.count;
        return;
    }
    auto& L = lv[std::size_t(li)];
    auto& os = L.orders;
    std::size_t k = 0;
    while (os[k].order_id != o.order_id) ++k;
    L.qty -= o.qty;
    if (u.flags & kRemovesOrder) {
        os.erase(os.begin() + std::ptrdiff_t(k));
        if (--L.count == 0) lv.erase(lv.begin() + li);
    } else {
        os[k].qty -= o.qty;
    }
}

template <class B>
inline void best_prices(const B& b, Row& r) {
    r.best_bid = b.bids.size() ? b.bids[b.bids.size() - 1].price : 0;
    r.best_ask = b.asks.size() ? b.asks[b.asks.size() - 1].price : 0;
}

// The state service for one state object S (hft::State, StdState, cista state).
template <class S>
struct Applier {
    std::vector<std::int32_t> book_of;  // by instrument, -1 when the instrument has no book

    void index(const S& s) {
        book_of.assign(s.rows.size(), -1);
        for (std::size_t i = 0; i < s.books.size(); ++i) book_of[s.books[i].instrument] = std::int32_t(i);
    }

    void apply(S& s, const Update& u) const {
        Row& r = s.rows[u.instrument];
        update_row(r, u);
        const std::int32_t bi = book_of[u.instrument];
        if (bi >= 0 && u.action != 'T') {
            auto& b = s.books[std::size_t(bi)];
            apply_book(b, u);
            best_prices(b, r);
        }
        s.seq = u.seq;
    }
};

// ---- digest: the state every model must reach --------------------------------------
inline std::uint64_t mix(std::uint64_t h, std::uint64_t v) { return (h ^ v) * 0x100000001b3ull; }

inline std::uint64_t digest_row(std::uint64_t h, const Row& r) {
    const std::uint64_t f[] = {r.symbol, r.instrument, r.live_orders, r.live_shares, std::uint64_t(r.best_bid),
                               std::uint64_t(r.best_ask), std::uint64_t(r.last_price), r.volume, r.trades, r.adds,
                               r.removes, std::uint64_t(r.high), std::uint64_t(r.low), r.last_ts};
    for (auto v : f) h = mix(h, v);
    return h;
}
inline std::uint64_t digest_order(std::uint64_t h, const Order& o) {
    return mix(mix(mix(mix(h, o.order_id), o.ts), std::uint64_t(o.price)), o.qty);
}

template <class S>
std::uint64_t digest_state(const S& s) {
    std::uint64_t h = mix(1469598103934665603ull, s.seq);
    for (std::size_t bi = 0; bi < s.books.size(); ++bi) {
        const auto& b = s.books[bi];
        h = mix(mix(h, b.instrument), b.symbol);
        for (const auto* lv : {&b.bids, &b.asks}) {
            h = mix(h, lv->size());
            for (long i = long(lv->size()) - 1; i >= 0; --i) {
                const auto& L = (*lv)[std::size_t(i)];
                h = mix(mix(mix(h, std::uint64_t(L.price)), L.qty), L.count);
                for (std::size_t k = 0; k < L.orders.size(); ++k) h = digest_order(h, L.orders[k]);
            }
        }
    }
    for (std::size_t i = 0; i < s.rows.size(); ++i) h = digest_row(h, s.rows[i]);
    return h;
}

// Rows for every instrument of the feed; books for `instruments`.
template <class S>
void init_state(S& s, const std::vector<std::uint64_t>& symbols, const std::vector<std::uint32_t>& instruments) {
    s.rows.resize(symbols.size());
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        Row& r = s.rows[i];
        r.symbol = symbols[i];
        r.instrument = std::uint32_t(i);
    }
    for (auto ins : instruments) {
        auto& b = s.books.emplace_back();
        b.instrument = ins;
        b.symbol = symbols[ins];
    }
}

}  // namespace hft
