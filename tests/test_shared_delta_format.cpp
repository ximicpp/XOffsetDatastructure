// XS 1.0, the shared delta message format (docs/SHARED_DELTA.md section 2),
// frozen: the bytes of a key message and of the delta after it are pinned
// here, for the sender (XSyncWriter, XRecordDelta) and for the receiver
// (XReceiver::apply_sync with XCheck::full and XCheck::admit). A change to
// any of these bytes is a new format, with its own format byte.
//
// Built with XOFFSET_FORCE_FULL_CHECK=0 so admit is the real admit path. No
// exception of its own: also built as test_shared_delta_format_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include "../xoffset_receiver.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <tuple>

namespace xo = XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

namespace sdf {
struct Head { std::int64_t tick{}; std::uint32_t seq{}; };
struct Item {
    std::uint32_t hp{};
    bool on{};
    std::int16_t pos{};
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
};
struct View { Head head; xo::XVector<Item> items; };
} // namespace sdf
XOFFSET_RECORD(sdf::Head, tick, seq)
XOFFSET_RECORD(sdf::Item, hp, on, pos, name, marks)
XOFFSET_RECORD(sdf::View, head, items)
XOFFSET_REGISTER_SCHEMA_NAME(sdf::View, "test.shared_delta_format.view")

namespace {
[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "test_shared_delta_format: %s\n", what);
    std::exit(1);
}

std::string hex(std::string_view s) {
    std::string out;
    char b[4];
    for (unsigned char c : s) { std::snprintf(b, sizeof b, "%02x ", c); out += b; }
    return out;
}

void same_bytes(std::string_view got, std::string_view want, const char* what) {
    if (got == want) return;
    std::fprintf(stderr, "test_shared_delta_format: %s\n  got  %s\n  want %s\n", what, hex(got).c_str(), hex(want).c_str());
    std::exit(1);
}

// The client: items keyed by id in slots of its own view.
struct Target {
    using tables = std::tuple<sdf::Head, sdf::Item>;
    template <unsigned K> static constexpr bool keyed = K == 1;
    xo::XBuffer* buf;
    std::map<std::uint32_t, std::int64_t> idx;
    sdf::View& view() { return buf->root<sdf::View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == 0) return id == 0 ? 0 : -1;
        else {
            const auto it = idx.find(id);
            return it == idx.end() ? -1 : it->second;
        }
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == 0) return view().head;
        else return view().items[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t id) {
        const auto s = std::int64_t(view().items.size());
        view().items.resize(view().items.size() + 1);
        idx.emplace(id, s);
        return s;
    }
    template <unsigned K> void leave(std::uint32_t id, std::int64_t) { idx.erase(id); }
};

xo::XBuffer make() {
    return xo::XBuffer::create<sdf::View>(4096, xo::XBuffer::maximum_capacity(std::size_t(1) << 20).with_growth(xo::XGrowPolicy::Automatic));
}

void set(sdf::Item& it, std::uint32_t hp, bool on, std::int16_t pos, std::string_view name, std::initializer_list<std::uint16_t> marks) {
    it.hp = hp;
    it.on = on;
    it.pos = pos;
    it.name = name;
    it.marks.resize(marks.size());
    std::size_t i = 0;
    for (auto m : marks) it.marks[i++] = m;
}

bool holds(Target& t, std::uint32_t id, std::uint32_t hp, bool on, std::int16_t pos, std::string_view name,
           std::initializer_list<std::uint16_t> marks) {
    const std::int64_t h = t.find<1>(id);
    if (h < 0) return false;
    const sdf::Item& it = t.view().items[std::size_t(h)];
    if (it.hp != hp || it.on != on || it.pos != pos || it.name.view() != name || it.marks.size() != marks.size()) return false;
    std::size_t i = 0;
    for (auto m : marks) if (it.marks[i++] != m) return false;
    return true;
}

// XS 1.0: 'X' 'S' 01 flags [stream u64 LE if KEY] varint version, count
// as three varint bytes, then ops: u8 (table << 2 | kind), varint id, body.
// Key: stream 0x0102030405060708, version 1, three ops: the head enters
// {tick 5, seq 1}; item 7 enters {hp 100, on, pos -2, "ab", [1, 2]}; item
// 300 enters {hp 1}.
constexpr unsigned char kKey[] = {
    'X', 'S', 0x01, 0x01, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x01, 0x83, 0x80, 0x00,
    // head, enter, id 0: one block (skip 0, words 0 and 1, one byte each): 05, 01
    0x00, 0x00, 0x01, 0x00, 0x03, 0x01, 0x01, 0x05, 0x01,
    // item, enter, id 7: one block (word 0: bytes 0, 4, 6, 7): 64 01 fe ff;
    // two container ops: name STR "ab", marks VSET [1, 2]
    0x04, 0x07, 0x01, 0x00, 0x01, 0xd1, 0x64, 0x01, 0xfe, 0xff, 0x02, 0x00, 0x02, 'a', 'b', 0x05, 0x02, 0x01, 0x00, 0x02, 0x00,
    // item, enter, id 300: one block (word 0: byte 0): 01; no container op
    0x04, 0xac, 0x02, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00,
};
// Delta: version 2, four ops: the head changes (tick 6, seq 2); item 300
// leaves; item 7 changes (hp 90, name "abc"); item 9 enters {hp 3, pos 4}.
constexpr unsigned char kDelta[] = {
    'X', 'S', 0x01, 0x00, 0x02, 0x84, 0x80, 0x00,
    // head, change, id 0: words 0 and 1, one byte each: 06, 02
    0x01, 0x00, 0x01, 0x00, 0x03, 0x01, 0x01, 0x06, 0x02,
    // item, leave, id 300
    0x06, 0xac, 0x02,
    // item, change, id 7: word 0 byte 0: 5a; one container op: name STR "abc"
    0x05, 0x07, 0x01, 0x00, 0x01, 0x01, 0x5a, 0x01, 0x00, 0x03, 'a', 'b', 'c',
    // item, enter, id 9: word 0 bytes 0 and 6: 03, 04
    0x04, 0x09, 0x01, 0x00, 0x01, 0x41, 0x03, 0x04, 0x00,
};

