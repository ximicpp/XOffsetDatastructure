// Fixed messages (xoffset_fixed_message.hpp): which types qualify, single
// and batch round trips (copied and built in place), every rejection a view
// makes (size, alignment, tag, bool bytes), tag dispatch over one
// connection, binding the types through the schema handshake, and the same
// records as elements of an ordinary XOffset root.
#include "../xoffset_fixed_message.hpp"
#include "../xoffset_handshake.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

using namespace XOffsetDatastructure;

namespace {
int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                            \
        }                                                                          \
    } while (0)
} // namespace

namespace mkt {
enum class Side : std::uint8_t { buy = 'B', sell = 'S' };
struct alignas(8) Quote {
    std::uint64_t ts;
    std::int64_t price;
    std::uint32_t qty;
    std::uint32_t instrument;
    Side side;
    std::uint8_t flags;
    std::uint8_t reserved[6];
};
// The same layout under another schema name: another tag.
struct alignas(8) QuoteMirror {
    std::uint64_t ts;
    std::int64_t price;
    std::uint32_t qty;
    std::uint32_t instrument;
    Side side;
    std::uint8_t flags;
    std::uint8_t reserved[6];
};
struct Ack {
    std::uint64_t order_id;
    std::uint32_t qty;
    bool accepted;
    bool last;
    std::uint8_t reserved[2];
};
struct Level {
    std::int64_t price;
    std::uint32_t qty;
    std::uint32_t orders;
};
struct Book {
    std::uint32_t instrument;
    std::uint32_t depth;
    Level bids[4];
    Level asks[4];
};
struct Fill {
    std::uint64_t order_id;
    std::uint32_t qty;
    bool aggressor[4];
};
struct Named {
    std::uint64_t id;
    XString name;
};
struct Sizes {
    std::uint64_t id;
    XVector<std::uint32_t> qty;
};
struct Padded {
    std::uint8_t a;
    std::uint64_t b;
};
struct Snapshot {
    std::uint64_t seq;
    XVector<Quote> quotes;
};
} // namespace mkt

XOFFSET_RECORD(mkt::Quote, ts, price, qty, instrument, side, flags, reserved)
XOFFSET_RECORD(mkt::QuoteMirror, ts, price, qty, instrument, side, flags, reserved)
XOFFSET_RECORD(mkt::Ack, order_id, qty, accepted, last, reserved)
XOFFSET_RECORD(mkt::Level, price, qty, orders)
XOFFSET_RECORD(mkt::Book, instrument, depth, bids, asks)
XOFFSET_RECORD(mkt::Fill, order_id, qty, aggressor)
XOFFSET_RECORD_DERIVED(mkt::Named, (), id, name)
XOFFSET_RECORD_DERIVED(mkt::Sizes, (), id, qty)
XOFFSET_RECORD(mkt::Padded, a, b)
XOFFSET_RECORD_DERIVED(mkt::Snapshot, (), seq, quotes)
XOFFSET_REGISTER_SCHEMA_NAME(mkt::Quote, "test.fixed.Quote")
XOFFSET_REGISTER_SCHEMA_NAME(mkt::QuoteMirror, "test.fixed.QuoteMirror")
XOFFSET_REGISTER_SCHEMA_NAME(mkt::Ack, "test.fixed.Ack")
XOFFSET_REGISTER_SCHEMA_NAME(mkt::Book, "test.fixed.Book")
XOFFSET_REGISTER_SCHEMA_NAME(mkt::Fill, "test.fixed.Fill")
XOFFSET_REGISTER_SCHEMA_NAME(mkt::Snapshot, "test.fixed.Snapshot")

