// HFT demo: the records every part of the chain shares, and the state models.
//
// One order record (Order) is the payload of the hot-path update message and
// the element of the order book's queues: an add is pushed into the book as
// the bytes it arrived in. The XOffset state keeps the books and the symbol
// metrics table in one XBuffer; the std model is the natural in-memory form
// for the formats that encode (zpp_bits, FlatBuffers, SBE).
#pragma once
#include <cstdint>
#include <vector>

#include "xoffset_fixed_message.hpp"
#include "xoffsetdatastructure.hpp"

namespace hft {

using namespace XOffsetDatastructure;

// A resting order. 32 bytes, no padding the compiler adds.
struct alignas(8) Order {
    std::uint64_t order_id;  // exchange order reference
    std::uint64_t ts;        // ns since midnight (time priority)
    std::int64_t price;      // ITCH Price(4): 1e-4 currency units
    std::uint32_t qty;       // shares
    std::uint8_t side;       // 'B' or 'S'
    std::uint8_t pad1;       // explicit padding as scalars (no arrays: every
    std::uint16_t pad2;      // rival's reflection takes plain members)
};
static_assert(sizeof(Order) == 32);

// The normalized update the feed handler publishes. 48 bytes.
//   action: 'A' add, 'F' fill (execution), 'C' partial cancel, 'D' delete,
//           'T' trade against a non-displayed order (no book effect).
//   For F and C, order.qty is the shares removed; for D, the shares left.
//   flags: kRemovesOrder when the update leaves the order with no shares.
inline constexpr std::uint8_t kRemovesOrder = 1;
struct alignas(8) Update {
    Order order;
    std::uint32_t instrument;  // stock locate code
    std::uint32_t seq;         // feed handler sequence number
    std::uint8_t action;
    std::uint8_t flags;
    std::uint16_t pad2;
    std::uint32_t pad4;
};
static_assert(sizeof(Update) == 48);

// One row of the symbol metrics table (in the spirit of HRT's SmallGrid):
// fixed columns, updated in place on every update of the symbol.
struct alignas(8) Row {
    std::uint64_t symbol;       // 8 ASCII characters, space padded
    std::uint32_t instrument;
    std::uint32_t live_orders;
    std::uint64_t live_shares;
    std::int64_t best_bid;      // books only, else 0
    std::int64_t best_ask;      // books only, else 0
    std::int64_t last_price;
    std::uint64_t volume;
    std::uint64_t trades;
    std::uint64_t adds;
    std::uint64_t removes;      // cancels and deletes
    std::int64_t high;
    std::int64_t low;
    std::uint64_t last_ts;
};
static_assert(sizeof(Row) == 104);

// ---- XOffset state: one XBuffer ----------------------------------------------
struct Level {
    std::int64_t price;
    std::uint64_t qty;
    std::uint32_t count;
    std::uint32_t pad;
    XVector<Order> orders;  // time priority
};
struct Book {
    std::uint32_t instrument;
    std::uint32_t pad;
    std::uint64_t symbol;
    XVector<Level> bids;  // best last: ascending price
    XVector<Level> asks;  // best last: descending price
};
struct State {
    std::uint64_t seq;  // last update applied
    XVector<Book> books;
    XVector<Row> rows;  // indexed by instrument
};

// ---- std model (zpp_bits, FlatBuffers, SBE) ------------------------------------
struct StdLevel {
    std::int64_t price = 0;
    std::uint64_t qty = 0;
    std::uint32_t count = 0;
    std::uint32_t pad = 0;
    std::vector<Order> orders;
};
struct StdBook {
    std::uint32_t instrument = 0;
    std::uint32_t pad = 0;
    std::uint64_t symbol = 0;
    std::vector<StdLevel> bids;
    std::vector<StdLevel> asks;
};
struct StdState {
    std::uint64_t seq = 0;
    std::vector<StdBook> books;
    std::vector<Row> rows;
};

}  // namespace hft

XOFFSET_RECORD(hft::Order, order_id, ts, price, qty, side, pad1, pad2)
XOFFSET_RECORD(hft::Update, order, instrument, seq, action, flags, pad2, pad4)
XOFFSET_RECORD(hft::Row, symbol, instrument, live_orders, live_shares, best_bid, best_ask, last_price, volume,
               trades, adds, removes, high, low, last_ts)
XOFFSET_RECORD(hft::Level, price, qty, count, pad, orders)
XOFFSET_RECORD(hft::Book, instrument, pad, symbol, bids, asks)
XOFFSET_RECORD(hft::State, seq, books, rows)
XOFFSET_REGISTER_SCHEMA_NAME(hft::Order, "hft.Order.v1")
XOFFSET_REGISTER_SCHEMA_NAME(hft::Update, "hft.Update.v1")
XOFFSET_REGISTER_SCHEMA_NAME(hft::State, "hft.State.v1")

static_assert(XOffsetDatastructure::is_fixed_message_v<hft::Update>);