std::string_view bytes(const unsigned char* p, std::size_t n) { return {reinterpret_cast<const char*>(p), n}; }
} // namespace

int main() {
    // Sender.
    auto server = make();
    auto base = make();
    sdf::View& S = server.root<sdf::View>();
    sdf::View& B = base.root<sdf::View>();
    S.items.resize(2);
    B.items.resize(2);
    S.head.tick = 5;
    S.head.seq = 1;
    set(S.items[0], 100, true, -2, "ab", {1, 2});
    set(S.items[1], 1, false, 0, "", {});
    xs::XSyncOut msg, body;
    {
        xs::XSyncWriter m(msg, xs::kFlagKey, 0x0102030405060708ull, 1);
        auto key = [&](unsigned table, std::uint32_t id, const auto& now, auto& before) {
            body.clear();
            xs::XRecordDelta<std::remove_cvref_t<decltype(now)>>::key(now, body);
            m.op(table, xs::enter, id, body.data(), body.size());
            xs::XRecordDelta<std::remove_cvref_t<decltype(now)>>::apply(body.data(), before);
        };
        key(0, 0, S.head, B.head);
        key(1, 7, S.items[0], B.items[0]);
        key(1, 300, S.items[1], B.items[1]);
        m.finish();
    }
    const std::string key_msg(msg.view());
    same_bytes(key_msg, bytes(kKey, sizeof kKey), "key message bytes");
    S.head.tick = 6;
    S.head.seq = 2;
    S.items[0].hp = 90;
    S.items[0].name = std::string_view("abc");
    {
        xs::XSyncWriter m(msg, 0, 0, 2);
        if (!m.change(0, 0, S.head, B.head)) fail("head unchanged");
        m.op(1, xs::leave, 300);
        if (!m.change(1, 7, S.items[0], B.items[0])) fail("item 7 unchanged");
        sdf::Item& nine = S.items[1];   // the slot item 300 left
        xs::XRecordDelta<sdf::Item>::clear(nine);
        set(nine, 3, false, 4, "", {});
        body.clear();
        xs::XRecordDelta<sdf::Item>::key(nine, body);
        m.op(1, xs::enter, 9, body.data(), body.size());
        m.finish();
    }
    same_bytes(msg.view(), bytes(kDelta, sizeof kDelta), "delta message bytes");
    if (!xs::is_key(bytes(kKey, sizeof kKey)) || xs::is_key(bytes(kDelta, sizeof kDelta))) fail("is_key");

    // Receiver, both tiers.
    xo::XReceiver<xo::XLink::from_server> rx(xo::XBuffer::maximum_capacity(std::size_t(1) << 20));
    for (int tier = 0; tier < 2; ++tier) {
        auto view = make();
        Target t{&view, {}};
        xs::XSyncCursor cur;
        auto apply = [&](std::string_view m) {
            return tier == 0 ? rx.apply_sync<xo::XCheck::full>(m, cur, t) : rx.apply_sync<xo::XCheck::admit>(m, cur, t);
        };
        if (!apply(bytes(kKey, sizeof kKey))) fail("key rejected");
        if (cur.stream() != 0x0102030405060708ull || cur.version() != 1 || cur.empty()) fail("cursor after the key");
        if (t.view().head.tick != 5 || t.view().head.seq != 1) fail("head after the key");
        if (!holds(t, 7, 100, true, -2, "ab", {1, 2}) || !holds(t, 300, 1, false, 0, "", {})) fail("items after the key");
        if (!apply(bytes(kDelta, sizeof kDelta))) fail("delta rejected");
        if (cur.version() != 2 || t.view().head.tick != 6 || t.view().head.seq != 2) fail("head after the delta");
        if (!holds(t, 7, 90, true, -2, "abc", {1, 2}) || !holds(t, 9, 3, false, 4, "", {}) || t.find<1>(300) >= 0)
            fail("items after the delta");
        // Another format byte is another format: a 1.0 receiver refuses it.
        std::string other(bytes(kDelta, sizeof kDelta));
        other[2] = 0x02;
        if (apply(other).code() != xo::XErrc::unsupported_version) fail("format byte 2 not refused");
        other[2] = 0x01;
        other[3] = 0x02;
        if (apply(other).code() != xo::XErrc::unsupported_flags) fail("unknown flag not refused");
    }
    std::printf("test_shared_delta_format: XS 1.0 key (%zu bytes) and delta (%zu bytes) pinned, full and admit apply them\n",
                sizeof kKey, sizeof kDelta);
    return 0;
}
