// HFT demo, case "state" (claim C): the state service keeps the order books of
// the busiest instruments and the metrics row of every instrument, and
// applies every update of the window in place.
//
//   xo     hft::State in one XBuffer (root pointer held)
//   std    hft::StdState (std::vector), the form the encoding formats decode into
//   cista  hft::cm::State (cista offset vectors)
//   pool   hft::PoolState: the hand-written fixed-capacity pool
//
// Phases: "seed" (the orders resting when the window opens) and "apply" (the
// window). Only "apply" is the result; both are measured.
#pragma once
#include <cstdio>
#include <string>

#include "hft/apply.hpp"
#include "hft/args.hpp"
#include "hft/itch.hpp"
#include "hft/measure.hpp"
#include "hft/model.hpp"
#include "hft/pool.hpp"
#if __has_include(<cista/serialization.h>)
#include <cista/serialization.h>
#include "hft/cista_model.hpp"
#define HFT_WITH_CISTA 1
#endif

namespace hft {

struct BookStats {
    std::size_t levels = 0, orders = 0, max_queue = 0;
};

template <class S>
BookStats book_stats(const S& s) {
    BookStats st;
    for (std::size_t bi = 0; bi < s.books.size(); ++bi)
        for (const auto* lv : {&s.books[bi].bids, &s.books[bi].asks})
            for (std::size_t i = 0; i < lv->size(); ++i) {
                const std::size_t n = (*lv)[i].orders.size();
                ++st.levels;
                st.orders += n;
                st.max_queue = std::max(st.max_queue, n);
            }
    return st;
}
inline BookStats book_stats(const PoolState& p) {
    BookStats st;
    for (const PoolBook& b : p.books)
        for (int s = 0; s < 2; ++s)
            for (const PoolLevel& L : b.lv[s]) {
                ++st.levels;
                st.orders += L.count;
                st.max_queue = std::max<std::size_t>(st.max_queue, L.count);
            }
    return st;
}

// Seed, then the window; `apply` is the model's whole per-update path.
template <class F>
void drive(const Feed& feed, Region& seed, Region& win, F&& apply) {
    const Update* u = feed.updates.data();
    seed.begin();
    for (std::size_t i = 0; i < feed.seed_end; ++i) apply(u[i]);
    seed.end();
    win.begin();
    for (std::size_t i = feed.seed_end; i < feed.updates.size(); ++i) apply(u[i]);
    win.end();
}

inline XBuffer::MaximumCapacity state_capacity() {
    return XBuffer::maximum_capacity(std::size_t(1) << 30).with_growth(XGrowPolicy::Automatic);
}

inline int run_state(const Args& a, const Feed& feed) {
    const std::string model = a.get("--model", "xo");
    const std::string ir = a.get("--ir", "apply");
    const auto books = busiest_instruments(feed, std::size_t(a.num("--books", 64)));
    Region seed{"seed", &ir}, win{"apply", &ir};
    std::uint64_t digest = 0;
    std::size_t snapshot_bytes = 0;
    BookStats st;
    if (model == "xo") {
        XBuffer buf = XBuffer::create<State>(std::size_t(16) << 20, state_capacity());
        State* s = &buf.root<State>();
        init_state(*s, feed.symbol, books);
        Applier<State> ap;
        ap.index(*s);
        drive(feed, seed, win, [&](const Update& u) { ap.apply(*s, u); });
        digest = digest_state(*s);
        st = book_stats(*s);
        snapshot_bytes = buf.save_compact<State>().size();  // what a late joiner gets (case join)
    } else if (model == "std") {
        StdState s;
        init_state(s, feed.symbol, books);
        Applier<StdState> ap;
        ap.index(s);
        drive(feed, seed, win, [&](const Update& u) { ap.apply(s, u); });
        digest = digest_state(s);
        st = book_stats(s);
    } else if (model == "pool") {
        PoolState p;
        p.init(feed.symbol, books, kPoolOrders, kPoolLevels);
        drive(feed, seed, win, [&](const Update& u) { p.apply(u); });
        digest = p.digest();
        st = book_stats(p);
        std::string snap;
        p.snapshot(snap);
        snapshot_bytes = snap.size();
#if HFT_WITH_CISTA
    } else if (model == "cista") {
        cm::State s{};
        init_state(s, feed.symbol, books);
        Applier<cm::State> ap;
        ap.index(s);
        drive(feed, seed, win, [&](const Update& u) { ap.apply(s, u); });
        digest = digest_state(s);
        st = book_stats(s);
        snapshot_bytes = cista::serialize<cista::mode::NONE>(s).size();
#endif
    } else {
        std::fprintf(stderr, "unknown model %s\n", model.c_str());
        return 2;
    }
    const std::size_t n = feed.updates.size() - feed.seed_end;
    std::printf("{\"case\":\"state\",\"model\":\"%s\",\"books\":%zu,\"seed\":%zu,\"updates\":%zu,"
                "\"seed_ns\":%.0f,\"apply_ns\":%.0f,\"ns_per_update\":%.2f,\"levels\":%zu,\"orders\":%zu,"
                "\"max_queue\":%zu,\"snapshot_bytes\":%zu,\"digest\":\"%016llx\"}\n",
                model.c_str(), books.size(), feed.seed_end, n, seed.ns, win.ns, win.ns / double(n), st.levels,
                st.orders, st.max_queue, snapshot_bytes, (unsigned long long)digest);
    return 0;
}

}  // namespace hft
