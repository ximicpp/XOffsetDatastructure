// Fixed-message subset micro-benchmark: a feed handler publishes packets of
// k normalized order-book records (40 bytes each) into a ring of slots; 4
// readers read every field of every record of every slot.
//
//   fixedmsg_bench --path raw|xo|xo_root|fixed|copy_floor
//                  --k K --packets P --rounds R [--ir encode|read]
//
// Paths (one per process):
//   raw            header {seq_first, count} + records; reader casts in place
//   xo             XOffset today: reset<Batch>, assign_range, save_verified_into;
//                  each reader reload_verified<Batch> into its own buffer
//   xo_root        k = 1 only: the record itself is the XOffset root
//   fixed          xoffset_fixed_message.hpp: write_fixed_batch / view_fixed_batch
//   copy_floor     raw wire, but each reader first copies the slot into its
//                  own buffer: the floor of any copy-in receive
// k = 0 sends single records without a batch header (raw / fixed / xo_root).
// Times only encode (write into the slot) and read (receive + fold).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include "xoffset_fixed_message.hpp"
#include "xoffsetdatastructure.hpp"
#if defined(FIXEDMSG_IR)
#include <valgrind/callgrind.h>
#endif

using namespace XOffsetDatastructure;

struct alignas(8) NormUpdate {
    uint64_t ts;
    uint64_t order_id;
    int64_t price;
    uint32_t qty;
    uint32_t instrument;
    uint32_t seq;
    uint8_t action;
    uint8_t side;
    uint8_t flags;
    uint8_t pad;
};
static_assert(sizeof(NormUpdate) == 40);
static_assert(is_fixed_message_v<NormUpdate>);

struct alignas(8) Batch {
    uint64_t seq_first;
    XVector<NormUpdate> updates;
};

XOFFSET_REGISTER_SCHEMA_NAME(NormUpdate, "bench.NormUpdate.v1")
XOFFSET_REGISTER_SCHEMA_NAME(Batch, "bench.NormBatch.v1")

struct RawHeader { uint64_t seq_first; uint32_t count; uint32_t pad; };

static inline uint64_t fold(uint64_t h, const NormUpdate& u) {
    return h + u.ts + u.order_id * 3 + uint64_t(u.price) * 5 + uint64_t(u.qty) * 7 + uint64_t(u.instrument) * 11 +
           uint64_t(u.seq) * 13 + uint64_t(u.action) * 17 + uint64_t(u.side) * 19 + uint64_t(u.flags) * 23;
}

static inline void write_slot(std::string& s, std::size_t n, auto&& fill) {
    s.resize_and_overwrite(n, [&](char* p, std::size_t) { fill(reinterpret_cast<std::byte*>(p)); return n; });
}

