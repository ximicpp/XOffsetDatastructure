// Small deltas (xoffset_delta.hpp): encode_small writes the delta of an
// XDeltaRecord::touches baseline whose touch log lists at most 8 blocks, and
// apply_small applies a delta with no flags and at most 8 blocks on an
// admitting receiver, each in one pass. Both must give exactly what the
// general path gives.
//
// XOFFSET_TEST_DELTA_GENERAL turns on DeltaAccess::test_general_path(), a
// per-thread switch that sends every delta on this thread through the
// general path. Two sources built and edited identically, one written with
// the short path allowed and one through the general path, must give the
// same delta bytes and stats after every step, through random streams of
// touched edits (scalars, touch_set, strings, containers that grow, erasure
// that leaves holes, more blocks than the list holds), options (digest, whole
// blocks), a copy baseline pairing in between (a shared touch log), a
// baseline object that moves (another owner of the log), resets and a
// content replacement (key deltas). Each delta is applied to two replicas,
// one per path, through XReceiver<from_server> (trust tier admit); the
// replicas must hold the same payload, holes and cursor, and each relays
// what it applies with a touches baseline of its own (the receiver's touch
// log replay), whose deltas must also match. Corrupted copies of each delta
// (flipped, truncated, extended, a changed block count, version or stream)
// applied to clones of both replicas must have the same outcome, error and
// result. Counters check that the short paths ran.
#define XOFFSET_TEST_DELTA_GENERAL 1
#include "fuzz/v2_fuzz_core.hpp"

namespace {
using Access = xo::detail::DeltaAccess;

void need(bool ok, const std::string& what) {
    if (!ok) fail(what);
}

std::string payload_of(const xo::XBuffer& b) {
    const auto* sm = b.arena();
    return std::string(static_cast<const char*>(b.get_address()), static_cast<std::size_t>(sm->used_end));
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> holes_of(const xo::XBuffer& b) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    // In-band, the free blocks are payload bytes (payload_of compares them),
    // and a corrupted delta this trusted link accepts can leave a directory
    // that names bytes outside the payload: not walked.
    if constexpr (xo::detail::inband_allocator) return out;
    b.for_each_free_block([&](xo::XBufferCore::WireHole h) { out.emplace_back(h.offset, h.bytes); });
    std::sort(out.begin(), out.end());
    return out;
}

// Runs fn with deltas on this thread forced through the general path or not.
template <class Fn>
decltype(auto) on_path(bool general, Fn&& fn) {
    struct Restore {
        bool old;
        ~Restore() { Access::test_general_path() = old; }
    } restore{Access::test_general_path()};
    Access::test_general_path() = general;
    return fn();
}

void fill(fz::Root& r, std::mt19937_64& rng) {
    r.epoch = rng();
    r.title = random_text(rng, 30);
    for (int i = 1 + int(rng() % 9); i > 0; --i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(rng());
        it.flag = rng() & 1;
        it.name = random_text(rng, 30);
        for (int j = int(rng() % 7); j > 0; --j) it.vals.push_back(std::uint16_t(rng()));
    }
    for (int i = int(rng() % 5); i > 0; --i) r.tags.emplace_back(random_text(rng, 20), r.tags.get_stored_allocator());
    for (int i = int(rng() % 6); i > 0; --i) r.keys.insert(std::uint32_t(rng() % 1000));
    r.attrs.insert_or_assign(std::string_view("a"), std::uint64_t(rng()));
}

// Touched edits, mostly small (one field, one block): the deltas the short
// path takes; sometimes growth, erasure or many blocks: the general path.
void mutate(Buffer& src, std::mt19937_64& rng) {
    auto& r = src.plain_root();
    for (int n = 1 + int(rng() % 3); n > 0; --n) {
        switch (rng() % 16) {
        case 0: case 1: case 2:
            src.touch_set(r.epoch, r.epoch + rng() % 300);  // the low bytes
            break;
        case 3:
            r.epoch = rng();
            src.touch(r.epoch);
            break;
        case 4: case 5:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (rng() % 2) { it.id = std::uint32_t(rng()); src.touch(it.id); }
                else src.touch_set(it.id, std::uint32_t(it.id ^ (rng() % 256) << (8 * (rng() % 4))));
            }
            break;
        case 6:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                src.touch_set(it.flag, rng() % 2 == 0);
            }
            break;
        case 7:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (!it.vals.empty()) src.touch_set(it.vals[rng() % it.vals.size()], std::uint16_t(rng()));
            }
            break;
        case 8:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                it.name = random_text(rng, 40);  // inline or heap, may move to a new block
                src.touch(it.name);
            }
            break;
        case 9: if (!r.attrs.empty()) { auto& v = r.attrs.begin()->second; v = rng(); src.touch(v); } break;
        case 10: r.title = random_text(rng, 150); src.touch(r.title); break;  // whole blocks
        case 11:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                for (int i = int(rng() % 9); i > 0; --i) it.vals.push_back(std::uint16_t(rng()));
                if (rng() % 3 == 0 && !it.vals.empty()) it.vals.pop_back();
                src.touch(it.vals);
            }
            break;
        case 12:
            if (r.items.size() > 1) {  // a hole
                r.items.erase(r.items.begin() + std::ptrdiff_t(rng() % r.items.size()));
                src.touch(r.items);
            }
            break;
        case 13:
            if (rng() % 4 == 0) {  // everything: more blocks than the list holds
                for (auto& it : r.items) it.id += 1;
                src.touch(r);
            }
            break;
        case 14: {  // raw bytes of an object, every byte
            r.epoch = ~std::uint64_t(0) / 255u * (1 + rng() % 254);
            src.touch(&r.epoch, sizeof r.epoch);
        } break;
        case 15: break;  // nothing: an empty delta
        }
    }
}

