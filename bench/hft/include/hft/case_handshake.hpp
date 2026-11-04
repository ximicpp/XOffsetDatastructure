// HFT demo, case "handshake" (claim A, second half): a strategy built from
// other type definitions is refused when it connects, before the first
// update, instead of reading the hot path wrong.
//
//   same     a strategy built from this build's types: accepted
//   swapped  an older build in which Order declares ts before order_id: the
//            same layout, so the same wire fingerprint and the same fixed
//            batch tag (view_fixed_batch accepts its batches and a raw cast
//            reads ts as the order id); the handshake rejects it (reject_names)
//   widened  a build in which Order.qty is 64-bit: another layout, so another
//            batch tag (view_fixed_batch refuses) and reject_unknown
// The hello is sent once per connection; nothing is added per update.
#pragma once
#include <cstdio>
#include <string>

#include "hft/args.hpp"
#include "hft/model.hpp"
#include "xoffset_handshake.hpp"

namespace hft_swapped {
struct alignas(8) Order {
    std::uint64_t ts;  // swapped with order_id
    std::uint64_t order_id;
    std::int64_t price;
    std::uint32_t qty;
    std::uint8_t side;
    std::uint8_t pad1;
    std::uint16_t pad2;
};
struct alignas(8) Update {
    Order order;
    std::uint32_t instrument;
    std::uint32_t seq;
    std::uint8_t action;
    std::uint8_t flags;
    std::uint16_t pad2;
    std::uint32_t pad4;
};
}  // namespace hft_swapped

namespace hft_widened {
struct alignas(8) Order {
    std::uint64_t order_id;
    std::uint64_t ts;
    std::int64_t price;
    std::uint64_t qty;  // 64-bit
    std::uint8_t side;
    std::uint8_t pad1;
    std::uint16_t pad2;
    std::uint32_t pad4;
};
struct alignas(8) Update {
    Order order;
    std::uint32_t instrument;
    std::uint32_t seq;
    std::uint8_t action;
    std::uint8_t flags;
    std::uint16_t pad2;
    std::uint32_t pad4;
};
}  // namespace hft_widened

XOFFSET_RECORD(hft_swapped::Order, ts, order_id, price, qty, side, pad1, pad2)
XOFFSET_RECORD(hft_swapped::Update, order, instrument, seq, action, flags, pad2, pad4)
XOFFSET_REGISTER_SCHEMA_NAME(hft_swapped::Order, "hft.Order.v1")
XOFFSET_REGISTER_SCHEMA_NAME(hft_swapped::Update, "hft.Update.v1")
XOFFSET_RECORD(hft_widened::Order, order_id, ts, price, qty, side, pad1, pad2, pad4)
XOFFSET_RECORD(hft_widened::Update, order, instrument, seq, action, flags, pad2, pad4)
XOFFSET_REGISTER_SCHEMA_NAME(hft_widened::Order, "hft.Order.v1")
XOFFSET_REGISTER_SCHEMA_NAME(hft_widened::Update, "hft.Update.v1")

namespace hft {

inline const char* verdict_name(std::uint32_t v) {
    switch (XSchemaVerdict(v)) {
    case XSchemaVerdict::same: return "same";
    case XSchemaVerdict::convert: return "convert";
    case XSchemaVerdict::reject_unknown: return "reject_unknown";
    case XSchemaVerdict::reject_names: return "reject_names";
    }
    return "?";
}

// The feed side accepts strategies that work in hft::Update; `Peer` is the
// strategy's Update.
template <class Peer>
void handshake_one(const char* name, bool last) {
    const std::string hello = make_schema_hello<Peer>(std::string("strategy-") + name);
    std::string ack;
    const auto res = accept_schema_hello<Update>(hello, ack, "feed-handler");
    const auto peer = read_schema_ack<Peer>(ack);
    const auto* e = res.find(wire_root_type_id_v<Peer>());

    // One batch as the feed publishes it, viewed by the strategy's build.
    Update u{};
    u.order = Order{1001, 34200000000123ull, 1234500, 300, 'B', 0, 0};
    u.instrument = 7;
    u.seq = 42;
    u.action = 'A';
    alignas(8) std::byte buf[fixed_batch_bytes<Update>(1)];
    (void)write_fixed_batch<Update>(buf, std::span<const Update>(&u, 1));
    const auto view = view_fixed_batch<Peer>(std::span<const std::byte>(buf, sizeof buf));
    const auto* raw = reinterpret_cast<const Peer*>(buf + sizeof(XFixedBatchHeader));
    std::printf("    {\"strategy\":\"%s\",\"hello_bytes\":%zu,\"ack_bytes\":%zu,\"verdict\":\"%s\",\"accepted\":%s,"
                "\"strategy_sees_rejection\":%s,\"same_wire_fingerprint\":%s,\"same_batch_tag\":%s,"
                "\"view_fixed_batch\":\"%s\",\"raw_cast_order_id\":%llu}%s\n",
                name, hello.size(), ack.size(), e ? verdict_name(std::uint32_t(e->verdict)) : "none",
                res.accepted() ? "true" : "false", peer ? "false" : "true",
                wire_fingerprint_v<Peer> == wire_fingerprint_v<Update> ? "true" : "false",
                fixed_message_tag_v<Peer> == fixed_message_tag_v<Update> ? "true" : "false",
                view ? "ok" : view.status == XFixedMessageStatus::wrong_tag ? "wrong_tag" : "wrong_size",
                (unsigned long long)raw->order.order_id, last ? "" : ",");
}

inline int run_handshake(const Args&) {
    std::printf("{\"case\":\"handshake\",\"sent_order_id\":1001,\"runs\":[\n");
    handshake_one<Update>("same", false);
    handshake_one<hft_swapped::Update>("swapped", false);
    handshake_one<hft_widened::Update>("widened", true);
    std::printf("]}\n");
    return 0;
}

}  // namespace hft