int main(int argc, char** argv) {
    std::string path = "raw", ir;
    int k = 4, packets = 1 << 16, rounds = 20;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string a = argv[i];
        if (a == "--path") path = argv[i + 1];
        else if (a == "--k") k = std::atoi(argv[i + 1]);
        else if (a == "--packets") packets = std::atoi(argv[i + 1]);
        else if (a == "--rounds") rounds = std::atoi(argv[i + 1]);
        else if (a == "--ir") ir = argv[i + 1];
    }
    const bool single = k == 0;
    const int n = single ? 1 : k;
    constexpr int kRing = 512, kReaders = 4;
    packets = (packets / kRing) * kRing;
    enum P { RAW, COPY, FIXED, XO, XO_ROOT };
    const P pk_ = path == "raw" ? RAW : path == "copy_floor" ? COPY : path == "fixed" ? FIXED
                : path == "xo" ? XO : XO_ROOT;

    // Synthetic records, the shape of normalized ITCH events.
    std::mt19937_64 rng(42);
    std::vector<NormUpdate> recs(std::size_t(packets) * n);
    uint32_t seq = 0;
    for (auto& u : recs) {
        u = NormUpdate{34200000000000ull + rng() % 1000000000ull, rng() % 400000000ull, int64_t(100000 + rng() % 5000000),
                       uint32_t(1 + rng() % 1000), uint32_t(rng() % 8000), ++seq, uint8_t("AFCDT"[rng() % 5]),
                       uint8_t(rng() % 2 ? 'B' : 'S'), 0, 0};
    }

    std::vector<std::string> ring(kRing);
    for (auto& s : ring) s.reserve(16384);
    std::optional<XBuffer> pub, rdr[kReaders];
    if (pk_ == XO) {
        pub.emplace(XBuffer::create<Batch>(16384));
        for (auto& r : rdr) r.emplace(XBuffer::create<Batch>(16384));
    } else if (pk_ == XO_ROOT) {
        pub.emplace(XBuffer::create<NormUpdate>(4096));
        for (auto& r : rdr) r.emplace(XBuffer::create<NormUpdate>(4096));
    }
    std::vector<std::string> copy_buf(kReaders);
    for (auto& c : copy_buf) c.reserve(16384);

    auto encode = [&](int p, std::string& slot) {
        const NormUpdate* src = &recs[std::size_t(p) * n];
        if (pk_ == RAW || pk_ == COPY) {
            if (single) write_slot(slot, sizeof(NormUpdate), [&](std::byte* d) { std::memcpy(d, src, sizeof(NormUpdate)); });
            else write_slot(slot, sizeof(RawHeader) + n * sizeof(NormUpdate), [&](std::byte* d) {
                RawHeader h{src->seq, uint32_t(n), 0};
                std::memcpy(d, &h, sizeof h);
                std::memcpy(d + sizeof h, src, n * sizeof(NormUpdate));
            });
        } else if (pk_ == FIXED) {
            if (single) write_slot(slot, sizeof(NormUpdate), [&](std::byte* d) {
                write_fixed_message<NormUpdate>({d, sizeof(NormUpdate)}, *src);
            });
            else {
                const std::size_t bytes = fixed_batch_bytes<NormUpdate>(uint32_t(n));
                write_slot(slot, bytes, [&](std::byte* d) {
                    write_fixed_batch<NormUpdate>({d, bytes}, {src, std::size_t(n)});
                });
            }
        } else if (pk_ == XO) {
            auto& b = pub->reset<Batch>();
            b.seq_first = src->seq;
            b.updates.assign_range(std::span<const NormUpdate>(src, std::size_t(n)));
            pub->save_verified_into<Batch>(slot);
        } else if (pk_ == XO_ROOT) {
            pub->reset<NormUpdate>() = *src;
            pub->save_verified_into<NormUpdate>(slot);
        }
    };
    auto read = [&](int r, const std::string& slot) -> uint64_t {
        const std::span<const std::byte> in(reinterpret_cast<const std::byte*>(slot.data()), slot.size());
        uint64_t h = 0;
        if (pk_ == RAW) {
            if (single) return fold(h, *reinterpret_cast<const NormUpdate*>(in.data()));
            RawHeader hd;
            std::memcpy(&hd, in.data(), sizeof hd);
            if (in.size() != sizeof hd + hd.count * sizeof(NormUpdate)) std::abort();
            const auto* u = reinterpret_cast<const NormUpdate*>(in.data() + sizeof hd);
            h = hd.seq_first;
            for (uint32_t i = 0; i < hd.count; ++i) h = fold(h, u[i]);
        } else if (pk_ == COPY) {
            std::string& c = copy_buf[std::size_t(r)];
            c.resize_and_overwrite(in.size(), [&](char* p, std::size_t) { std::memcpy(p, in.data(), in.size()); return in.size(); });
            if (single) return fold(h, *reinterpret_cast<const NormUpdate*>(c.data()));
            RawHeader hd;
            std::memcpy(&hd, c.data(), sizeof hd);
            const auto* u = reinterpret_cast<const NormUpdate*>(c.data() + sizeof hd);
            h = hd.seq_first;
            for (uint32_t i = 0; i < hd.count; ++i) h = fold(h, u[i]);
        } else if (pk_ == FIXED) {
            if (single) {
                const auto u = view_fixed_message<NormUpdate>(in);
                if (!u) std::abort();
                return fold(h, *u);
            }
            const auto v = view_fixed_batch<NormUpdate>(in);
            if (!v) std::abort();
            h = v.records.empty() ? 0 : v.records[0].seq;
            for (const auto& u : v.records) h = fold(h, u);
        } else if (pk_ == XO) {
            rdr[r]->reload_verified<Batch>(in);
            const Batch& b = rdr[r]->root<Batch>();
            h = b.seq_first;
            for (const auto& u : b.updates) h = fold(h, u);
        } else if (pk_ == XO_ROOT) {
            rdr[r]->reload_verified<NormUpdate>(in);
            return fold(h, rdr[r]->root<NormUpdate>());
        }
        return h;
    };

    using clk = std::chrono::steady_clock;
    double enc_ns = 0, read_ns = 0;
    uint64_t digest = 0, bytes = 0;
    const bool ir_enc = ir == "encode", ir_read = ir == "read";
    (void)ir_enc; (void)ir_read;
    for (int round = 0; round < rounds; ++round) {
        for (int base = 0; base < packets; base += kRing) {
            auto t0 = clk::now();
#if defined(FIXEDMSG_IR)
            if (ir_enc && round > 0) CALLGRIND_TOGGLE_COLLECT;
#endif
            for (int i = 0; i < kRing; ++i) encode(base + i, ring[std::size_t(i)]);
#if defined(FIXEDMSG_IR)
            if (ir_enc && round > 0) CALLGRIND_TOGGLE_COLLECT;
#endif
            auto t1 = clk::now();
#if defined(FIXEDMSG_IR)
            if (ir_read && round > 0) CALLGRIND_TOGGLE_COLLECT;
#endif
            for (int r = 0; r < kReaders; ++r)
                for (int i = 0; i < kRing; ++i) digest += read(r, ring[std::size_t(i)]);
#if defined(FIXEDMSG_IR)
            if (ir_read && round > 0) CALLGRIND_TOGGLE_COLLECT;
#endif
            auto t2 = clk::now();
            if (round > 0) {
                enc_ns += std::chrono::duration<double, std::nano>(t1 - t0).count();
                read_ns += std::chrono::duration<double, std::nano>(t2 - t1).count();
            }
            if (round == 0)
                for (auto& s : ring) bytes += s.size();
        }
    }
    const double pk = double(packets) * (rounds - 1);
    std::printf("{\"path\":\"%s\",\"k\":%d,\"packets\":%d,\"rounds\":%d,\"bytes_per_packet\":%.1f,"
                "\"encode_ns\":%.2f,\"read_ns_per_reader\":%.2f,\"digest\":%llu}\n",
                path.c_str(), k, packets, rounds, double(bytes) / packets, enc_ns / pk, read_ns / pk / kReaders,
                (unsigned long long)digest);
    return 0;
}
