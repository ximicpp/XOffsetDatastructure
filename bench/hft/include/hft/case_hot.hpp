// HFT demo, case "hot" (claim A): the feed handler publishes every packet of
// the window into a ring of slots; 4 strategy readers read every field of
// every update of every slot. Only the publish ("encode") and the reads
// ("read") are measured.
//
//   raw    8-byte header {count, 0} + the Update records; the reader checks
//          the count against the bytes and casts
//   fixed  xoffset_fixed_message.hpp: place_fixed_batch / view_fixed_batch
//          (the same bytes as raw, plus the type tag the reader checks)
//   sbe    SBE (real-logic 1.41 C++ codegen, SBE_NO_BOUNDS_CHECK: trusted
//          link): a Packet message with a repeating group of updates
//   xo     an XBuffer per packet, the general XOffset path: reset<Batch>,
//          assign_range, save_verified_into; each reader reload_verified
#pragma once
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "hft/args.hpp"
#include "hft/itch.hpp"
#include "hft/measure.hpp"
#include "hft/model.hpp"
#if __has_include("hft_sbe/Packet.h")
#define SBE_NO_BOUNDS_CHECK 1
#include "hft_sbe/MessageHeader.h"
#include "hft_sbe/Packet.h"
#define HFT_WITH_SBE 1
#endif

namespace hft {

// The general XOffset path's message: one packet.
struct Batch {
    std::uint64_t seq_first;
    XVector<Update> updates;
};

}  // namespace hft

XOFFSET_RECORD(hft::Batch, seq_first, updates)
XOFFSET_REGISTER_SCHEMA_NAME(hft::Batch, "hft.Batch.v1")