using Receiver = xo::XReceiver<xo::XLink::from_server>;

struct Stats {
    std::size_t steps = 0, deltas = 0, empties = 0, corrupt = 0, corrupt_accepted = 0;
} totals;

void same_stats(const xo::XDeltaStats& a, const xo::XDeltaStats& b, const std::string& where) {
    need(a.key == b.key && a.holes == b.holes && a.empty == b.empty && a.blocks == b.blocks &&
             a.changed_bytes == b.changed_bytes && a.compared_bytes == b.compared_bytes &&
             a.delta_bytes == b.delta_bytes,
         where + ": stats differ between the short and the general path");
}

void same_replica(const Buffer& a, const xo::XDeltaCursor& ca, const Buffer& b, const xo::XDeltaCursor& cb,
                  const std::string& where) {
    need(payload_of(a) == payload_of(b), where + ": replica payloads differ");
    need(holes_of(a) == holes_of(b), where + ": replica hole tables differ");
    need(ca.stream() == cb.stream() && ca.version() == cb.version() && ca.last_check() == cb.last_check(),
         where + ": cursors differ");
}

// Applies d to rep on one path; the status.
xo::XStatus apply(bool general, Buffer& rep, const std::string& d, xo::XDeltaCursor& cur) {
    static Receiver rx(xo::XBuffer::maximum_capacity(kLimit));
    return on_path(general, [&] { return rx.apply_delta(rep, d, cur); });
}