// Which types qualify.
static_assert(is_fixed_message_v<mkt::Quote> && fixed_message<mkt::Quote>);
static_assert(is_fixed_message_v<mkt::Ack>);
static_assert(is_fixed_message_v<mkt::Level> && is_fixed_message_v<mkt::Book>);
static_assert(is_fixed_message_v<mkt::Fill>);
static_assert(is_fixed_message_v<const mkt::Quote>);
static_assert(!is_fixed_message_v<mkt::Named>);     // XString inside
static_assert(!is_fixed_message_v<mkt::Sizes>);     // XVector inside
static_assert(!is_fixed_message_v<mkt::Snapshot>);  // XVector of fixed records is still a container
static_assert(!is_fixed_message_v<mkt::Padded>);    // implicit padding
static_assert(!is_fixed_message_v<std::uint64_t>);  // a message is a record
static_assert(!is_fixed_message_v<std::string>);
// Tags: never 0, and differ between two types of the same layout.
static_assert(fixed_message_tag_v<mkt::Quote> != fixed_message_tag_v<mkt::QuoteMirror>);
static_assert(fixed_message_tag_v<mkt::Quote> != 0 && fixed_message_tag_v<mkt::Ack> != 0);
static_assert(fixed_message_tag_v<const mkt::Quote> == fixed_message_tag_v<mkt::Quote>);
static_assert(fixed_batch_bytes<mkt::Quote>(3) == 8 + 3 * sizeof(mkt::Quote));