namespace hft {

// Every format's encode and read is its own function, called once per
// packet (as a handler would be), so the compiler's inlining choices do not
// differ between formats and each one is optimized on its own.
#define HFT_INLINE [[gnu::noinline]]

// What a reader does with an update: read every field.
inline std::uint64_t fold_fields(std::uint64_t h, std::uint64_t id, std::uint64_t ts, std::int64_t px, std::uint32_t q,
                                 std::uint8_t side, std::uint32_t ins, std::uint32_t seq, std::uint8_t act,
                                 std::uint8_t flags) {
    return h + id + ts * 3 + std::uint64_t(px) * 5 + std::uint64_t(q) * 7 + std::uint64_t(side) * 11 +
           std::uint64_t(ins) * 13 + std::uint64_t(seq) * 17 + std::uint64_t(act) * 19 + std::uint64_t(flags) * 23;
}
inline std::uint64_t fold(std::uint64_t h, const Update& u) {
    return fold_fields(h, u.order.order_id, u.order.ts, u.order.price, u.order.qty, u.order.side, u.instrument, u.seq,
                       u.action, u.flags);
}

inline std::span<const std::byte> bytes_of(const std::string& s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

// One slot of the ring: a fixed-size, cache-line aligned buffer, as a
// shared-memory ring has.
struct Slot {
    static constexpr std::size_t kBytes = 4096 - 64;
    alignas(64) std::byte data[kBytes];
    std::uint32_t size = 0;
    std::span<std::byte> out() { return {data, kBytes}; }
    std::span<const std::byte> in() const { return {data, size}; }
};

struct RawHeader {
    std::uint32_t count;
    std::uint32_t pad;
};

struct RawFormat {
    static constexpr const char* name = "raw";
    HFT_INLINE void encode(Slot& slot, std::span<const Update> us) {
        const RawHeader h{std::uint32_t(us.size()), 0};
        std::memcpy(slot.data, &h, sizeof h);
        std::memcpy(slot.data + sizeof h, us.data(), us.size_bytes());
        slot.size = std::uint32_t(sizeof h + us.size_bytes());
    }
    HFT_INLINE std::uint64_t read(int, const Slot& slot) {
        const std::span<const std::byte> in = slot.in();
        RawHeader h;
        std::memcpy(&h, in.data(), sizeof h);
        // The one check a datagram reader cannot skip: the count fits the bytes.
        if (in.size() != sizeof h + std::size_t(h.count) * sizeof(Update)) std::abort();
        const auto* u = reinterpret_cast<const Update*>(in.data() + sizeof h);
        std::uint64_t f = 0;
        for (std::uint32_t i = 0; i < h.count; ++i) f = fold(f, u[i]);
        return f;
    }
};

struct FixedFormat {
    static constexpr const char* name = "fixed";
    HFT_INLINE void encode(Slot& slot, std::span<const Update> us) {
        const auto out = place_fixed_batch<Update>(slot.out(), std::uint32_t(us.size()));
        std::memcpy(out.records.data(), us.data(), us.size_bytes());
        slot.size = std::uint32_t(out.bytes);
    }
    HFT_INLINE std::uint64_t read(int, const Slot& slot) {
        const auto v = view_fixed_batch<Update>(slot.in());
        if (!v) std::abort();
        std::uint64_t f = 0;
        for (const Update& u : v.records) f = fold(f, u);
        return f;
    }
};

#if HFT_WITH_SBE
struct SbeFormat {
    static constexpr const char* name = "sbe";
    hft_sbe::Packet pkt;
    HFT_INLINE void encode(Slot& slot, std::span<const Update> us) {
        pkt.wrapAndApplyHeader(reinterpret_cast<char*>(slot.data), 0, Slot::kBytes).seqFirst(us[0].seq);
        auto& g = pkt.updatesCount(std::uint16_t(us.size()));
        for (const Update& u : us)
            g.next()
                .orderId(u.order.order_id)
                .ts(u.order.ts)
                .price(u.order.price)
                .qty(u.order.qty)
                .side(u.order.side)
                .action(u.action)
                .flags(u.flags)
                .instrument(u.instrument)
                .seq(u.seq);
        slot.size = std::uint32_t(hft_sbe::MessageHeader::encodedLength() + pkt.encodedLength());
    }
    HFT_INLINE std::uint64_t read(int, const Slot& slot) {
        char* b = const_cast<char*>(reinterpret_cast<const char*>(slot.data));
        hft_sbe::MessageHeader hdr(b, slot.size);
        hft_sbe::Packet p;
        p.wrapForDecode(b, hft_sbe::MessageHeader::encodedLength(), hdr.blockLength(), hdr.version(), slot.size);
        std::uint64_t f = 0;
        auto& g = p.updates();
        while (g.hasNext()) {
            g.next();
            f = fold_fields(f, g.orderId(), g.ts(), g.price(), g.qty(), g.side(), g.instrument(), g.seq(), g.action(),
                            g.flags());
        }
        return f;
    }
};
#endif

struct XoFormat {
    static constexpr const char* name = "xo";
    static constexpr int kReaders = 4;
    XBuffer pub = XBuffer::create<Batch>(16384);
    XBuffer rdr[kReaders] = {XBuffer::create<Batch>(16384), XBuffer::create<Batch>(16384),
                             XBuffer::create<Batch>(16384), XBuffer::create<Batch>(16384)};
    std::string wire;
    // save_verified_into writes a std::string; the copy into the slot is
    // part of this path's cost.
    HFT_INLINE void encode(Slot& slot, std::span<const Update> us) {
        auto& b = pub.reset<Batch>();
        b.seq_first = us[0].seq;
        b.updates.assign_range(us);
        pub.save_verified_into<Batch>(wire);
        std::memcpy(slot.data, wire.data(), wire.size());
        slot.size = std::uint32_t(wire.size());
    }
    HFT_INLINE std::uint64_t read(int r, const Slot& slot) {
        rdr[r].reload_verified<Batch>(slot.in());
        const Batch& b = rdr[r].root<Batch>();
        std::uint64_t f = 0;
        for (const Update& u : b.updates) f = fold(f, u);
        return f;
    }
};

template <class Fmt>
int hot_run(const Args& a, const Feed& feed) {
    constexpr int kRing = 512, kReaders = 4;
    const std::string ir = a.get("--ir", "read");
    Region enc{"encode", &ir}, rd{"read", &ir};
    Fmt fmt;
    std::vector<Slot> ring(kRing);
    const std::size_t packets = feed.packets();
    const Update* up = feed.updates.data();
    std::uint64_t digest = 0, bytes = 0, max_updates = 0;
    for (std::size_t base = 0; base < packets; base += kRing) {
        const int n = int(std::min<std::size_t>(kRing, packets - base));
        enc.begin();
        for (int i = 0; i < n; ++i) {
            const std::size_t p = base + std::size_t(i);
            fmt.encode(ring[std::size_t(i)], {up + feed.packet_begin[p], up + feed.packet_begin[p + 1]});
        }
        enc.end();
        rd.begin();
        for (int r = 0; r < kReaders; ++r)
            for (int i = 0; i < n; ++i) digest += fmt.read(r, ring[std::size_t(i)]);
        rd.end();
        for (int i = 0; i < n; ++i) {
            bytes += ring[std::size_t(i)].size;
            const std::size_t p = base + std::size_t(i);
            max_updates = std::max<std::uint64_t>(max_updates, feed.packet_begin[p + 1] - feed.packet_begin[p]);
        }
    }
    const std::size_t updates = feed.updates.size() - feed.packet_begin[0];
    std::printf("{\"case\":\"hot\",\"format\":\"%s\",\"packets\":%zu,\"updates\":%zu,\"max_updates_per_packet\":%llu,"
                "\"bytes_per_packet\":%.2f,\"encode_ns_per_packet\":%.2f,\"read_ns_per_packet_per_reader\":%.2f,"
                "\"digest\":\"%016llx\"}\n",
                Fmt::name, packets, updates, (unsigned long long)max_updates, double(bytes) / double(packets),
                enc.ns / double(packets), rd.ns / double(packets) / kReaders, (unsigned long long)digest);
    return 0;
}

inline int run_hot(const Args& a, const Feed& feed) {
    const std::string f = a.get("--format", "fixed");
    if (f == "raw") return hot_run<RawFormat>(a, feed);
    if (f == "fixed") return hot_run<FixedFormat>(a, feed);
#if HFT_WITH_SBE
    if (f == "sbe") return hot_run<SbeFormat>(a, feed);
#endif
    if (f == "xo") return hot_run<XoFormat>(a, feed);
    std::fprintf(stderr, "unknown format %s\n", f.c_str());
    return 2;
}

}  // namespace hft
