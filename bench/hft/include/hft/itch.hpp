// HFT demo: replay of a Nasdaq TotalView-ITCH 5.0 window (tools/itch_extract)
// through a feed handler that normalizes it into Update records.
//
// The exchange protocol is fixed (big-endian, per-message types), so decoding
// it costs every format the same: the whole window is normalized once at
// start-up and no case times it. This is the one place the chain converts.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "hft/model.hpp"

namespace hft {

inline std::uint64_t be(const std::uint8_t* p, int n) {
    std::uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}

// A replay window, normalized.
//   updates [0, seed_end): one add per order resting when the window opens.
//   updates [seed_end, end): the window, split into packets;
//   packet p = updates [packet_begin[p], packet_begin[p + 1]).
struct Feed {
    std::vector<Update> updates;
    std::size_t seed_end = 0;
    std::vector<std::uint32_t> packet_begin;
    std::vector<std::uint64_t> symbol;  // by instrument (locate), 8 ASCII bytes as little-endian u64
    std::uint64_t window_start_ns = 0, window_end_ns = 0;
    std::uint64_t itch_messages = 0, itch_bytes = 0;
    std::size_t packets() const { return packet_begin.empty() ? 0 : packet_begin.size() - 1; }
    std::size_t instruments() const { return symbol.size(); }
};

// Packets: consecutive messages are grouped the way a sequencer emits bursts:
// a packet takes the messages whose timestamps lie within 1 us of its first
// one, up to 1,400 bytes of ITCH payload (one MoldUDP64 datagram under a
// 1,500-byte MTU). Only messages that produce updates count; a packet that
// produces none is dropped. `seconds` > 0 cuts the window short.
inline Feed load_feed(const std::string& path, double seconds = 0) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<std::uint8_t> raw;
    std::fseek(f, 0, SEEK_END);
    raw.resize(std::size_t(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    const bool ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size();
    std::fclose(f);
    if (!ok) throw std::runtime_error("short read " + path);

    struct Resting { std::uint32_t instrument; std::uint8_t side; std::int64_t price; std::uint32_t qty; std::uint64_t ts; };
    std::unordered_map<std::uint64_t, Resting> live;
    live.reserve(1 << 21);
    Feed feed;
    std::uint32_t seq = 0;
    auto emit = [&](std::uint64_t ts, std::uint64_t id, std::int64_t px, std::uint32_t q, std::uint32_t ins,
                    std::uint8_t action, std::uint8_t side, bool removes = false) {
        Update u{};
        u.order = Order{id, ts, px, q, side, 0, 0};
        u.instrument = ins;
        u.seq = ++seq;
        u.action = action;
        u.flags = removes ? kRemovesOrder : 0;
        feed.updates.push_back(u);
    };

    // The extractor writes the seed (one add per resting order, all stamped
    // with the window start) right after the directory. Its order within a
    // price level is lost there; order references grow with time, so sorting
    // the seed by reference restores time priority.
    std::vector<std::pair<std::uint64_t, std::size_t>> seed_msgs;  // (reference, position)
    std::size_t pos = 0;
    std::uint64_t first_ts = 0;
    while (pos + 2 <= raw.size()) {
        const auto n = std::uint16_t(be(&raw[pos], 2));
        const std::uint8_t* m = &raw[pos + 2];
        if (n >= 11 && m[0] != 'R') {
            const std::uint64_t ts = be(m + 5, 6);
            if (!first_ts) first_ts = ts;
            if (m[0] != 'A' || ts != first_ts) break;
            seed_msgs.push_back({be(m + 11, 8), pos});
        } else if (n >= 19 && m[0] == 'R') {
            const auto ins = std::uint32_t(be(m + 1, 2));
            if (feed.symbol.size() <= ins) feed.symbol.resize(ins + 1, 0x2020202020202020ull);
            std::uint64_t s = 0;
            std::memcpy(&s, m + 11, 8);
            feed.symbol[ins] = s;
        }
        pos += 2 + n;
    }
    std::sort(seed_msgs.begin(), seed_msgs.end());
    for (const auto& [ref, at] : seed_msgs) {
        const std::uint8_t* m = &raw[at + 2];
        const auto ins = std::uint32_t(be(m + 1, 2));
        Resting r{ins, m[19], std::int64_t(be(m + 32, 4)), std::uint32_t(be(m + 20, 4)), be(m + 5, 6)};
        live[ref] = r;
        emit(r.ts, ref, r.price, r.qty, ins, 'A', r.side);
    }
    feed.seed_end = feed.updates.size();
    feed.window_start_ns = first_ts;
    const std::uint64_t limit = seconds > 0 ? first_ts + std::uint64_t(seconds * 1e9) : ~0ull;

    feed.packet_begin.push_back(std::uint32_t(feed.updates.size()));
    std::uint64_t pkt_first_ts = 0, pkt_bytes = 0;
    bool pkt_open = false;
    auto close_packet = [&] {
        if (pkt_open && feed.updates.size() > feed.packet_begin.back())
            feed.packet_begin.push_back(std::uint32_t(feed.updates.size()));
        pkt_open = false;
    };
    // pos is at the first message after the seed.
    while (pos + 2 <= raw.size()) {
        const auto n = std::uint16_t(be(&raw[pos], 2));
        const std::uint8_t* m = &raw[pos + 2];
        pos += 2 + n;
        if (n < 11) continue;
        const std::uint8_t type = m[0];
        const std::uint64_t ts = be(m + 5, 6);
        const auto ins = std::uint32_t(be(m + 1, 2));
        if (type == 'R') continue;
        if (ts >= limit) break;
        feed.window_end_ns = ts;
        ++feed.itch_messages;
        feed.itch_bytes += n;
        if (!pkt_open || ts > pkt_first_ts + 1000 || pkt_bytes + n > 1400) {
            close_packet();
            pkt_open = true;
            pkt_first_ts = ts;
            pkt_bytes = 0;
        }
        pkt_bytes += n;
        switch (type) {
        case 'A': case 'F': {
            const std::uint64_t id = be(m + 11, 8);
            Resting r{ins, m[19], std::int64_t(be(m + 32, 4)), std::uint32_t(be(m + 20, 4)), ts};
            live[id] = r;
            emit(ts, id, r.price, r.qty, ins, 'A', r.side);
            break;
        }
        case 'E': case 'C': case 'X': {
            const std::uint64_t id = be(m + 11, 8);
            auto it = live.find(id);
            if (it == live.end()) break;
            const std::uint32_t q = std::min(std::uint32_t(be(m + 19, 4)), it->second.qty);
            const Resting& r = it->second;
            emit(ts, id, r.price, q, r.instrument, type == 'X' ? 'C' : 'F', r.side, q == r.qty);
            it->second.qty -= q;
            if (!it->second.qty) live.erase(it);
            break;
        }
        case 'D': {
            const std::uint64_t id = be(m + 11, 8);
            auto it = live.find(id);
            if (it == live.end()) break;
            const Resting& r = it->second;
            emit(ts, id, r.price, r.qty, r.instrument, 'D', r.side, true);
            live.erase(it);
            break;
        }
        case 'U': {
            const std::uint64_t old_id = be(m + 11, 8), new_id = be(m + 19, 8);
            auto it = live.find(old_id);
            if (it == live.end()) break;
            Resting r = it->second;
            emit(ts, old_id, r.price, r.qty, r.instrument, 'D', r.side, true);
            live.erase(it);
            r.qty = std::uint32_t(be(m + 27, 4));
            r.price = std::int64_t(be(m + 31, 4));
            r.ts = ts;
            live[new_id] = r;
            emit(ts, new_id, r.price, r.qty, r.instrument, 'A', r.side);
            break;
        }
        case 'P':
            emit(ts, be(m + 11, 8), std::int64_t(be(m + 32, 4)), std::uint32_t(be(m + 20, 4)), ins, 'T', m[19]);
            break;
        default: break;
        }
    }
    close_packet();
    if (feed.symbol.empty()) throw std::runtime_error("no stock directory in " + path);
    return feed;
}

// The normalized window, cached so that runs (callgrind ones above all) do
// not decode the ITCH file again: a header, the symbols, the packet bounds,
// then the updates as they are in memory.
inline void save_feed(const Feed& feed, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot write " + path);
    const std::uint64_t head[8] = {0x3164656546544648ull /* "HFTFeed1" */, feed.updates.size(), feed.seed_end,
                                   feed.packet_begin.size(), feed.symbol.size(), feed.window_start_ns,
                                   feed.window_end_ns, feed.itch_messages};
    bool ok = std::fwrite(head, sizeof head, 1, f) == 1 && std::fwrite(&feed.itch_bytes, 8, 1, f) == 1;
    ok = ok && std::fwrite(feed.symbol.data(), 8, feed.symbol.size(), f) == feed.symbol.size();
    ok = ok && std::fwrite(feed.packet_begin.data(), 4, feed.packet_begin.size(), f) == feed.packet_begin.size();
    ok = ok && std::fwrite(feed.updates.data(), sizeof(Update), feed.updates.size(), f) == feed.updates.size();
    ok = std::fclose(f) == 0 && ok;
    if (!ok) throw std::runtime_error("short write " + path);
}

inline Feed load_cached_feed(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open " + path);
    std::uint64_t head[8];
    Feed feed;
    bool ok = std::fread(head, sizeof head, 1, f) == 1 && head[0] == 0x3164656546544648ull &&
              std::fread(&feed.itch_bytes, 8, 1, f) == 1;
    if (ok) {
        feed.updates.resize(head[1]);
        feed.seed_end = head[2];
        feed.packet_begin.resize(head[3]);
        feed.symbol.resize(head[4]);
        feed.window_start_ns = head[5];
        feed.window_end_ns = head[6];
        feed.itch_messages = head[7];
        ok = std::fread(feed.symbol.data(), 8, feed.symbol.size(), f) == feed.symbol.size() &&
             std::fread(feed.packet_begin.data(), 4, feed.packet_begin.size(), f) == feed.packet_begin.size() &&
             std::fread(feed.updates.data(), sizeof(Update), feed.updates.size(), f) == feed.updates.size();
    }
    std::fclose(f);
    if (!ok) throw std::runtime_error("bad feed cache " + path);
    return feed;
}

// The `k` instruments with the most book updates in the window (ties by
// locate), most active first: the order books the state service keeps.
inline std::vector<std::uint32_t> busiest_instruments(const Feed& feed, std::size_t k) {
    std::unordered_map<std::uint32_t, std::uint64_t> n;
    for (std::size_t i = feed.seed_end; i < feed.updates.size(); ++i)
        if (feed.updates[i].action != 'T') ++n[feed.updates[i].instrument];
    std::vector<std::pair<std::uint64_t, std::uint32_t>> v;
    for (auto [ins, c] : n) v.push_back({c, ins});
    std::sort(v.begin(), v.end(), [](auto a, auto b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < v.size() && i < k; ++i) out.push_back(v[i].second);
    return out;
}

}  // namespace hft