namespace {

alignas(16) std::byte g_buf[4096];

std::span<std::byte> out(std::size_t offset = 0, std::size_t size = sizeof(g_buf)) {
    return std::span<std::byte>(g_buf + offset, size - offset);
}
std::span<const std::byte> in(std::size_t offset, std::size_t size) {
    return std::span<const std::byte>(g_buf + offset, size);
}

mkt::Quote quote(std::uint32_t i) {
    mkt::Quote q{};
    q.ts = 34200000000000ull + i;
    q.price = 1000000 + std::int64_t(i) * 100;
    q.qty = 100 + i;
    q.instrument = 7 + i % 3;
    q.side = i % 2 ? mkt::Side::sell : mkt::Side::buy;
    q.flags = std::uint8_t(i);
    return q;
}

bool same(const mkt::Quote& a, const mkt::Quote& b) { return std::memcmp(&a, &b, sizeof a) == 0; }

void single() {
    const mkt::Quote q = quote(1);
    CHECK(write_fixed_message<mkt::Quote>(out(), q) == sizeof q);
    auto v = view_fixed_message<mkt::Quote>(in(0, sizeof q));
    CHECK(v && v.status == XFixedMessageStatus::ok);
    CHECK(v.record == reinterpret_cast<const mkt::Quote*>(g_buf));  // read where it lies
    CHECK(same(*v, q) && v->qty == 101 && v->side == mkt::Side::sell);

    CHECK(write_fixed_message<mkt::Quote>(out(0, sizeof q - 1), q) == 0);
    CHECK(view_fixed_message<mkt::Quote>(in(0, sizeof q - 1)).status == XFixedMessageStatus::wrong_size);
    CHECK(view_fixed_message<mkt::Quote>(in(0, sizeof q + 1)).status == XFixedMessageStatus::wrong_size);
    CHECK(view_fixed_message<mkt::Quote>(in(0, 0)).status == XFixedMessageStatus::wrong_size);
    CHECK(!view_fixed_message<mkt::Quote>(in(0, 0)).record);

    CHECK(write_fixed_message<mkt::Quote>(out(4), q) == sizeof q);
    CHECK(view_fixed_message<mkt::Quote>(in(4, sizeof q)).status == XFixedMessageStatus::misaligned);
}

void bool_values() {
    const mkt::Ack a{42, 10, true, false, {0, 0}};
    CHECK(write_fixed_message<mkt::Ack>(out(), a) == sizeof a);
    auto v = view_fixed_message<mkt::Ack>(in(0, sizeof a));
    CHECK(v && v->order_id == 42 && v->accepted && !v->last);
    g_buf[offsetof(mkt::Ack, last)] = std::byte{2};
    CHECK(view_fixed_message<mkt::Ack>(in(0, sizeof a)).status == XFixedMessageStatus::invalid_value);
    g_buf[offsetof(mkt::Ack, last)] = std::byte{1};
    CHECK(view_fixed_message<mkt::Ack>(in(0, sizeof a)));

    // bool inside an array, inside a batch.
    const std::array<mkt::Fill, 3> fills{
        {{1, 5, {true, false, false, true}}, {2, 6, {}}, {3, 7, {false, true, true, false}}}};
    const std::size_t n = write_fixed_batch<mkt::Fill>(out(), fills);
    CHECK(n == fixed_batch_bytes<mkt::Fill>(3));
    auto b = view_fixed_batch<mkt::Fill>(in(0, n));
    CHECK(b && b.records.size() == 3 && b.records[2].aggressor[1] && !b.records[1].aggressor[3]);
    g_buf[8 + 2 * sizeof(mkt::Fill) + offsetof(mkt::Fill, aggressor) + 3] = std::byte{0xFF};
    b = view_fixed_batch<mkt::Fill>(in(0, n));
    CHECK(b.status == XFixedMessageStatus::invalid_value && b.records.empty());
}

void batch() {
    std::array<mkt::Quote, 16> qs;
    for (std::uint32_t i = 0; i < qs.size(); ++i) qs[i] = quote(i);
    const std::size_t n = write_fixed_batch<mkt::Quote>(out(), qs);
    CHECK(n == 8 + 16 * sizeof(mkt::Quote));
    CHECK(peek_fixed_batch_tag(in(0, n)) == fixed_message_tag_v<mkt::Quote>);
    auto v = view_fixed_batch<mkt::Quote>(in(0, n));
    CHECK(v && v.records.size() == 16);
    CHECK(static_cast<const void*>(v.records.data()) == static_cast<const void*>(g_buf + 8));
    for (std::uint32_t i = 0; i < qs.size(); ++i) CHECK(same(v.records[i], qs[i]));

    // Wrong size: truncated, one byte too many, shorter than the header.
    CHECK(view_fixed_batch<mkt::Quote>(in(0, n - 1)).status == XFixedMessageStatus::wrong_size);
    CHECK(view_fixed_batch<mkt::Quote>(in(0, n + 1)).status == XFixedMessageStatus::wrong_size);
    CHECK(view_fixed_batch<mkt::Quote>(in(0, 7)).status == XFixedMessageStatus::wrong_size);
    CHECK(peek_fixed_batch_tag(in(0, 7)) == 0);
    // A count that does not match the bytes.
    const std::uint32_t big = 0xFFFFFFFFu;
    std::memcpy(g_buf, &big, 4);
    CHECK(view_fixed_batch<mkt::Quote>(in(0, n)).status == XFixedMessageStatus::wrong_size);
    // Another type of the same layout: the tag tells them apart.
    CHECK(write_fixed_batch<mkt::Quote>(out(), qs) == n);
    CHECK(view_fixed_batch<mkt::QuoteMirror>(in(0, n)).status == XFixedMessageStatus::wrong_tag);
    // Output too small or misaligned.
    CHECK(write_fixed_batch<mkt::Quote>(out(0, n - 1), qs) == 0);
    CHECK(write_fixed_batch<mkt::Quote>(out(4), qs) == 0);
    CHECK(!place_fixed_batch<mkt::Quote>(out(4), 1));
    CHECK(write_fixed_batch<mkt::Quote>(out(8), qs) == n);
    CHECK(view_fixed_batch<mkt::Quote>(in(8, n)));
    CHECK(view_fixed_batch<mkt::Quote>(in(12, n)).status == XFixedMessageStatus::misaligned);

    // Empty batch: valid, no records.
    CHECK(write_fixed_batch<mkt::Quote>(out(), std::span<const mkt::Quote>()) == 8);
    auto e = view_fixed_batch<mkt::Quote>(in(0, 8));
    CHECK(e && e.records.empty());
}

void in_place() {
    auto slot = place_fixed_batch<mkt::Book>(out(), 2);
    CHECK(slot && slot.bytes == fixed_batch_bytes<mkt::Book>(2) && slot.records.size() == 2);
    for (std::uint32_t i = 0; i < 2; ++i) {
        mkt::Book& b = slot.records[i];
        b = mkt::Book{};
        b.instrument = 100 + i;
        b.depth = 4;
        for (std::uint32_t d = 0; d < 4; ++d) {
            b.bids[d] = {1000 - std::int64_t(d), 10 * (d + 1), d + 1};
            b.asks[d] = {1001 + std::int64_t(d), 20 * (d + 1), d + 2};
        }
    }
    auto v = view_fixed_batch<mkt::Book>(in(0, slot.bytes));
    CHECK(v && v.records.size() == 2);
    CHECK(v.records[1].instrument == 101 && v.records[1].asks[3].price == 1004 && v.records[0].bids[2].qty == 30);
    auto e = place_fixed_batch<mkt::Book>(out(), 0);
    CHECK(e && e.bytes == 8 && e.records.empty());
}

// One connection, several fixed types: switch on the tag.
int dispatch(std::span<const std::byte> msg, std::uint64_t& sum) {
    switch (peek_fixed_batch_tag(msg)) {
    case fixed_message_tag_v<mkt::Quote>:
        if (auto v = view_fixed_batch<mkt::Quote>(msg)) {
            for (const auto& q : v.records) sum += q.qty;
            return 1;
        }
        return -1;
    case fixed_message_tag_v<mkt::Ack>:
        if (auto v = view_fixed_batch<mkt::Ack>(msg)) {
            for (const auto& a : v.records) sum += a.order_id;
            return 2;
        }
        return -1;
    default:
        return 0;
    }
}

void tag_dispatch() {
    std::uint64_t sum = 0;
    const std::array<mkt::Quote, 2> qs{quote(0), quote(1)};
    std::size_t n = write_fixed_batch<mkt::Quote>(out(), qs);
    CHECK(dispatch(in(0, n), sum) == 1 && sum == 100 + 101);
    const std::array<mkt::Ack, 1> as{{{9, 1, true, true, {0, 0}}}};
    n = write_fixed_batch<mkt::Ack>(out(), as);
    CHECK(dispatch(in(0, n), sum) == 2 && sum == 201 + 9);
    n = write_fixed_batch<mkt::QuoteMirror>(out(), std::span<const mkt::QuoteMirror>());
    CHECK(dispatch(in(0, n), sum) == 0);
    CHECK(dispatch(in(0, 3), sum) == 0);
}

// The types are bound once per connection by the ordinary handshake.
void handshake() {
    const std::string hello = make_schema_hello<mkt::Quote, mkt::Ack>("feed-1");
    std::string ack;
    const auto r = accept_schema_hello<mkt::Quote, mkt::Ack>(hello, ack);
    CHECK(r.accepted() && r.entries.size() == 2);
    CHECK((read_schema_ack<mkt::Quote, mkt::Ack>(ack)));
    // A peer whose Quote is another type is refused before any message.
    std::string ack2;
    const auto r2 = accept_schema_hello<mkt::QuoteMirror, mkt::Ack>(hello, ack2);
    CHECK(r2.valid && !r2.accepted());
}

// The same records as elements of an ordinary root: identical bytes.
void inside_root() {
    std::array<mkt::Quote, 4> qs;
    for (std::uint32_t i = 0; i < qs.size(); ++i) qs[i] = quote(i);
    auto b = TypedXBuffer<mkt::Snapshot>::create();
    b.root().seq = 5;
    b.root().quotes.assign_range(std::span<const mkt::Quote>(qs));
    const std::string wire = b.save_verified();
    auto l = TypedXBuffer<mkt::Snapshot>::load_verified(wire);
    CHECK(l.root().quotes.size() == 4);
    const std::size_t n = write_fixed_batch<mkt::Quote>(out(), qs);
    auto v = view_fixed_batch<mkt::Quote>(in(0, n));
    CHECK(v && std::memcmp(v.records.data(), &l.root().quotes[0], 4 * sizeof(mkt::Quote)) == 0);
}

} // namespace

int main() {
    single();
    bool_values();
    batch();
    in_place();
    tag_dispatch();
    handshake();
    inside_root();
    if (failures) {
        std::fprintf(stderr, "test_fixed_message: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_fixed_message: all checks passed\n");
    return 0;
}
