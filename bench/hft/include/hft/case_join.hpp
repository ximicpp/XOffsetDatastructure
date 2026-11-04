// HFT demo, case "join" (claim B): a client joins in the middle of the
// window. The server applies the updates before the join point (not
// measured), then
//   snapshot  the server turns its state into bytes (the server's stall)
//   ready     the client turns the bytes into a state it can update, with its
//             instrument -> book index (and, for the pool, its order index)
//   rest      the client applies every update after the join point
// The client's final digest must equal the state case's digest. The server
// state stays alive while the client gets ready, as in two processes: the
// client cannot reuse the memory the server frees.
//
// Formats (server state -> bytes -> client state):
//   xo             XBuffer -> save() -> load_unverified + unsafe_root (trusted link)
//   xo_verified    XBuffer -> save_compact -> load_verified
//   cista          cista state -> serialize -> copy + deserialize mode::CAST (trusted)
//   cista_checked  same, deserialize mode::DEEP_CHECK
//   pool           pool -> array copy (pool.hpp) -> array copy + rebuild the order index
//   zpp            std state -> zpp_bits -> std state
//   fb             std state -> FlatBuffers -> std state (no Verifier: trusted)
//   fb_verified    same, Verifier first
//   sbe            std state -> SBE Snapshot (no bounds checks: trusted) -> std state
// The encoding formats keep the std model on both ends, the form they decode
// into; the in-place formats keep their own state on both ends.
#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hft/apply.hpp"
#include "hft/args.hpp"
#include "hft/case_hot.hpp"
#include "hft/case_state.hpp"
#include "hft/itch.hpp"
#include "hft/measure.hpp"
#include "hft/model.hpp"
#include "hft/pool.hpp"
#if __has_include(<zpp_bits.h>)
#include <zpp_bits.h>
#define HFT_WITH_ZPP 1
#endif
#if __has_include("hft_generated.h")
#include "hft_generated.h"
#define HFT_WITH_FB 1
#endif
#if HFT_WITH_SBE
#include "hft_sbe/Snapshot.h"
#endif