// Corrupted copies of d: the same outcome, error and result on both paths.
void corrupt_trials(const Buffer& ra, const xo::XDeltaCursor& ca, const Buffer& rb, const xo::XDeltaCursor& cb,
                    const std::string& d, std::mt19937_64& rng, const std::string& where) {
    for (int k = 0; k < 4; ++k) {
        std::string bad = d;
        switch (rng() % 8) {
        case 0: bad[rng() % bad.size()] ^= char(1u << (rng() % 8)); break;
        case 1: bad.resize(rng() % bad.size()); break;
        case 2: bad.push_back(char(rng())); break;
        case 3: if (bad.size() > 12) bad[12 + rng() % (bad.size() - 12)] = char(rng()); break;  // past the stream
        case 4: if (bad.size() > 13) bad[13] = char(rng() % 12); break;  // a block count (one-byte version)
        case 5: if (bad.size() > 12) bad[12] = char(bad[12] + 1); break;  // the version
        case 6: bad[4 + rng() % 8] ^= 1; break;  // the stream
        case 7: bad[3] = char(rng() % 16); break;  // the flags
        }
        auto a = ra.clone();
        auto b = rb.clone();
        auto cur_a = ca;
        auto cur_b = cb;
        const auto sa = apply(false, a, bad, cur_a);
        const auto sb = apply(true, b, bad, cur_b);
        ++totals.corrupt;
        need(bool(sa) == bool(sb), where + ": a corrupted delta has different outcomes on the two paths");
        if (!sa) {
            need(sa.error().code == sb.error().code && sa.error().message == sb.error().message,
                 where + ": a corrupted delta is rejected differently: " + sa.error().message + " / " +
                     sb.error().message);
        } else {
            ++totals.corrupt_accepted;
        }
        same_replica(a, cur_a, b, cur_b, where + " (corrupted)");
    }
}

void stream(std::uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    // The same content in both sources (fill and every edit replay one rng).
    auto src_a = Buffer::create(4096, cap);
    auto src_b = Buffer::create(4096, cap);
    {
        auto fill_rng = rng;
        fill(src_a.plain_root(), fill_rng);
        fill_rng = rng;
        fill(src_b.plain_root(), fill_rng);
        rng = fill_rng;
    }
    const std::uint64_t stream_id = seed * 2 + 1;
    auto base_a = std::make_unique<xo::XDeltaBaseline>(xo::XDeltaRecord::touches, stream_id);
    auto base_b = std::make_unique<xo::XDeltaBaseline>(xo::XDeltaRecord::touches, stream_id);
    xo::XDeltaBaseline copy_a(stream_id + 2), copy_b(stream_id + 2);
    Buffer rep_a(4096, cap), rep_b(4096, cap);
    xo::XDeltaCursor cur_a, cur_b;
    xo::XDeltaBaseline relay_a(xo::XDeltaRecord::touches, stream_id + 4), relay_b(xo::XDeltaRecord::touches, stream_id + 4);
    Buffer down_a(4096, cap), down_b(4096, cap);
    xo::XDeltaCursor down_cur_a, down_cur_b;
    for (int step = 0; step < steps; ++step, ++totals.steps) {
        const std::string where = "seed " + std::to_string(seed) + " step " + std::to_string(step);
        const auto op = rng() % 32;
        if (op == 0) {
            auto r = rng;
            src_a.reset();
            fill(src_a.plain_root(), r);
            r = rng;
            src_b.reset();
            fill(src_b.plain_root(), r);
            rng = r;
        } else if (op == 1) {
            base_a->reset();  // the receiver asked for a key delta
            base_b->reset();
        } else if (op == 2) {
            // The baseline object moves: the log's owner is another object.
            base_a = std::make_unique<xo::XDeltaBaseline>(*base_a);
            base_b = std::make_unique<xo::XDeltaBaseline>(*base_b);
        } else {
            auto r = rng;
            mutate(src_a, r);
            r = rng;
            mutate(src_b, r);
            rng = r;
        }
        xo::XDeltaOptions options;
        options.digest = rng() % 16 == 0;
        options.whole_blocks = rng() % 16 == 0;
        std::string da = "stale a", db = "stale b";
        const auto sa = on_path(false, [&] { return src_a.save_delta_into(da, *base_a, options); });
        const auto sb = on_path(true, [&] { return src_b.save_delta_into(db, *base_b, options); });
        same_stats(sa, sb, where);
        need(da == db, where + ": delta bytes differ between the short and the general path");
        need(payload_of(src_a) == payload_of(src_b), where + ": sources differ");
        if (sa.empty) {
            ++totals.empties;
        } else {
            ++totals.deltas;
            if (rng() % 3 == 0) corrupt_trials(rep_a, cur_a, rep_b, cur_b, da, rng, where);
            const auto ra = apply(false, rep_a, da, cur_a);
            const auto rb = apply(true, rep_b, da, cur_b);
            need(bool(ra) && bool(rb), where + ": delta rejected: " + (ra ? std::string() : ra.error().message) +
                                           (rb ? std::string() : rb.error().message));
            same_replica(rep_a, cur_a, rep_b, cur_b, where);
            need(payload_of(rep_a) == payload_of(src_a), where + ": the replica is not the source");
        }
        if (rng() % 8 == 0) {
            // A copy baseline pairs in between: the touch log is shared.
            xo::XDeltaOptions touched;
            touched.touched_only = true;
            std::string ca, cb;
            const auto sca = on_path(false, [&] { return src_a.save_delta_into(ca, copy_a, touched); });
            const auto scb = on_path(true, [&] { return src_b.save_delta_into(cb, copy_b, touched); });
            same_stats(sca, scb, where + " (copy)");
            need(ca == cb, where + " (copy): delta bytes differ");
        }
        // Each replica relays what it applied (its touch log replays the
        // blocks each delta wrote).
        std::string ra, rb;
        const auto rsa = on_path(false, [&] { return rep_a.save_delta_into(ra, relay_a, options); });
        const auto rsb = on_path(true, [&] { return rep_b.save_delta_into(rb, relay_b, options); });
        same_stats(rsa, rsb, where + " (relay)");
        need(ra == rb, where + " (relay): delta bytes differ");
        if (!rsa.empty) {
            const auto sda = apply(false, down_a, ra, down_cur_a);
            const auto sdb = apply(true, down_b, rb, down_cur_b);
            need(bool(sda) && bool(sdb), where + " (relay): delta rejected");
            same_replica(down_a, down_cur_a, down_b, down_cur_b, where + " (relay)");
            need(payload_of(down_a) == payload_of(src_a), where + " (relay): downstream is not the source");
        }
    }
}