namespace hft {

struct Join {
    std::string ir;
    Region snapshot{"snapshot", &ir}, ready{"ready", &ir}, rest{"rest", &ir};
    std::size_t at = 0;  // first update the client applies
    std::size_t bytes = 0;
    std::uint64_t digest = 0;
};

template <class F>
void apply_range(const Feed& feed, std::size_t from, std::size_t to, F&& apply) {
    const Update* u = feed.updates.data();
    for (std::size_t i = from; i < to; ++i) apply(u[i]);
}

// The in-place XOffset state: one XBuffer on each end.
template <bool Verified>
void join_xo(const Feed& feed, const std::vector<std::uint32_t>& books, Join& j) {
    std::string wire;
    XBuffer server = XBuffer::create<State>(std::size_t(16) << 20, state_capacity());
    {
        State* s = &server.root<State>();
        init_state(*s, feed.symbol, books);
        Applier<State> ap;
        ap.index(*s);
        apply_range(feed, 0, j.at, [&](const Update& u) { ap.apply(*s, u); });
        j.snapshot.begin();
        if constexpr (Verified) server.save_compact_into<State>(wire);
        else wire = server.save();
        j.snapshot.end();
    }
    j.bytes = wire.size();
    j.ready.begin();
    XBuffer client = Verified ? XBuffer::load_verified<State>(bytes_of(wire), state_capacity())
                              : XBuffer::load_unverified(bytes_of(wire), state_capacity());
    State* c = Verified ? &client.root<State>() : &client.unsafe_root<State>();
    Applier<State> ap;
    ap.index(*c);
    j.ready.end();
    j.rest.time([&] { apply_range(feed, j.at, feed.updates.size(), [&](const Update& u) { ap.apply(*c, u); }); });
    j.digest = digest_state(*c);
}

#if HFT_WITH_CISTA
template <cista::mode Mode>
void join_cista(const Feed& feed, const std::vector<std::uint32_t>& books, Join& j) {
    std::vector<std::uint8_t> wire;
    cm::State s{};
    {
        init_state(s, feed.symbol, books);
        Applier<cm::State> ap;
        ap.index(s);
        apply_range(feed, 0, j.at, [&](const Update& u) { ap.apply(s, u); });
        j.snapshot.time([&] { wire = cista::serialize<Mode>(s); });
    }
    j.bytes = wire.size();
    // The client copies the bytes into a buffer it owns and deserializes there.
    std::vector<std::uint8_t> held;
    cm::State* c = nullptr;
    Applier<cm::State> ap;
    j.ready.time([&] {
        held.assign(wire.begin(), wire.end());
        c = cista::deserialize<cm::State, Mode>(held);
        ap.index(*c);
    });
    j.rest.time([&] { apply_range(feed, j.at, feed.updates.size(), [&](const Update& u) { ap.apply(*c, u); }); });
    j.digest = digest_state(*c);
}
#endif

inline void join_pool(const Feed& feed, const std::vector<std::uint32_t>& books, Join& j) {
    std::string wire;
    PoolState p;
    {
        p.init(feed.symbol, books, kPoolOrders, kPoolLevels);
        apply_range(feed, 0, j.at, [&](const Update& u) { p.apply(u); });
        j.snapshot.time([&] { p.snapshot(wire); });
    }
    j.bytes = wire.size();
    PoolState c;
    j.ready.time([&] { c.restore(wire, kPoolOrders, kPoolLevels); });
    j.rest.time([&] { apply_range(feed, j.at, feed.updates.size(), [&](const Update& u) { c.apply(u); }); });
    j.digest = c.digest();
}

// The encoding formats: std state on both ends, `encode(state, bytes)` and
// `decode(bytes, state)`.
template <class Encode, class Decode>
void join_std(const Feed& feed, const std::vector<std::uint32_t>& books, Join& j, Encode&& encode, Decode&& decode) {
    std::vector<std::byte> wire;
    StdState s;
    {
        init_state(s, feed.symbol, books);
        Applier<StdState> ap;
        ap.index(s);
        apply_range(feed, 0, j.at, [&](const Update& u) { ap.apply(s, u); });
        j.snapshot.time([&] { encode(s, wire); });
    }
    j.bytes = wire.size();
    StdState c;
    Applier<StdState> ap;
    j.ready.time([&] {
        decode(std::span<const std::byte>(wire), c);
        ap.index(c);
    });
    j.rest.time([&] { apply_range(feed, j.at, feed.updates.size(), [&](const Update& u) { ap.apply(c, u); }); });
    j.digest = digest_state(c);
}

#if HFT_WITH_FB
// The schema declares Order and Row field for field (hft.fbs).
static_assert(sizeof(fb::Order) == sizeof(Order) && alignof(fb::Order) == alignof(Order));
static_assert(sizeof(fb::Row) == sizeof(Row) && alignof(fb::Row) == alignof(Row));

// Order and Row go in and out as whole struct arrays (same layout), the
// fastest way FlatBuffers offers.
inline void fb_encode(const StdState& s, std::vector<std::byte>& out, flatbuffers::FlatBufferBuilder& fbb) {
    fbb.Clear();
    std::vector<flatbuffers::Offset<fb::Book>> bks;
    std::vector<flatbuffers::Offset<fb::Level>> lv[2];
    for (const StdBook& b : s.books) {
        flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fb::Level>>> side[2];
        int k = 0;
        for (const auto* levels : {&b.bids, &b.asks}) {
            lv[k].clear();
            for (const StdLevel& L : *levels) {
                const auto os = fbb.CreateVectorOfStructs(reinterpret_cast<const fb::Order*>(L.orders.data()), L.orders.size());
                lv[k].push_back(fb::CreateLevel(fbb, L.price, L.qty, L.count, os));
            }
            side[k] = fbb.CreateVector(lv[k]);
            ++k;
        }
        bks.push_back(fb::CreateBook(fbb, b.instrument, b.symbol, side[0], side[1]));
    }
    const auto bv = fbb.CreateVector(bks);
    const auto rv = fbb.CreateVectorOfStructs(reinterpret_cast<const fb::Row*>(s.rows.data()), s.rows.size());
    fbb.Finish(fb::CreateState(fbb, s.seq, bv, rv));
    const auto* p = reinterpret_cast<const std::byte*>(fbb.GetBufferPointer());
    out.assign(p, p + fbb.GetSize());
}

template <bool Verified>
inline void fb_decode(std::span<const std::byte> in, StdState& c) {
    if constexpr (Verified) {
        flatbuffers::Verifier v(reinterpret_cast<const std::uint8_t*>(in.data()), in.size());
        if (!fb::VerifyStateBuffer(v)) std::abort();
    }
    const fb::State* st = fb::GetState(in.data());
    c.seq = st->seq();
    const auto* rows = st->rows();
    c.rows.assign(reinterpret_cast<const Row*>(rows->data()), reinterpret_cast<const Row*>(rows->data()) + rows->size());
    const auto* bks = st->books();
    c.books.resize(bks->size());
    for (flatbuffers::uoffset_t i = 0; i < bks->size(); ++i) {
        const fb::Book* b = bks->Get(i);
        StdBook& cb = c.books[i];
        cb.instrument = b->instrument();
        cb.symbol = b->symbol();
        std::vector<StdLevel>* out[2] = {&cb.bids, &cb.asks};
        const flatbuffers::Vector<flatbuffers::Offset<fb::Level>>* in_lv[2] = {b->bids(), b->asks()};
        for (int k = 0; k < 2; ++k) {
            out[k]->resize(in_lv[k]->size());
            for (flatbuffers::uoffset_t l = 0; l < in_lv[k]->size(); ++l) {
                const fb::Level* L = in_lv[k]->Get(l);
                StdLevel& cl = (*out[k])[l];
                cl.price = L->price();
                cl.qty = L->qty();
                cl.count = L->count();
                const auto* os = L->orders();
                const auto* o = reinterpret_cast<const Order*>(os->data());
                cl.orders.assign(o, o + os->size());
            }
        }
    }
}
#endif

#if HFT_WITH_SBE
inline void sbe_encode(const StdState& s, std::vector<std::byte>& out) {
    using Snap = hft_sbe::Snapshot;
    std::size_t levels = 0, orders = 0;
    for (const StdBook& b : s.books)
        for (const auto* lv : {&b.bids, &b.asks}) {
            levels += lv->size();
            for (const StdLevel& L : *lv) orders += L.orders.size();
        }
    const std::size_t gh = hft_sbe::GroupSizeEncoding::encodedLength();
    const std::size_t cap = hft_sbe::MessageHeader::encodedLength() + Snap::sbeBlockLength() + 2 * gh +
                            s.rows.size() * Snap::Rows::sbeBlockLength() +
                            s.books.size() * (Snap::Books::sbeBlockLength() + 2 * gh) +
                            levels * (Snap::Books::Bids::sbeBlockLength() + gh) +
                            orders * Snap::Books::Bids::Orders::sbeBlockLength();
    auto fits = [](std::size_t n) {
        if (n > 0xFFFE) std::abort();  // SBE's uint16 group count
        return std::uint16_t(n);
    };
    out.resize(cap);
    Snap snap;
    snap.wrapAndApplyHeader(reinterpret_cast<char*>(out.data()), 0, cap).seq(s.seq);
    auto& rows = snap.rowsCount(fits(s.rows.size()));
    for (const Row& r : s.rows)
        rows.next()
            .symbol(r.symbol)
            .instrument(r.instrument)
            .liveOrders(r.live_orders)
            .liveShares(r.live_shares)
            .bestBid(r.best_bid)
            .bestAsk(r.best_ask)
            .lastPrice(r.last_price)
            .volume(r.volume)
            .trades(r.trades)
            .adds(r.adds)
            .removes(r.removes)
            .high(r.high)
            .low(r.low)
            .lastTs(r.last_ts);
    auto& bks = snap.booksCount(fits(s.books.size()));
    for (const StdBook& b : s.books) {
        bks.next().instrument(b.instrument).symbol(b.symbol);
        auto put = [&](auto& g, const std::vector<StdLevel>& lv) {
            for (const StdLevel& L : lv) {
                g.next().price(L.price).qty(L.qty).orderCount(L.count);
                auto& os = g.ordersCount(fits(L.orders.size()));
                for (const Order& o : L.orders) os.next().orderId(o.order_id).ts(o.ts).price(o.price).qty(o.qty).side(o.side);
            }
        };
        put(bks.bidsCount(fits(b.bids.size())), b.bids);
        put(bks.asksCount(fits(b.asks.size())), b.asks);
    }
    out.resize(hft_sbe::MessageHeader::encodedLength() + snap.encodedLength());
}

inline void sbe_decode(std::span<const std::byte> in, StdState& c) {
    char* b = const_cast<char*>(reinterpret_cast<const char*>(in.data()));
    hft_sbe::MessageHeader hdr(b, in.size());
    hft_sbe::Snapshot snap;
    snap.wrapForDecode(b, hft_sbe::MessageHeader::encodedLength(), hdr.blockLength(), hdr.version(), in.size());
    c.seq = snap.seq();
    auto& rows = snap.rows();
    c.rows.resize(rows.count());
    for (Row& r : c.rows) {
        rows.next();
        r = Row{rows.symbol(), rows.instrument(), rows.liveOrders(), rows.liveShares(), rows.bestBid(), rows.bestAsk(),
                rows.lastPrice(), rows.volume(), rows.trades(), rows.adds(), rows.removes(), rows.high(), rows.low(),
                rows.lastTs()};
    }
    auto& bks = snap.books();
    c.books.resize(bks.count());
    for (StdBook& cb : c.books) {
        bks.next();
        cb.instrument = bks.instrument();
        cb.symbol = bks.symbol();
        auto get = [&](auto& g, std::vector<StdLevel>& lv) {
            lv.resize(g.count());
            for (StdLevel& L : lv) {
                g.next();
                L.price = g.price();
                L.qty = g.qty();
                L.count = g.orderCount();
                auto& os = g.orders();
                L.orders.resize(os.count());
                for (Order& o : L.orders) {
                    os.next();
                    o = Order{os.orderId(), os.ts(), os.price(), os.qty(), os.side(), 0, 0};
                }
            }
        };
        get(bks.bids(), cb.bids);
        get(bks.asks(), cb.asks);
    }
}
#endif

inline int run_join(const Args& a, const Feed& feed) {
    const std::string f = a.get("--format", "xo");
    const auto books = busiest_instruments(feed, std::size_t(a.num("--books", 64)));
    Join j;
    j.ir = a.get("--ir", "ready");
    j.at = feed.seed_end + std::size_t(double(feed.updates.size() - feed.seed_end) * a.num("--at", 0.5));
    if (f == "xo") join_xo<false>(feed, books, j);
    else if (f == "xo_verified") join_xo<true>(feed, books, j);
    else if (f == "pool") join_pool(feed, books, j);
#if HFT_WITH_CISTA
    else if (f == "cista") join_cista<cista::mode::CAST>(feed, books, j);
    else if (f == "cista_checked") join_cista<cista::mode::DEEP_CHECK>(feed, books, j);
#endif
#if HFT_WITH_ZPP
    else if (f == "zpp")
        join_std(
            feed, books, j,
            [](const StdState& s, std::vector<std::byte>& out) {
                out.clear();
                zpp::bits::out o(out);
                if (zpp::bits::failure(o(s))) std::abort();
            },
            [](std::span<const std::byte> in, StdState& c) {
                zpp::bits::in i(in);
                if (zpp::bits::failure(i(c))) std::abort();
            });
#endif
#if HFT_WITH_FB
    else if (f == "fb" || f == "fb_verified") {
        flatbuffers::FlatBufferBuilder fbb(std::size_t(1) << 24);
        auto enc = [&](const StdState& s, std::vector<std::byte>& out) { fb_encode(s, out, fbb); };
        if (f == "fb") join_std(feed, books, j, enc, fb_decode<false>);
        else join_std(feed, books, j, enc, fb_decode<true>);
    }
#endif
#if HFT_WITH_SBE
    else if (f == "sbe") join_std(feed, books, j, sbe_encode, sbe_decode);
#endif
    else {
        std::fprintf(stderr, "unknown format %s\n", f.c_str());
        return 2;
    }
    std::printf("{\"case\":\"join\",\"format\":\"%s\",\"at\":%zu,\"rest_updates\":%zu,\"bytes\":%zu,\"snapshot_ns\":%.0f,"
                "\"ready_ns\":%.0f,\"rest_ns_per_update\":%.2f,\"digest\":\"%016llx\"}\n",
                f.c_str(), j.at, feed.updates.size() - j.at, j.bytes, j.snapshot.ns, j.ready.ns,
                j.rest.ns / double(feed.updates.size() - j.at), (unsigned long long)j.digest);
    return 0;
}

}  // namespace hft