// Fixed cases: a scalar, a whole block, eight and nine blocks, an empty
// delta, a version past one varint byte.
void fixed_cases() {
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    auto& r = src.plain_root();
    std::mt19937_64 rng(5);
    for (int i = 0; i < 64; ++i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(i);
        it.name = random_text(rng, 30);
    }
    xo::XDeltaBaseline base(xo::XDeltaRecord::touches, 77);
    Buffer rep(4096, cap);
    xo::XDeltaCursor cur;
    std::string d;
    auto st = src.save_delta_into(d, base);
    need(st.key && bool(apply(false, rep, d, cur)), "fixed: key delta");
    const auto before = Access::test_small_counts()[0];
    // One scalar, the low byte.
    src.touch_set(r.epoch, r.epoch + 1);
    st = src.save_delta_into(d, base);
    need(Access::test_small_counts()[0] == before + 1 && st.blocks == 1 && st.changed_bytes == 1,
         "fixed: a scalar did not take the short path as one byte");
    need(bool(apply(false, rep, d, cur)) && payload_of(rep) == payload_of(src), "fixed: scalar applied");
    // An untouched buffer: an empty delta, no version.
    const auto version = base.version();
    st = src.save_delta_into(d, base);
    need(st.empty && d.empty() && base.version() == version, "fixed: an untouched buffer sent a delta");
    // Eight blocks: short; nine: general (the list holds them, more than 8).
    const auto items_block = [&](std::size_t i) {
        return (reinterpret_cast<const char*>(&r.items[std::uint32_t(i)].id) -
                static_cast<const char*>(src.get_address())) / 64;
    };
    for (std::size_t want : {std::size_t(8), std::size_t(9)}) {
        std::vector<std::size_t> touched;
        for (std::size_t i = 0; i < r.items.size() && touched.size() < want; ++i) {
            const auto block = items_block(i);
            if (std::find(touched.begin(), touched.end(), block) != touched.end()) continue;
            touched.push_back(block);
            src.touch_set(r.items[std::uint32_t(i)].id, r.items[std::uint32_t(i)].id + 1000);
        }
        need(touched.size() == want, "fixed: not enough blocks");
        const auto small = Access::test_small_counts()[0];
        st = src.save_delta_into(d, base);
        need(st.blocks == want && (Access::test_small_counts()[0] == small + 1) == (want <= 8),
             "fixed: " + std::to_string(want) + " blocks took the wrong path");
        const auto applied = Access::test_small_counts()[1];
        need(bool(apply(false, rep, d, cur)) && payload_of(rep) == payload_of(src), "fixed: blocks applied");
        need((Access::test_small_counts()[1] == applied + 1) == (want <= 8),
             "fixed: " + std::to_string(want) + " blocks applied on the wrong path");
    }
    // A touch of 56 bytes of a block, every word: whole (64 bytes); of 55:
    // the mask form. Both against the general path on a twin.
    {
        auto twin = src.clone();
        Buffer twin_rep(4096, cap);
        xo::XDeltaBaseline twin_base(xo::XDeltaRecord::touches, 99);
        xo::XDeltaCursor twin_cur;
        std::string td;
        need(bool(apply(true, twin_rep, twin.save_delta(twin_base), twin_cur)), "fixed: twin key delta");
        // The block section (past the header, whose stream and version differ).
        const auto body = [](const std::string& x) {
            std::size_t at = 12;
            while (static_cast<unsigned char>(x[at]) >= 0x80u) ++at;
            return x.substr(at + 1);
        };
        r.title = std::string(200, 'x');
        twin.plain_root().title = std::string(200, 'x');
        src.touch(r.title);
        twin.touch(twin.plain_root().title);
        need(bool(apply(false, rep, src.save_delta(base), cur)) &&
                 bool(apply(true, twin_rep, on_path(true, [&] { return twin.save_delta(twin_base); }), twin_cur)),
             "fixed: title");
        const char* text = r.title.c_str();
        const auto at = std::size_t(text - static_cast<const char*>(src.get_address()));
        const std::size_t block = (at + 63) / 64 * 64;  // a block inside the text
        for (std::size_t n : {std::size_t(57), std::size_t(56), std::size_t(55)}) {
            for (auto* b : {&src, &twin}) {
                char* base_at = const_cast<char*>(static_cast<const char*>(b->get_address())) + block;
                for (std::size_t k = 7; k < 7 + n; ++k) base_at[k] = char('a' + (k + n) % 26);
                b->touch(base_at + 7, n);  // bytes [7, 7 + n): every word
            }
            st = src.save_delta_into(d, base);
            const auto tst = on_path(true, [&] { return twin.save_delta_into(td, twin_base); });
            same_stats(st, tst, "fixed: " + std::to_string(n) + " bytes");
            need(st.changed_bytes == (n >= 56 ? 64u : n), "fixed: " + std::to_string(n) + " touched bytes in the wrong form");
            need(body(d) == body(td), "fixed: " + std::to_string(n) + " bytes: delta differs");
            need(bool(apply(false, rep, d, cur)) && payload_of(rep) == payload_of(src), "fixed: whole block applied");
            need(bool(apply(true, twin_rep, td, twin_cur)), "fixed: twin applied");
        }
    }
    // Versions past 127 and 16383 (two and three varint bytes).
    for (int i = 0; i < 17000; ++i) {
        src.touch_set(r.epoch, r.epoch + 1);
        const auto s1 = src.save_delta_into(d, base);
        need(!s1.empty, "fixed: a version step sent nothing");
        need(bool(apply(false, rep, d, cur)), "fixed: version " + std::to_string(base.version()) + " rejected");
    }
    need(base.version() > 16384 && payload_of(rep) == payload_of(src), "fixed: long stream");
}
// Hand-made deltas around the edges a receiver checks: one written byte
// at every position of the first, a middle and the last blocks (block 0
// starts in the arena header; the last ends past used), whole blocks there,
// non-canonical varints, several blocks out of order or repeated, and the
// most blocks the short path takes plus one. Both paths: the same outcome,
// error and result.
void crafted_cases() {
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    std::mt19937_64 rng(9);
    fill(src.plain_root(), rng);
    xo::XDeltaBaseline base(xo::XDeltaRecord::touches, 1234);
    Buffer rep(4096, cap);
    xo::XDeltaCursor cur;
    need(bool(apply(false, rep, src.save_delta(base), cur)), "crafted: key delta");
    const std::size_t used = payload_of(rep).size();
    const std::size_t last = (used - 1) / 64;
    std::size_t cases = 0;
    const auto header = [&](std::string& d, std::uint32_t count) {
        d.assign("XD", 2);
        d.push_back(char(0x10));
        d.push_back(char(0));
        const std::uint64_t stream = cur.stream();
        d.append(reinterpret_cast<const char*>(&stream), 8);
        std::uint32_t v = cur.version() + 1u;
        while (v >= 0x80u) { d.push_back(char((v & 0x7Fu) | 0x80u)); v >>= 7; }
        d.push_back(char(v));
        d.push_back(char(count));
    };
    const auto both = [&](const std::string& d, const std::string& what) {
        auto a = rep.clone();
        auto b = rep.clone();
        auto ca = cur;
        auto cb = cur;
        const auto sa = apply(false, a, d, ca);
        const auto sb = apply(true, b, d, cb);
        ++cases;
        need(bool(sa) == bool(sb), "crafted " + what + ": different outcomes");
        if (!sa) need(sa.error().code == sb.error().code && sa.error().message == sb.error().message,
                      "crafted " + what + ": different errors");
        same_replica(a, ca, b, cb, "crafted " + what);
    };
    for (std::size_t block : {std::size_t(0), std::size_t(1), last / 2, last - 1, last, last + 1}) {
        for (unsigned bit = 0; bit < 64; ++bit) {
            std::string d;
            header(d, 1);
            d.push_back(char(block));                    // skip (blocks here are below 128)
            d.push_back(char(1u << (bit / 8)));          // selector
            d.push_back(char(1u << (bit % 8)));          // mask byte
            d.push_back(char(0x5A));                     // the byte
            both(d, "block " + std::to_string(block) + " bit " + std::to_string(bit));
        }
        std::string whole;
        header(whole, 1);
        whole.push_back(char(block));
        whole.push_back(char(0));
        whole.append(64, char(0x33));
        both(whole, "whole block " + std::to_string(block));
        std::string word;  // a whole word, mask byte 0xFF
        header(word, 1);
        word.push_back(char(block));
        word.push_back(char(0x80));
        word.push_back(char(0xFF));
        word.append(8, char(0x44));
        both(word, "word of block " + std::to_string(block));
    }
    // A two-byte skip of a small value, a zero mask byte, blocks repeated
    // (skips are relative: a repeat is the next block), 8 and 9 blocks.
    {
        std::string d;
        header(d, 1);
        d.push_back(char(0x81));
        d.push_back(char(0x00));
        d.push_back(char(1));
        d.push_back(char(1));
        d.push_back(char(7));
        both(d, "two-byte skip");
        header(d, 1);
        d.push_back(char(2));
        d.push_back(char(1));
        d.push_back(char(0));
        d.push_back(char(7));
        both(d, "zero mask byte");
    }
    for (std::uint32_t count : {8u, 9u}) {
        std::string d;
        header(d, count);
        for (std::uint32_t k = 0; k < count; ++k) {
            d.push_back(char(k == 0 ? 1 : 0));
            d.push_back(char(1));
            d.push_back(char(1));
            d.push_back(char(k + 1));
        }
        both(d, std::to_string(count) + " blocks");
    }
    need(cases > 6 * 64, "crafted: too few cases");
}
} // namespace

int main() {
    crafted_cases();
    fixed_cases();
    for (std::uint64_t seed = 1; seed <= 40; ++seed) stream(seed, 300);
    const auto* counts = Access::test_small_counts();
    std::printf("test_delta_small: %zu steps, %zu deltas (%zu empty), short path wrote %zu and applied %zu; "
                "%zu corrupted (%zu accepted)\n",
                totals.steps, totals.deltas, totals.empties, counts[0], counts[1], totals.corrupt,
                totals.corrupt_accepted);
    need(counts[0] > totals.deltas / 4 && counts[1] > totals.deltas / 4, "the short paths ran too rarely");
    std::printf("test_delta_small: OK\n");
    return 0;
}
