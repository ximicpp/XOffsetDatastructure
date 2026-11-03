// Delta sync (xoffset_delta.hpp, docs/DELTA_SYNC.md).
//
// Random streams over the fuzz model (inline/heap strings, nested vectors,
// set, map, holes from erasure, growth, a sender reset that shrinks the
// payload, baseline resets): after every applied delta the replica holds the
// sender's payload byte for byte, the same hole set and the same contents,
// and the cursor follows the baseline. Corrupted deltas (flipped, truncated,
// extended) either reject, leaving the replica and cursor exactly as they
// were, or are accepted only as a payload that verifies (with a digest: only
// as the sender's exact result). Fixed cases: empty deltas, base mismatch,
// out-of-order and repeated deltas, wrong root type, a replica changed
// locally (caught by the digest), capacity limits, rejected key deltas, the
// xxHash64 digest against reference values, replicas fed on several threads
// at once (and one at thread exit), baselines that record touches instead
// of a copy (XDeltaRecord::touches), and a viewport-like model whose
// delta sizes are reported. With exceptions on, the throwing entry must
// report exactly what the try_ entry reports. No try/catch of its own: also
// built as test_delta_sync_noexc.
#include "fuzz/v2_fuzz_core.hpp"

#include <functional>
#include <thread>

namespace dz {
struct Unit {
    std::uint32_t id{};
    std::uint32_t owner{};
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t hp{};
    std::uint32_t state{}; // 0: empty slot
    xo::XString name;
};
struct View {
    std::uint32_t tick{};
    std::uint32_t live{};
    xo::XVector<Unit> slots; // stable slots: a unit keeps its slot while it is in view
};
} // namespace dz
XOFFSET_RECORD(dz::Unit, id, owner, x, y, hp, state, name)
XOFFSET_RECORD(dz::View, tick, live, slots)
XOFFSET_REGISTER_SCHEMA_NAME(dz::View, "test.delta.view")

namespace {
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

std::string payload_of(const xo::XBuffer& b) {
    const auto* sm = b.arena();
    return std::string(static_cast<const char*>(b.get_address()), static_cast<std::size_t>(sm->used_end));
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> holes_of(const xo::XBuffer& b) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    b.for_each_free_block([&](xo::XBufferCore::WireHole h) { out.emplace_back(h.offset, h.bytes); });
    std::sort(out.begin(), out.end());
    return out;
}

constexpr xo::XCompactOptions kTight{.slack_div = 0};

struct Totals {
    std::size_t deltas = 0, keys = 0, empties = 0, digests = 0, shrinks = 0;
    std::size_t corrupt_rejected = 0, corrupt_accepted = 0, late_joins = 0;
    std::size_t checks[4] = {};  // by XDeltaCheck: key, none, in_place, full
    std::uint64_t delta_bytes = 0, full_bytes = 0;
} totals;

// try_apply_delta_verified, and (exceptions on) the throwing entry on a copy
// of the same replica and cursor, which must agree on outcome, code, message
// and result.
template <class Root>
xo::XStatus apply_entry(xo::TypedXBuffer<Root>& rep, const std::string& d, xo::XDeltaCursor& cur) {
#if !XOFFSET_NO_EXCEPTIONS
    std::optional<xo::TypedXBuffer<Root>> twin;
    if (rep.arena() && rep.has_root()) twin.emplace(rep.clone());
    else twin.emplace(xo::TypedXBuffer<Root>(4096, xo::XBuffer::maximum_capacity(rep.maximum_capacity()
                                                                                 ? rep.maximum_capacity()
                                                                                 : kLimit)));
    auto twin_cursor = cur;
    std::optional<xo::XError> thrown;
    try {
        twin->apply_delta_verified(d, twin_cursor);
    } catch (const xo::XException& e) {
        thrown = xo::XError{e.code(), e.what()};
    }
#endif
    auto r = rep.try_apply_delta_verified(d, cur);
    if (!r) need(r.code() != xo::XErrc::ok && !r.error().message.empty(), "rejected without a code and message");
#if !XOFFSET_NO_EXCEPTIONS
    need(bool(thrown) != bool(r), std::string("try_apply_delta_verified ") + (r ? "accepted" : "rejected (" +
         r.error().message + ")") + " where apply_delta_verified " + (thrown ? "threw (" + thrown->message + ")"
                                                                           : "accepted"));
    if (thrown) {
        need(thrown->code == r.code() && thrown->message == r.error().message,
             "try_ and throwing entries report different errors: " + r.error().message + " / " + thrown->message);
    } else if (rep.arena() && rep.has_root()) {
        need(payload_of(*twin) == payload_of(rep), "throwing entry produced a different payload");
        need(twin_cursor.version() == cur.version(), "throwing entry moved the cursor differently");
    }
#endif
    return r;
}

void same_state(fz::Root& src_root, Buffer& src, Buffer& rep, const std::string& where) {
    need(payload_of(src) == payload_of(rep), where + ": replica payload differs from the sender's");
    need(holes_of(src) == holes_of(rep), where + ": replica hole set differs from the sender's");
    need(dump(src_root) == dump(rep.root()), where + ": replica contents differ");
    need(rep.save_compact(kTight) == src.save_compact(kTight), where + ": replica compact wire differs");
}

// Corrupted copies of `d` applied to the replica with a copy of its cursor,
// so that they take the same check path as `d` would (the throwing entry of
// apply_entry runs on a clone, which is verified in full: both must agree).
// An accepted one must verify; the replica is then restored from a clone,
// which the cursor no longer matches (its next delta is verified in full).
void corrupt_trials(Buffer& rep, const xo::XDeltaCursor& cur, const std::string& d, bool digest,
                    const std::string& expected_payload, std::mt19937_64& rng, const std::string& where) {
    const auto before = payload_of(rep);
    const auto before_holes = holes_of(rep);
    const auto before_dump = dump(rep.root());
    for (int k = 0; k < 6; ++k) {
        std::string bad = d;
        switch (rng() % 6) {
        case 0: bad[rng() % bad.size()] ^= char(1u << (rng() % 8)); break;
        case 1: bad.resize(rng() % bad.size()); break;
        case 2: bad.push_back(char(rng())); break;
        case 3: bad[rng() % std::min<std::size_t>(bad.size(), 56)] ^= char(1u << (rng() % 8)); break;
        case 4: bad[bad.size() - 1 - rng() % std::max<std::size_t>(1, bad.size() / 4)] ^= char(1 + rng() % 255); break;
        case 5: { // a byte inside the block section
            const std::size_t from = std::min<std::size_t>(bad.size() - 1, 14 + (digest ? 8 : 0));
            bad[from + rng() % (bad.size() - from)] ^= char(1 + rng() % 255);
        } break;
        }
        if (bad == d) continue;
        auto saved = rep.clone();
        auto tc = cur;
        auto r = apply_entry(rep, bad, tc);
        if (!r) {
            need(payload_of(rep) == before && holes_of(rep) == before_holes,
                 where + ": a rejected delta changed the replica (" + r.error().message + ")");
            need(tc.version() == cur.version() && tc.stream() == cur.stream(), where + ": a rejected delta moved the cursor");
            need(dump(rep.root()) == before_dump, where + ": a rejected delta changed the contents");
            ++totals.corrupt_rejected;
        } else {
            auto again = typed_load(rep.save_verified());
            need(bool(again), where + ": an accepted corrupted delta left a payload that does not verify");
            if (digest) need(payload_of(rep) == expected_payload, where + ": a digest delta accepted another result");
            ++totals.corrupt_accepted;
            rep = std::move(saved);
        }
    }
}

void fill(fz::Root& r, std::mt19937_64& rng) {
    r.epoch = rng();
    r.title = random_text(rng, 30);
    for (int i = int(rng() % 9); i > 0; --i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(rng());
        it.flag = rng() & 1;
        it.name = random_text(rng, 30);
        for (int j = int(rng() % 7); j > 0; --j) it.vals.push_back(std::uint16_t(rng()));
    }
    for (int i = int(rng() % 5); i > 0; --i) r.tags.emplace_back(random_text(rng, 20), r.tags.get_stored_allocator());
    for (int i = int(rng() % 6); i > 0; --i) r.keys.insert(std::uint32_t(rng() % 1000));
}

// Changes that keep the structure (scalars, bools, vector elements, map
// values, inline strings that stay inline): deltas the receiver applies
// unchecked or with in-place checks.
void mutate_light(fz::Root& r, std::mt19937_64& rng) {
    for (int n = 1 + int(rng() % 4); n > 0; --n) {
        switch (rng() % 6) {
        case 0: r.epoch = rng(); break;
        case 1: if (!r.items.empty()) r.items[rng() % r.items.size()].id = std::uint32_t(rng()); break;
        case 2: if (!r.items.empty()) { auto& it = r.items[rng() % r.items.size()]; it.flag = !it.flag; } break;
        case 3:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (!it.vals.empty()) it.vals[rng() % it.vals.size()] = std::uint16_t(rng());
            }
            break;
        case 4:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (it.name.size() <= 10) it.name = random_text(rng, 10);
            }
            break;
        case 5: if (!r.attrs.empty()) r.attrs.begin()->second = rng(); break;
        }
        // Sizes within capacity: containers checked on their own (a set
        // insert or a vector push that grows the block is verified in full).
        if (rng() % 3 == 0) {
            if (rng() % 2) r.keys.insert(std::uint32_t(rng() % 1000));
            else if (!r.keys.empty()) r.keys.erase(*(r.keys.begin() + std::ptrdiff_t(rng() % r.keys.size())));
        }
        if (rng() % 3 == 0 && !r.items.empty()) {
            auto& it = r.items[rng() % r.items.size()];
            if (rng() % 2 || it.vals.empty()) it.vals.push_back(std::uint16_t(rng()));
            else it.vals.pop_back();
        }
    }
}

void random_stream(std::uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    auto src = Buffer::create(4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    fill(src.root(), rng);
    xo::XDeltaBaseline base(seed * 2 + 1);
    Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit)); // an empty arena: only a key delta applies
    xo::XDeltaCursor cur;
    // A second receiver of the same deltas that joins late: it starts from
    // the baseline's key_delta (the recorded state, same stream and version).
    Buffer late(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor late_cur;
    bool joined = false;
    for (int step = 0; step < steps; ++step) {
        const std::string where = "seed " + std::to_string(seed) + " step " + std::to_string(step);
        const auto op = rng() % 20;
        if (op == 0) {
            base.reset(); // the receiver asked for a resync: a key delta onto its current content
        } else if (op == 1) {
            src.reset(); // the payload shrinks
            fill(src.root(), rng);
            ++totals.shrinks;
        } else if (op < 9 && op > 2) {
            mutate_light(src.root(), rng);
        } else if (op != 2) { // op 2: nothing changes
            mutate_valid(src.root(), rng);
            if (rng() % 4 == 0) src.root().epoch ^= std::uint64_t(1) << (rng() % 64); // one bit
        }
        xo::XDeltaOptions options;
        options.digest = rng() % 2;
        options.whole_blocks = rng() % 4 == 0;
        std::string d = "stale bytes from an earlier delta";
        const auto stats = src.save_delta_into(d, base, options);
        need(stats.delta_bytes == d.size(), where + ": stats disagree with the delta size");
        // Whole blocks: every block but block 0 (it starts in the arena
        // header) and one ending past used_end carries 64 bytes.
        need(!options.whole_blocks || stats.blocks <= 1 || stats.changed_bytes > (stats.blocks - 2u) * 64u,
             where + ": whole_blocks sent masked blocks");
        if (stats.empty) {
            need(d.empty() && !stats.key, where + ": an empty delta wrote bytes");
            need(payload_of(src) == payload_of(rep), where + ": empty delta although the payload changed");
            ++totals.empties;
            continue;
        }
        need(stats.key == (op == 0 || step == 0), where + ": unexpected key delta");
        ++totals.deltas;
        totals.keys += stats.key;
        totals.digests += options.digest;
        totals.delta_bytes += d.size();
        totals.full_bytes += src.save_verified().size();
        if (!stats.key && rng() % 2 == 0) corrupt_trials(rep, cur, d, options.digest, payload_of(src), rng, where);
        auto r = apply_entry(rep, d, cur);
        need(bool(r), where + ": delta rejected: " + (r ? std::string() : r.error().message));
        need((cur.last_check() == xo::XDeltaCheck::key) == stats.key, where + ": key delta not reported as key");
        ++totals.checks[std::size_t(cur.last_check())];
        same_state(src.root(), src, rep, where);
        need(cur.version() == base.version() && cur.stream() == base.stream(), where + ": cursor does not follow");
        if (joined) {
            auto lr = apply_entry(late, d, late_cur);
            need(bool(lr), where + ": late receiver rejected a shared delta: " + (lr ? std::string() : lr.error().message));
            same_state(src.root(), src, late, where + " (late receiver)");
        } else if (rng() % 5 == 0) {
            const auto version = base.version();
            std::string k = "stale";
            base.key_delta_into(k, options);
            need(base.version() == version && !base.empty(), where + ": key_delta_into advanced the baseline");
            need(k == base.key_delta(options), where + ": key_delta and key_delta_into differ");
            auto lr = apply_entry(late, k, late_cur);
            need(bool(lr), where + ": late key delta rejected: " + (lr ? std::string() : lr.error().message));
            same_state(src.root(), src, late, where + " (late join)");
            need(late_cur.version() == base.version() && late_cur.stream() == base.stream(),
                 where + ": late cursor does not follow the baseline");
            ++totals.late_joins;
            joined = true;
        }
        // The same delta again no longer applies, and changes nothing.
        if (rng() % 8 == 0) {
            const auto held = payload_of(rep);
            auto again = apply_entry(rep, d, cur);
            if (!stats.key) {
                need(!again && again.code() == xo::XErrc::delta_base_mismatch, where + ": a repeated delta applied");
                need(payload_of(rep) == held, where + ": a repeated delta changed the replica");
            } else {
                need(bool(again) && payload_of(rep) == held, where + ": a repeated key delta failed");
            }
        }
    }
}

std::string varint(std::uint32_t v) {
    std::string out;
    for (; v >= 0x80; v >>= 7) out += char(0x80 | (v & 0x7F));
    return out + char(v);
}

// A delta header (format 1.0) without EXTENT: used_end and root unchanged.
std::string delta_header(std::uint64_t stream, std::uint32_t version, std::uint32_t blocks, std::uint32_t flags = 0) {
    std::string h = {'X', 'D', char(0x10), char(flags)};
    h.append(reinterpret_cast<const char*>(&stream), 8);
    return h + varint(version) + varint(blocks);
}

std::string delta_of(Buffer& src, xo::XDeltaBaseline& base, xo::XDeltaOptions options = {}) {
    std::string d;
    (void)src.save_delta_into(d, base, options);
    return d;
}

void fixed_cases() {
    // xxHash64, seed 0 (reference values of the xxHash library).
    {
        const std::string p768 = [] { std::string s; for (int r = 0; r < 3; ++r) for (int i = 0; i < 256; ++i) s += char(i); return s; }();
        const struct { std::string in; std::uint64_t want; } vec[] = {
            {"", 0xef46db3751d8e999ull}, {"a", 0xd24ec4f1a98c6e5bull}, {"abc", 0x44bc2cf5ad770999ull},
            {p768, 0x8e03c838c596036full}, {std::string(31, 'x'), 0x60dd0d01083b99f0ull},
            {std::string(32, 'y'), 0x64da1f1d3495ce66ull}, {std::string(33, 'z') + "tail1234567", 0x8f6a8b73f8da89fcull}};
        for (const auto& v : vec)
            need(xo::detail::xxh64(v.in.data(), v.in.size()) == v.want, "xxh64 differs from the reference");
    }
    std::mt19937_64 rng(7);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);

    // A key delta into a default-constructed buffer (storage of the default
    // maximum, as load_verified would), then empty and incremental deltas.
    {
        auto src = Buffer::create(4096, cap);
        fill(src.root(), rng);
        xo::XDeltaBaseline base;
        Buffer rep;
        xo::XDeltaCursor cur;
        need(cur.empty() && base.empty(), "fresh cursor or baseline not empty");
        const auto key = delta_of(src, base);
        need(!key.empty() && base.version() == 1 && base.stream() != 0, "first delta is not a key delta");
        need(bool(apply_entry(rep, key, cur)), "key delta into a default buffer rejected");
        same_state(src.root(), src, rep, "default buffer");
        std::string d = "x";
        const auto stats = src.save_delta_into(d, base);
        need(stats.empty && d.empty() && base.version() == 1, "unchanged buffer gave a delta");
        src.root().epoch += 1;
        const auto small = delta_of(src, base);
        need(small.size() <= 20, "one changed scalar gave a large delta: " +
             std::to_string(small.size()));
        need(bool(apply_entry(rep, small, cur)), "scalar delta rejected");
        same_state(src.root(), src, rep, "scalar");
    }

    // Base mismatch: out of order, repeated, on an empty cursor, another stream.
    {
        auto src = Buffer::create(4096, cap);
        fill(src.root(), rng);
        xo::XDeltaBaseline base;
        Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
        xo::XDeltaCursor cur;
        need(bool(apply_entry(rep, delta_of(src, base), cur)), "key rejected");
        src.root().title = "first";
        const auto d1 = delta_of(src, base);
        src.root().title = "second, long enough for a heap block";
        const auto d2 = delta_of(src, base);
        const auto held = payload_of(rep);
        auto r = apply_entry(rep, d2, cur);
        need(!r && r.code() == xo::XErrc::delta_base_mismatch && payload_of(rep) == held, "out-of-order delta applied");
        need(bool(apply_entry(rep, d1, cur)), "d1 rejected");
        r = apply_entry(rep, d1, cur);
        need(!r && r.code() == xo::XErrc::delta_base_mismatch, "repeated delta applied");
        need(bool(apply_entry(rep, d2, cur)), "d2 rejected");
        same_state(src.root(), src, rep, "in order");

        Buffer other(4096, xo::XBuffer::maximum_capacity(kLimit));
        xo::XDeltaCursor empty;
        r = apply_entry(other, d1, empty);
        need(!r && r.code() == xo::XErrc::delta_base_mismatch && empty.empty(), "non-key delta on an empty cursor");

        xo::XDeltaBaseline second; // another stream of the same buffer
        (void)delta_of(src, second);
        src.root().epoch ^= 5;
        const auto foreign = delta_of(src, second);
        r = apply_entry(rep, foreign, cur);
        need(!r && r.code() == xo::XErrc::delta_base_mismatch, "delta of another stream applied");
    }

    // A replica changed locally: a digest delta rejects and rolls back.
    {
        auto src = Buffer::create(4096, cap);
        fill(src.root(), rng);
        xo::XDeltaBaseline base;
        Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
        xo::XDeltaCursor cur;
        need(bool(apply_entry(rep, delta_of(src, base), cur)), "key rejected");
        rep.root().epoch ^= 0xFF; // not allowed by the contract: replicas are read-only
        const auto changed = payload_of(rep);
        src.root().title = "after the local change";
        xo::XDeltaOptions with_digest;
        with_digest.digest = true;
        const auto d = delta_of(src, base, with_digest);
        auto r = apply_entry(rep, d, cur);
        need(!r && r.code() == xo::XErrc::digest_mismatch, "digest did not catch a locally changed replica");
        need(payload_of(rep) == changed && cur.version() == 1, "digest rejection did not roll back");
        // Resync: a key delta onto the replica's current content.
        base.reset();
        need(bool(apply_entry(rep, delta_of(src, base, with_digest), cur)), "resync key delta rejected");
        same_state(src.root(), src, rep, "resync");
    }

    // The replica's maximum capacity: growth past it rejects, nothing changes.
    {
        auto src = Buffer::create(4096, cap);
        fill(src.root(), rng);
        xo::XDeltaBaseline base;
        Buffer rep(4096, xo::XBuffer::maximum_capacity(64 * 1024));
        xo::XDeltaCursor cur;
        need(bool(apply_entry(rep, delta_of(src, base), cur)), "key rejected");
        auto& big = src.root().items.emplace_back();
        for (int i = 0; i < 40000; ++i) big.vals.push_back(std::uint16_t(i));
        const auto held = payload_of(rep);
        auto r = apply_entry(rep, delta_of(src, base), cur);
        need(!r && r.code() == xo::XErrc::capacity_exceeded, "growth past the maximum capacity applied");
        need(payload_of(rep) == held && cur.version() == 1, "capacity rejection changed the replica");
    }

    // A key delta rejected after its header: the buffer is left empty, the
    // cursor cleared; a good key delta then applies.
    {
        auto src = Buffer::create(4096, cap);
        fill(src.root(), rng);
        src.root().items.emplace_back().name = "a heap string that is long enough";
        xo::XDeltaBaseline base;
        Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
        xo::XDeltaCursor cur;
        auto key = delta_of(src, base);
        need(bool(apply_entry(rep, key, cur)), "key rejected");
        base.reset();
        src.root().epoch += 3;
        key = delta_of(src, base);
        // The root offset (the third header varint) one word further: the
        // header checks pass, verification of the patched payload rejects.
        std::size_t at = 12;
        const auto skip_varint = [&] { while (std::uint8_t(key[at]) & 0x80) ++at; ++at; };
        skip_varint();  // version
        skip_varint();  // used_end
        const std::size_t root_at = at;
        std::uint32_t root = 0;
        for (int shift = 0;; shift += 7) {
            root |= std::uint32_t(std::uint8_t(key[at]) & 0x7F) << shift;
            if (!(std::uint8_t(key[at++]) & 0x80)) break;
        }
        const std::string bad = key.substr(0, root_at) + varint(root + 8) + key.substr(at);
        auto r = apply_entry(rep, bad, cur);
        need(!r && cur.empty() && !rep.has_root(), "rejected key delta did not leave the buffer empty");
        need(bool(apply_entry(rep, key, cur)), "good key delta after a rejected one");
        same_state(src.root(), src, rep, "key after reject");
    }
}

// Viewport-like model: a client's view of up to 128 units in stable slots;
// each tick some move, some lose hp, two leave and two enter. Reports delta
// sizes against the full wires.
void view_sizes() {
    using ViewBuffer = xo::TypedXBuffer<dz::View>;
    std::mt19937_64 rng(42);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto server = ViewBuffer::create(4096, cap);
    auto& v = server.root();
    v.slots.resize(128);
    std::uint32_t next_id = 1;
    auto enter = [&](dz::Unit& u) {
        u.id = next_id++;
        u.owner = std::uint32_t(rng() % 8);
        u.x = std::int32_t(rng() % 512);
        u.y = std::int32_t(rng() % 384);
        u.hp = 100;
        u.state = 1;
        u.name = "unit-" + std::to_string(u.id) + (rng() % 2 ? "-veteran" : "");
    };
    for (std::size_t i = 0; i < 120; ++i) enter(v.slots[i]);
    v.live = 120;
    xo::XDeltaBaseline base;
    ViewBuffer client(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor cur;
    std::string d;
    (void)server.save_delta_into(d, base);
    need(bool(client.try_apply_delta_verified(d, cur)), "view key delta rejected");
    const std::size_t key_bytes = d.size();
    std::uint64_t delta_sum = 0, full_sum = 0, compact_sum = 0, blocks = 0;
    const int ticks = 200;
    for (int t = 0; t < ticks; ++t) {
        ++v.tick;
        for (auto& u : v.slots) {
            if (u.state == 0) continue;
            if (rng() % 10 < 3) { u.x += std::int32_t(rng() % 7) - 3; u.y += std::int32_t(rng() % 7) - 3; }
            if (rng() % 20 == 0) u.hp -= std::int32_t(1 + rng() % 10);
        }
        for (int k = 0; k < 2; ++k) { // two leave, two enter
            auto& gone = v.slots[rng() % v.slots.size()];
            if (gone.state != 0) { gone = dz::Unit{}; --v.live; }
        }
        for (auto& u : v.slots) {
            if (v.live >= 120) break;
            if (u.state == 0) { enter(u); ++v.live; }
        }
        const auto stats = server.save_delta_into(d, base);
        delta_sum += d.size();
        blocks += stats.blocks;
        full_sum += server.save_verified().size();
        compact_sum += server.save_compact(kTight).size();
        need(bool(client.try_apply_delta_verified(d, cur)), "view delta rejected at tick " + std::to_string(t));
        need(payload_of(client) == payload_of(server), "view replica differs at tick " + std::to_string(t));
    }
    std::cout << "view (120 units, 30% move, 5% hp, 2 leave + 2 enter per tick): key delta " << key_bytes
              << " B; per tick: delta " << delta_sum / ticks << " B in " << blocks / ticks << " blocks, save_verified "
              << full_sum / ticks << " B, save_compact " << compact_sum / ticks << " B\n";
    need(delta_sum * 4 < compact_sum, "view deltas are not much smaller than compact wires");
}

// The block scan against a byte-by-byte reference: the blocks (offset, mask,
// whole) holding bytes of [from, n) and the updated copy, and both
// block-mask versions.
void diff_reference_case() {
    std::mt19937_64 rng(20261102);
    xo::detail::DeltaBlockBuffer blocks;
    struct Want { std::size_t offset; std::uint64_t mask; bool whole; };
    std::vector<Want> want;
    for (int trial = 0; trial < 3000; ++trial) {
        const std::size_t n = 48 + 8 * std::size_t(rng() % 90);
        const std::size_t from = std::min<std::size_t>(n, rng() % 2 ? 48 : 8 * (rng() % 10));
        std::vector<char> cur(n), base(n);
        for (auto& c : cur) c = char(rng());
        base = cur;
        // Changes in clusters, often next to 64-byte block edges.
        const int clusters = int(rng() % 12);
        for (int c = 0; c < clusters; ++c) {
            std::size_t at = rng() % 2 ? from + (rng() % 12) * 64 + (rng() % 7) - 3 : rng() % n;
            const std::size_t len = 1 + rng() % 9;
            for (std::size_t k = 0; k < len; ++k, at += 1 + (rng() % 4 == 0 ? rng() % 5 : 0))
                if (at < n) base[at] = char(base[at] ^ char(1 + rng() % 255));
        }
        if (trial % 50 == 0) for (auto& c : base) c = char(rng());   // everything differs
        if (trial % 50 == 1) for (std::size_t i = from; i + 1 < n; i += 2) base[i] = char(~cur[i]); // every other byte
        want.clear();
        // Blocks at multiples of 64; bytes below from are never in a mask.
        for (std::size_t start = from / 64 * 64; start < n; start += 64) {
            std::uint64_t mask = 0;
            for (std::size_t k = 0; k < 64 && start + k < n; ++k)
                if (start + k >= from && cur[start + k] != base[start + k]) mask |= std::uint64_t(1) << k;
            if (mask == 0) continue;
            int mask_bytes = 0;
            for (int k = 0; k < 8; ++k) mask_bytes += ((mask >> (8 * k)) & 0xFF) != 0;
            const bool whole = start >= from && start + 64 <= n && mask_bytes + std::popcount(mask) >= 64;
            want.push_back({start, mask, whole});
        }
        std::vector<char> copy = base;
        (void)xo::detail::delta_diff_update(cur.data(), copy.data(), from, n, blocks);
        need(blocks.size() == want.size(), "diff: block count differs from the reference");
        for (std::size_t r = 0; r < blocks.size(); ++r)
            need(blocks[r].offset == want[r].offset && blocks[r].mask == want[r].mask &&
                     bool(blocks[r].whole) == want[r].whole,
                 "diff: block differs from the reference");
        need(std::equal(copy.begin() + std::ptrdiff_t(from), copy.end(), cur.begin() + std::ptrdiff_t(from)) &&
                 std::equal(copy.begin(), copy.begin() + std::ptrdiff_t(from), base.begin()),
             "diff: the recorded copy was not brought up to date");
        // Masked writes and reads round-trip, in every version (plain,
        // shuffle, the dispatching one; with and without room to read past
        // the bytes), and leave the other bytes as they were.
        for (const auto& blk : blocks) {
            if (blk.whole || blk.offset + 64 > n) continue;
            const char* src = cur.data() + blk.offset;
            const auto count = std::size_t(std::popcount(blk.mask));
            char packed[3][64 + 8];
            const char* ends[3] = {
                xo::detail::delta_put_masked_plain(packed[0], src, blk.mask),
                xo::detail::delta_put_masked(packed[1], src, blk.mask),
                xo::detail::delta_shuffle_supported() ? xo::detail::delta_put_masked_shuffle(packed[2], src, blk.mask)
                                                      : xo::detail::delta_put_masked_plain(packed[2], src, blk.mask)};
            for (int v = 0; v < 3; ++v) {
                need(ends[v] == packed[v] + count, "diff: masked write has the wrong length");
                need(std::equal(packed[v], packed[v] + count, packed[0]), "diff: masked write versions differ");
            }
            for (int v = 0; v < 4; ++v) {
                char out[64];
                for (auto& c : out) c = static_cast<char>(rng());
                char before[64];
                std::memcpy(before, out, 64);
                const char* got = nullptr;
                if (v == 0) got = xo::detail::delta_get_masked_plain(out, packed[0], blk.mask);
                else if (v == 1) got = xo::detail::delta_get_masked(out, packed[0], blk.mask, packed[0] + count);
                else if (v == 2) got = xo::detail::delta_get_masked(out, packed[0], blk.mask, packed[0] + sizeof packed[0]);
                else got = xo::detail::delta_shuffle_supported()
                               ? xo::detail::delta_get_masked_shuffle(out, packed[0], blk.mask)
                               : xo::detail::delta_get_masked_plain(out, packed[0], blk.mask);
                need(got == packed[0] + count, "diff: masked read has the wrong length");
                for (std::size_t k = 0; k < 64; ++k)
                    need(out[k] == (blk.mask >> k & 1 ? src[k] : before[k]), "diff: masked round trip differs");
            }
        }
        // Both block-mask versions against the reference, at every offset.
        for (std::size_t i = 0; i + 64 <= n; i += 1 + rng() % 9) {
            std::uint64_t ref = 0;
            for (std::size_t k = 0; k < 64; ++k) if (cur[i + k] != base[i + k]) ref |= std::uint64_t(1) << k;
            need(xo::detail::delta_block_mask(cur.data() + i, base.data() + i) == ref &&
                     xo::detail::delta_block_mask_words(cur.data() + i, base.data() + i) == ref,
                 "diff: block mask differs from the reference");
        }
    }
}

// Hand-made block sections that the parser must reject, before a byte of the
// replica changes.
void block_section_case() {
    std::mt19937_64 rng(11);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    fill(src.root(), rng);
    xo::XDeltaBaseline base;
    Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor cur;
    need(bool(apply_entry(rep, delta_of(src, base), cur)), "block section: key delta rejected");
    src.root().epoch += 1;
    const std::string good = delta_of(src, base);
    const std::size_t used = std::size_t(rep.arena()->used_end);
    const std::size_t last = (used - 1) / 64; // index of the last block (block k: bytes [64k, 64k + 64))
    const auto make = [&](std::uint32_t count, std::string section) {
        return delta_header(cur.stream(), cur.version() + 1, count) + section;
    };
    const std::size_t header_size = make(1, "").size();
    need(good.compare(0, header_size, make(1, "")) == 0, "block section: the scalar delta is not one plain block");
    struct Case { const char* what; std::string delta; xo::XErrc code; };
    std::vector<Case> cases = {
        {"a zero mask byte", make(1, std::string("\x01\x03\x01\0", 4) + "a"), xo::XErrc::invalid_extent_table},
        {"a block at used", make(1, varint(std::uint32_t((used + 63) / 64)) + "\x01\x01" + "a"),
         xo::XErrc::invalid_extent_table},
        {"a block far past used", make(1, varint(0x0FFFFFFF) + "\x01\x01" + "a"), xo::XErrc::invalid_extent_table},
        {"a second block past used", make(2, std::string("\x01\x01\x01", 3) + "a" + varint(std::uint32_t(last)) +
                                                 "\x01\x01" + "b"),
         xo::XErrc::invalid_extent_table},
        {"a truncated entry", make(1, std::string("\x01\x01", 2)), xo::XErrc::size_mismatch},
        {"missing changed bytes", make(1, std::string("\x01\x01\x03", 3) + "a"), xo::XErrc::size_mismatch},
        {"a missing mask byte", make(1, std::string("\x01\x03\x01", 3)), xo::XErrc::size_mismatch},
        {"a block count past the section", make(100, std::string("\x01\x01\x01", 3) + "a"), xo::XErrc::size_mismatch},
        {"trailing bytes", make(1, std::string("\x01\x01\x01", 3) + "ab"), xo::XErrc::size_mismatch},
        // Block 0 starts in the arena header [0, 48), which no delta writes.
        {"block 0 whole", make(1, std::string(2, '\0') + std::string(64, 'w')), xo::XErrc::invalid_extent_table},
        {"a mask bit in the header", make(1, std::string("\0\x01\x01", 3) + "a"), xo::XErrc::invalid_extent_table},
        {"a mask bit just below 48", make(1, std::string("\0\x60\x80\x01", 4) + "ab"),
         xo::XErrc::invalid_extent_table},
    };
    // Header cases.
    const std::string body = good.substr(header_size);
    cases.push_back({"a bad magic", "XE" + good.substr(2), xo::XErrc::bad_magic});
    cases.push_back({"format 2.0", good.substr(0, 2) + char(0x20) + good.substr(3), xo::XErrc::unsupported_version});
    cases.push_back({"format 1.1", good.substr(0, 2) + char(0x11) + good.substr(3), xo::XErrc::unsupported_version});
    cases.push_back({"an unknown flag", delta_header(cur.stream(), cur.version() + 1, 1, 0x10) + body,
                     xo::XErrc::unsupported_flags});
    cases.push_back({"a header cut before its varints", good.substr(0, 12), xo::XErrc::truncated});
    cases.push_back({"a header cut in a varint", good.substr(0, 12) + char(0x81), xo::XErrc::truncated});
    cases.push_back({"a key delta without extent",
                     delta_header(cur.stream(), cur.version() + 1, 1, xo::XDELTA_FLAG_KEY | xo::XDELTA_FLAG_HOLES) +
                         std::string(16, '\0') + body,
                     xo::XErrc::unsupported_flags});
    cases.push_back({"a stream of 0", delta_header(0, cur.version() + 1, 1) + body, xo::XErrc::delta_base_mismatch});
    cases.push_back({"another stream", delta_header(cur.stream() ^ 1, cur.version() + 1, 1) + body,
                     xo::XErrc::delta_base_mismatch});
    if (used % 64 != 0) {
        const std::size_t tail = used % 64; // bytes of the last, shorter block
        cases.push_back({"a whole block past used", make(1, varint(std::uint32_t(last)) + std::string(1, '\0') +
                                                                std::string(64, 'w')),
                         xo::XErrc::invalid_extent_table});
        std::string mask(8, '\0');
        mask[tail / 8] = char(1u << (tail % 8));  // the first byte at used
        std::string sel(1, char(1u << (tail / 8)));
        cases.push_back({"a mask bit at used", make(1, varint(std::uint32_t(last)) + sel + mask.substr(tail / 8, 1) + "a"),
                         xo::XErrc::invalid_extent_table});
    }
    // The good delta, rebuilt from parts, applies (the cases differ from it
    // only where they say).
    need(make(1, good.substr(header_size)) == good, "block section: rebuilt delta differs");
    const auto before = payload_of(rep);
    for (const auto& c : cases) {
        auto trial = rep.clone();
        auto tc = cur;
        auto r = apply_entry(trial, c.delta, tc);
        need(!r, std::string("block section: accepted ") + c.what);
        need(r.code() == c.code, std::string("block section: wrong code for ") + c.what + ": " + r.error().message);
        need(payload_of(trial) == before && tc.version() == cur.version(),
             std::string("block section: rejecting ") + c.what + " changed the replica");
    }
    need(bool(apply_entry(rep, good, cur)), "block section: the good delta was rejected");
    same_state(src.root(), src, rep, "block section");
}

// A delta with one block that writes `bytes` at payload offset `at`, as the
// next delta of the cursor's stream onto the replica's current extent.
std::string poke_delta(const xo::XBuffer& rep, const xo::XDeltaCursor& cur, std::size_t at, const std::string& bytes) {
    const std::size_t i = at;  // block k: payload bytes [64k, 64k + 64)
    need(i % 64 + bytes.size() <= 64, "poke: bytes cross a block");
    std::uint64_t mask = 0;
    for (std::size_t k = 0; k < bytes.size(); ++k) mask |= std::uint64_t(1) << (i % 64 + k);
    std::string section = varint(std::uint32_t(i / 64));
    unsigned sel = 0;
    for (int w = 0; w < 8; ++w) if ((mask >> (8 * w)) & 0xFF) sel |= 1u << w;
    section += char(sel);
    for (int w = 0; w < 8; ++w) if ((mask >> (8 * w)) & 0xFF) section += char(mask >> (8 * w));
    section += bytes;
    (void)rep;
    return delta_header(cur.stream(), cur.version() + 1, 1) + section;
}

// Which check each kind of change takes, and the in-place checks rejecting
// what verification rejects.
void check_path_case() {
    std::mt19937_64 rng(5);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    auto& r = src.root();
    r.title = "short";
    for (int i = 0; i < 3; ++i) {
        auto& it = r.items.emplace_back();
        it.id = std::uint32_t(i + 1);
        it.name = "n" + std::to_string(i);
        for (int j = 0; j < 4; ++j) it.vals.push_back(std::uint16_t(j));
    }
    r.items.reserve(8);
    r.items[0].vals.reserve(16);
    r.keys.reserve(8);
    r.keys.insert(5);
    r.title = "a title that is long enough for a heap block";
    r.attrs.emplace(xo::XString("hp", r.attrs.get_stored_allocator()), 10);
    xo::XDeltaBaseline base;
    Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor cur;
    need(bool(apply_entry(rep, delta_of(src, base), cur)) && cur.last_check() == xo::XDeltaCheck::key,
         "check path: key delta");
    const auto step = [&](const char* what, xo::XDeltaCheck want, auto&& change) {
        change();
        const auto d = delta_of(src, base);
        need(!d.empty(), std::string("check path: no delta for ") + what);
        auto res = apply_entry(rep, d, cur);
        need(bool(res), std::string("check path: rejected ") + what + (res ? "" : ": " + res.error().message));
        need(cur.last_check() == want, std::string("check path: unexpected check for ") + what + ": " +
                                           std::to_string(int(cur.last_check())));
        same_state(src.root(), src, rep, std::string("check path: ") + what);
    };
    using C = xo::XDeltaCheck;
    step("a scalar", C::none, [&] { r.epoch = 77; });
    step("vector elements", C::none, [&] { r.items[1].vals[2] = 9; r.items[2].id = 30; });
    step("a map value", C::none, [&] { r.attrs.begin()->second = 11; });
    step("inline characters", C::none, [&] { r.items[0].name = "m0"; });
    step("a bool", C::in_place, [&] { r.items[1].flag = true; });
    step("an inline length", C::in_place, [&] { r.items[0].name = "longer"; });
    step("an inline string emptied", C::in_place, [&] { r.items[2].name = ""; });
    step("a scalar after in-place checks", C::none, [&] { r.epoch = 78; });
    step("a heap string", C::full, [&] { r.items[2].name = "a name too long to be inline"; });
    step("a heap string within its block", C::in_place, [&] { r.title = "a shorter title, still on the heap"; });
    step("a set insert within capacity", C::in_place, [&] { r.keys.insert(7); r.keys.insert(1); });
    step("a set erase", C::in_place, [&] { r.keys.erase(*r.keys.begin()); });
    step("scalar vector sizes within capacity", C::in_place, [&] { r.items[0].vals.push_back(4); r.items[0].vals.push_back(5); });
    step("a scalar vector shrinks", C::in_place, [&] { r.items[0].vals.pop_back(); });
    step("a set insert past capacity", C::full, [&] { for (std::uint32_t k = 100; k < 120; ++k) r.keys.insert(k); });
    step("a vector of records grows within capacity", C::full, [&] { r.items.emplace_back().id = 9; });
    step("a scalar after a full check", C::none, [&] { r.epoch = 79; });

    // Hand-made deltas through the in-place checks and the unchecked path.
    const char* rbase = static_cast<const char*>(rep.get_address());
    const auto off = [&](const void* p) { return std::size_t(static_cast<const char*>(p) - rbase); };
    const auto rejects = [&](const char* what, const std::string& d, const char* text) {
        const auto before = payload_of(rep);
        auto tc = cur;
        auto res = apply_entry(rep, d, tc);
        need(!res, std::string("check path: accepted ") + what);
        need(res.error().message.find(text) != std::string::npos,
             std::string("check path: wrong error for ") + what + ": " + res.error().message);
        need(payload_of(rep) == before && tc.version() == cur.version(),
             std::string("check path: rejecting ") + what + " changed the replica");
    };
    rejects("a bool of 2", poke_delta(rep, cur, off(&rep.root().items[0].flag), std::string(1, '\x02')),
            "invalid bool representation");
    rejects("inline padding", poke_delta(rep, cur, off(&rep.root().items[0].name) + 9, std::string(1, 'z')),
            "nonzero padding");
    rejects("an inline length of 11", poke_delta(rep, cur, off(&rep.root().items[0].name) + 11, std::string(1, char(0x8B))),
            "exceeds 10");
    rejects("unordered set keys", poke_delta(rep, cur, off(&*rep.root().keys.begin()), std::string(4, '\xFF')),
            "set keys not strictly ordered");
    rejects("a vector size past capacity", poke_delta(rep, cur, off(&rep.root().items[0].vals) + 4, std::string(1, '\x7F')),
            "size exceeds capacity");
    rejects("a missing terminator",
            poke_delta(rep, cur, off(rep.root().title.data() + rep.root().title.size()), std::string(1, 'x')),
            "missing terminator");
    {
        // Free bytes take any value without a check.
        auto tc = cur;
        const auto d = poke_delta(rep, cur, off(&rep.root().epoch), std::string("\x01\x02\x03\x04", 4));
        auto res = apply_entry(rep, d, tc);
        need(bool(res) && tc.last_check() == C::none, "check path: free bytes were not applied unchecked");
        need(bool(typed_load(rep.save_verified())), "check path: unchecked result does not verify");
    }
}

// Changes with their touch() marks: exactly the objects and containers
// changed (containers after the change, wherever their block is now).
void mutate_touched(Buffer& src, std::mt19937_64& rng) {
    auto& r = src.root();
    for (int n = 1 + int(rng() % 4); n > 0; --n) {
        switch (rng() % 14) {
        case 0: r.epoch = rng(); src.touch(r.epoch); break;
        case 1: if (!r.items.empty()) { auto& it = r.items[rng() % r.items.size()]; it.id = std::uint32_t(rng()); src.touch(it.id); } break;
        case 2: if (!r.items.empty()) { auto& it = r.items[rng() % r.items.size()]; it.flag = !it.flag; src.touch(it.flag); } break;
        case 3:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                if (!it.vals.empty()) { auto& v = it.vals[rng() % it.vals.size()]; v = std::uint16_t(rng()); src.touch(v); }
            }
            break;
        case 4:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                it.name = random_text(rng, 40);  // inline or heap, may move to a new block
                src.touch(it.name);
            }
            break;
        case 5: if (!r.attrs.empty()) { auto& v = r.attrs.begin()->second; v = rng(); src.touch(v); } break;
        case 6:
            if (rng() % 2) r.keys.insert(std::uint32_t(rng() % 1000));
            else if (!r.keys.empty()) r.keys.erase(*(r.keys.begin() + std::ptrdiff_t(rng() % r.keys.size())));
            src.touch(r.keys);
            break;
        case 7:
            if (!r.items.empty()) {
                auto& it = r.items[rng() % r.items.size()];
                for (int i = int(rng() % 9); i > 0; --i) it.vals.push_back(std::uint16_t(rng()));
                if (rng() % 3 == 0 && !it.vals.empty()) it.vals.pop_back();
                src.touch(it.vals);
            }
            break;
        case 8: r.title = random_text(rng, 40); src.touch(r.title); break;
        case 9:
            if (!r.items.empty()) {
                r.items.erase(r.items.begin() + std::ptrdiff_t(rng() % r.items.size()));
                src.touch(r.items);
            }
            break;
        case 10: {
            auto& it = r.items.emplace_back();
            it.id = std::uint32_t(rng());
            it.flag = rng() & 1;
            it.name = random_text(rng, 30);
            src.touch(r.items);  // with every item's name and values (the block may have moved)
        } break;
        case 11: r.tags.emplace_back(random_text(rng, 24), r.tags.get_stored_allocator()); src.touch(r.tags); break;
        case 12: r.attrs.insert_or_assign(std::string_view(random_text(rng, 16)), std::uint64_t(rng())); src.touch(r.attrs); break;
        case 13: {  // raw bytes of an object
            r.epoch ^= std::uint64_t(1) << (rng() % 64);
            src.touch(&r.epoch, sizeof r.epoch);
        } break;
        }
    }
}

// touched_only: when the sender touches what it changed, every delta is the
// one a whole comparison writes (same stream, same recorded state), and it
// compares less. A relay forwards what it applies (the blocks a delta wrote
// count as touches) and its deltas match a whole comparison too.
struct TouchTotals {
    std::size_t deltas = 0, relayed = 0, replaced = 0, second = 0;
    std::uint64_t compared_touched = 0, compared_whole = 0;
} touch_totals;

void touched_stream(std::uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    fill(src.root(), rng);
    xo::XDeltaBaseline tb(seed * 2 + 1), wb(seed * 2 + 1);
    Buffer rep(4096, cap);
    xo::XDeltaCursor cur;
    xo::XDeltaBaseline rtb(seed * 4 + 3), rwb(seed * 4 + 3);
    Buffer down(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor down_cur;
    // A second touched_only receiver of src that gets a delta now and then
    // (the touch log is then shared: each baseline finds its own touches).
    xo::XDeltaBaseline tb2(seed * 8 + 5);
    Buffer rep2(4096, cap);
    xo::XDeltaCursor cur2;
    for (int step = 0; step < steps; ++step) {
        const std::string where = "touched seed " + std::to_string(seed) + " step " + std::to_string(step);
        const auto op = rng() % 16;
        bool replaced = false;
        if (op == 0) {
            src.reset();  // content replaced: the next touched delta compares everything
            fill(src.root(), rng);
            replaced = true;
            ++touch_totals.replaced;
        } else if (op == 1) {
            mutate_valid(src.root(), rng);
            src.touch(src.root());  // everything
        } else if (op == 2) {
            tb.reset();  // the receiver asked for a key delta
            wb.reset();
        } else if (op != 3) {  // op 3: nothing changes
            mutate_touched(src, rng);
        }
        xo::XDeltaOptions whole;
        whole.digest = rng() % 2;
        xo::XDeltaOptions touched = whole;
        touched.touched_only = true;
        std::string dt = "stale", dw = "stale";
        const auto st = src.save_delta_into(dt, tb, touched);
        const auto sw = src.save_delta_into(dw, wb, whole);
        need(dt == dw && st.empty == sw.empty && st.key == sw.key && st.blocks == sw.blocks,
             where + ": a touched_only delta differs from the whole comparison");
        need(st.compared_bytes <= sw.compared_bytes, where + ": touched_only compared more");
        if (replaced || st.key) need(st.compared_bytes == sw.compared_bytes, where + ": replaced content not compared whole");
        touch_totals.compared_touched += st.compared_bytes;
        touch_totals.compared_whole += sw.compared_bytes;
        if (rng() % 3 == 0) {
            std::string d2 = "stale";
            if (!src.save_delta_into(d2, tb2, touched).empty) {
                auto r2 = apply_entry(rep2, d2, cur2);
                need(bool(r2), where + ": second receiver's delta rejected: " + (r2 ? std::string() : r2.error().message));
                same_state(src.root(), src, rep2, where + " (second receiver)");
                ++touch_totals.second;
            }
        }
        if (st.empty) continue;
        ++touch_totals.deltas;
        auto r = apply_entry(rep, dt, cur);
        need(bool(r), where + ": delta rejected: " + (r ? std::string() : r.error().message));
        same_state(src.root(), src, rep, where);
        // The relay forwards.
        std::string rt = "stale", rw = "stale";
        const auto rst = rep.save_delta_into(rt, rtb, touched);
        const auto rsw = rep.save_delta_into(rw, rwb, whole);
        need(rt == rw && rst.empty == rsw.empty, where + ": a relayed touched_only delta differs from the whole comparison");
        need(rst.compared_bytes <= rsw.compared_bytes, where + ": the relay compared more");
        if (rst.empty) continue;
        ++touch_totals.relayed;
        auto dr = apply_entry(down, rt, down_cur);
        need(bool(dr), where + ": relayed delta rejected: " + (dr ? std::string() : dr.error().message));
        same_state(src.root(), src, down, where + " (downstream)");
    }
}

// XDeltaRecord::touches: no copy; each delta carries the bytes touched since
// the baseline's last one, and the replica holds the source after every
// delta. Mostly the baseline is the only one pairing with the source's touch
// log (its list holds the touches); sometimes a copy baseline pairs in
// between (the next touches delta sends whole touched blocks), the content
// is replaced (a key delta) or the receiver asks for one. A relay forwards
// what it applies with a touches baseline too.
struct TouchesTotals {
    std::size_t deltas = 0, keys = 0, relayed = 0, interleaved = 0;
    std::uint64_t bytes = 0, copy_bytes = 0;
} touches_totals;

void touches_stream(std::uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    fill(src.root(), rng);
    xo::XDeltaBaseline xb(xo::XDeltaRecord::touches), cb(seed * 2 + 1);
    Buffer rep(4096, cap);
    xo::XDeltaCursor cur;
    Buffer crep(4096, cap);
    xo::XDeltaCursor ccur;
    xo::XDeltaBaseline rb(xo::XDeltaRecord::touches, seed * 4 + 3);
    Buffer down(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor down_cur;
    for (int step = 0; step < steps; ++step) {
        const std::string where = "touches seed " + std::to_string(seed) + " step " + std::to_string(step);
        const auto op = rng() % 16;
        if (op == 0) {
            src.reset();  // content replaced: a key delta
            fill(src.root(), rng);
        } else if (op == 1) {
            mutate_valid(src.root(), rng);
            src.touch(src.root());  // everything (often more blocks than the list holds)
        } else if (op == 2) {
            xb.reset();  // the receiver asked for a key delta
        } else if (op != 3) {  // op 3: nothing changes
            mutate_touched(src, rng);
        }
        xo::XDeltaOptions options;
        options.digest = rng() % 2;
        options.whole_blocks = rng() % 8 == 0;
        std::string d = "stale";
        const auto st = src.save_delta_into(d, xb, options);
        need(xb.recorded_bytes() == 0 && xb.record() == xo::XDeltaRecord::touches, where + ": a touches baseline kept a copy");
        need(st.key || st.compared_bytes == 0, where + ": a touches delta compared bytes");
        if (st.empty) {
            need(d.empty() && payload_of(src) == payload_of(rep), where + ": empty touches delta although the payload changed");
        } else {
            ++touches_totals.deltas;
            touches_totals.keys += st.key;
            touches_totals.bytes += d.size();
            auto r = apply_entry(rep, d, cur);
            need(bool(r), where + ": touches delta rejected: " + (r ? std::string() : r.error().message));
            same_state(src.root(), src, rep, where);
        }
        if (rng() % 6 == 0) {
            // A copy baseline pairs in between.
            ++touches_totals.interleaved;
            xo::XDeltaOptions touched;
            touched.touched_only = true;
            std::string dc = "stale";
            const auto sc = src.save_delta_into(dc, cb, touched);
            if (!sc.empty) {
                touches_totals.copy_bytes += dc.size();
                auto r = apply_entry(crep, dc, ccur);
                need(bool(r), where + ": copy delta rejected: " + (r ? std::string() : r.error().message));
                same_state(src.root(), src, crep, where + " (copy)");
            }
        }
        // The relay forwards.
        std::string rd = "stale";
        const auto rst = rep.save_delta_into(rd, rb, options);
        if (rst.empty) {
            need(payload_of(rep) == payload_of(down) || down_cur.empty(), where + ": empty relayed delta although it changed");
            continue;
        }
        ++touches_totals.relayed;
        auto dr = apply_entry(down, rd, down_cur);
        need(bool(dr), where + ": relayed touches delta rejected: " + (dr ? std::string() : dr.error().message));
        same_state(src.root(), src, down, where + " (downstream)");
    }
}

// Touches deltas: touched bytes go even unchanged, untouched changes do not;
// more touched blocks than the list holds go whole; growth and shrinking;
// the copy is never kept and key_delta_into is not offered.
void touches_fixed_case() {
    std::mt19937_64 rng(23);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    fill(src.root(), rng);
    while (src.root().items.size() < 2) src.root().items.emplace_back().id = 1;
    xo::XDeltaBaseline xb(xo::XDeltaRecord::touches);
    Buffer rep(4096, cap);
    xo::XDeltaCursor cur;
    std::string d;
    auto st = src.save_delta_into(d, xb);
    need(st.key && xb.recorded_bytes() == 0 && !xb.empty(), "touches: first delta is not a key delta");
    need(bool(apply_entry(rep, d, cur)), "touches: key delta");
    // A touched scalar that did not change still goes (its 8 bytes).
    src.touch(src.root().epoch);
    st = src.save_delta_into(d, xb);
    need(!st.empty && st.blocks == 1 && st.changed_bytes == sizeof(src.root().epoch) && st.compared_bytes == 0,
         "touches: an unchanged touched scalar did not go as its bytes");
    need(bool(apply_entry(rep, d, cur)), "touches: scalar delta");
    same_state(src.root(), src, rep, "touches: scalar");
    // Nothing touched: nothing sent.
    st = src.save_delta_into(d, xb);
    need(st.empty, "touches: an untouched buffer sent a delta");
    // An untouched change is not sent; the digest catches it.
    src.root().epoch += 1;
    src.root().items[0].id += 1;
    src.touch(src.root().items[0].id);
    xo::XDeltaOptions digest;
    digest.digest = true;
    st = src.save_delta_into(d, xb, digest);
    need(!st.empty && st.changed_bytes == sizeof(src.root().items[0].id), "touches: the touched id did not go alone");
    auto res = apply_entry(rep, d, cur);
    need(!res && res.code() == xo::XErrc::digest_mismatch, "touches: the digest did not catch an untouched change");
    xb.reset();
    need(bool(apply_entry(rep, src.save_delta(xb), cur)) && src.save_delta_into(d, xb).empty,
         "touches: resync key delta");
    same_state(src.root(), src, rep, "touches: resync");
    // Growth: the bytes past the receiver's used size go where nonzero.
    auto& vals = src.root().items[1].vals;
    for (int i = 0; i < 3000; ++i) vals.push_back(std::uint16_t(i + 1));
    src.touch(vals);
    st = src.save_delta_into(d, xb);
    need(!st.empty, "touches: no delta for growth");
    need(bool(apply_entry(rep, d, cur)), "touches: growth delta");
    same_state(src.root(), src, rep, "touches: growth");
    // More touched blocks than the list holds: they go whole.
    const auto used = std::size_t(src.arena()->used_end);
    need(used / 64 > xo::detail::DeltaTouchLog::list_capacity + 4, "touches: payload too small for the list");
    for (std::size_t i = 0; i < vals.size(); i += 32) vals[i] = std::uint16_t(vals[i] + 1);
    for (std::size_t i = 0; i < vals.size(); i += 32) src.touch(vals[i]);
    st = src.save_delta_into(d, xb);
    const std::size_t touched_blocks = (vals.size() + 31) / 32;
    need(touched_blocks > xo::detail::DeltaTouchLog::list_capacity && st.blocks == touched_blocks &&
             st.changed_bytes > (touched_blocks - 2) * 64,  // all whole but maybe the payload's last
         "touches: an overflowed list did not send whole blocks");
    need(bool(apply_entry(rep, d, cur)), "touches: overflow delta");
    same_state(src.root(), src, rep, "touches: overflow");
    // Back within the list: only the touched bytes again.
    vals[5] = 7;
    src.touch(vals[5]);
    st = src.save_delta_into(d, xb);
    need(st.blocks == 1 && st.changed_bytes == sizeof(std::uint16_t), "touches: the list was not used again");
    need(bool(apply_entry(rep, d, cur)), "touches: delta after overflow");
    // An item erased: its blocks become holes (they go with the hole change).
    src.root().items.erase(src.root().items.begin() + 1);
    src.touch(src.root().items);
    st = src.save_delta_into(d, xb);
    need(st.holes, "touches: erasing an item changed no hole");
    need(bool(apply_entry(rep, d, cur)), "touches: erase delta");
    same_state(src.root(), src, rep, "touches: erase");
}

void touched_fixed_case() {
    std::mt19937_64 rng(17);
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto src = Buffer::create(4096, cap);
    fill(src.root(), rng);
    while (src.root().items.size() < 2) src.root().items.emplace_back().id = 1;
    xo::XDeltaOptions touched;
    touched.touched_only = true;
    xo::XDeltaBaseline base;
    Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor cur;
    need(bool(apply_entry(rep, delta_of(src, base, touched), cur)), "touched: key delta");
    const std::size_t whole = payload_of(src).size() - xo::XWIRE_PAYLOAD_BEGIN;
    std::string d;
    // The first touched delta of a baseline compares everything (the key
    // delta paired it); a touched change then compares its block only.
    src.root().epoch += 1;
    src.touch(src.root().epoch);
    auto st = src.save_delta_into(d, base, touched);
    // Its block: 64 bytes, or the 16 past the arena header for block 0.
    const auto epoch_at = std::size_t(reinterpret_cast<const char*>(&src.root().epoch) -
                                      static_cast<const char*>(static_cast<const xo::XBufferCore&>(src).get_address()));
    const std::size_t epoch_block = epoch_at < 64 ? 64 - xo::XWIRE_PAYLOAD_BEGIN : 64;
    need(epoch_at % 64 + sizeof(src.root().epoch) <= 64, "touched: the scalar crosses a block");
    need(!st.empty && st.compared_bytes == epoch_block, "touched: one touched scalar did not compare one block");
    need(bool(apply_entry(rep, d, cur)), "touched: scalar delta");
    same_state(src.root(), src, rep, "touched: scalar");
    // Nothing touched: nothing compared, nothing sent.
    st = src.save_delta_into(d, base, touched);
    need(st.empty && st.compared_bytes == 0, "touched: an untouched buffer compared bytes");
    {
        // A copy of the baseline left behind while the original goes on:
        // the touches since it are not kept for it, so it compares everything.
        xo::XDeltaBaseline behind = base;
        Buffer rep_behind = rep.clone();
        xo::XDeltaCursor cur_behind = cur;
        for (int i = 0; i < 2; ++i) {
            src.root().epoch += 1;
            src.touch(src.root().epoch);
            need(bool(apply_entry(rep, src.save_delta(base, touched), cur)), "touched: delta ahead of a copy");
        }
        auto sb = src.save_delta_into(d, behind, touched);
        need(sb.compared_bytes == whole, "touched: a baseline left behind did not compare everything");
        need(bool(apply_entry(rep_behind, d, cur_behind)), "touched: delta of a baseline left behind");
        same_state(src.root(), src, rep_behind, "touched: baseline left behind");
        // Its delta paired it: the log is now shared, and the original
        // still compares only what was touched since its own last delta.
        src.root().epoch += 1;
        src.touch(src.root().epoch);
        st = src.save_delta_into(d, base, touched);
        need(st.compared_bytes == epoch_block, "touched: the original compared more once the log was shared");
        need(bool(apply_entry(rep, d, cur)), "touched: delta after sharing");
        sb = src.save_delta_into(d, behind, touched);
        need(sb.compared_bytes == epoch_block, "touched: the second baseline compared more than its touches");
        need(bool(apply_entry(rep_behind, d, cur_behind)), "touched: second baseline's delta");
        same_state(src.root(), src, rep_behind, "touched: second baseline");
    }
    // A change not touched is not sent; with a digest the receiver finds out.
    src.root().epoch += 1;
    st = src.save_delta_into(d, base, touched);
    need(st.empty, "touched: an untouched change was sent");
    src.root().items[0].id += 1;
    src.touch(src.root().items[0].id);
    xo::XDeltaOptions digest = touched;
    digest.digest = true;
    st = src.save_delta_into(d, base, digest);
    need(!st.empty, "touched: no delta for a touched change");
    auto res = apply_entry(rep, d, cur);
    need(!res && res.code() == xo::XErrc::digest_mismatch, "touched: the digest did not catch an untouched change");
    base.reset();  // the receiver asks for a key delta
    need(bool(apply_entry(rep, delta_of(src, base, touched), cur)), "touched: resync key delta");
    same_state(src.root(), src, rep, "touched: resync");
    // A touch outside the payload: everything is compared next time.
    std::uint64_t elsewhere = 0;
    src.touch(elsewhere);
    src.root().epoch += 1;
    st = src.save_delta_into(d, base, touched);
    need(!st.empty && st.compared_bytes == whole, "touched: a foreign touch did not compare everything");
    need(bool(apply_entry(rep, d, cur)), "touched: delta after a foreign touch");
    same_state(src.root(), src, rep, "touched: foreign touch");
    // Content replaced in place (reload): everything is compared.
    auto copy = src.save_verified();
    src.reload_verified(copy);
    src.root().epoch += 1;
    src.touch(src.root().epoch);
    st = src.save_delta_into(d, base, touched);
    need(!st.empty && st.compared_bytes == payload_of(src).size() - xo::XWIRE_PAYLOAD_BEGIN,
         "touched: a reloaded buffer did not compare everything");
    need(bool(apply_entry(rep, d, cur)), "touched: delta after reload");
    same_state(src.root(), src, rep, "touched: reload");
    // A container that moves to a new block: touching it where it is now
    // suffices (the block it left is a hole, found by the hole tables).
    auto& vals = src.root().items[1].vals;
    for (int i = 0; i < 40; ++i) vals.push_back(std::uint16_t(i));
    src.touch(vals);
    st = src.save_delta_into(d, base, touched);
    need(!st.empty && st.compared_bytes < payload_of(src).size() - xo::XWIRE_PAYLOAD_BEGIN,
         "touched: a moved container compared everything");
    need(bool(apply_entry(rep, d, cur)), "touched: moved container");
    same_state(src.root(), src, rep, "touched: moved container");
}

// The bytes past the baseline's used size are compared even where the
// touches skip a whole group of untouched blocks: here a string head is
// touched and the block it now points to came with growth, untouched. The
// old used end moves through a group of 16 blocks as `count` grows.
void touched_growth_case() {
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    xo::XDeltaOptions touched;
    touched.touched_only = true;
    for (std::uint32_t count = 8; count < 80; ++count) {
        const std::string where = "touched growth, " + std::to_string(count) + " slots";
        auto src = xo::TypedXBuffer<dz::View>::create(4096, cap);
        src.root().slots.reserve(count);  // one block, no holes
        for (std::uint32_t i = 0; i < count; ++i) src.root().slots.emplace_back().id = i + 1;
        need(holes_of(src).empty(), where + ": holes before the change");
        xo::XDeltaBaseline base;
        xo::TypedXBuffer<dz::View> rep(4096, xo::XBuffer::maximum_capacity(kLimit));
        xo::XDeltaCursor cur;
        std::string d;
        (void)src.save_delta_into(d, base, touched);
        need(bool(rep.try_apply_delta_verified(d, cur)), where + ": key delta");
        auto& name = src.root().slots[0].name;
        name = "a name too long to stay inline";  // a new block at the end
        need(holes_of(src).empty(), where + ": holes after the change");
        src.touch(&name, sizeof(name));  // the head only
        const auto st = src.save_delta_into(d, base, touched);
        need(!st.empty, where + ": no delta");
        auto r = rep.try_apply_delta_verified(d, cur);
        need(bool(r), where + ": delta rejected: " + (r ? std::string() : r.error().message));
        need(payload_of(src) == payload_of(rep), where + ": the grown bytes were not sent");
    }
}

// The cursors of a thread share its apply scratch: several threads at once
// feed their replicas in turn, and a delta applied from a thread_local
// destructor that runs after the thread's scratch is destroyed uses the
// cursor's own.
struct AtThreadExit {
    std::function<void()> run;
    ~AtThreadExit() {
        if (run) run();
    }
};

struct LateApply {
    std::unique_ptr<xo::TypedXBuffer<dz::View>> rep;
    xo::XDeltaCursor cur;
    std::string key, expect;
    bool applied = false;
};

void thread_scratch_case() {
    constexpr int threads = 4, replicas = 3, steps = 80;
    std::vector<LateApply> late(threads);
    std::vector<std::string> errors(threads);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&late, &errors, t] {
            static thread_local AtThreadExit at_exit;  // before any delta: destroyed after the scratch
            const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
            std::mt19937_64 rng(20261104u + std::uint64_t(t));
            auto src = xo::TypedXBuffer<dz::View>::create(4096, cap);
            src.root().slots.reserve(64);
            std::vector<xo::XDeltaBaseline> bases(replicas);
            std::vector<xo::XDeltaCursor> curs(replicas);
            std::vector<std::unique_ptr<xo::TypedXBuffer<dz::View>>> reps;
            for (int r = 0; r < replicas; ++r)
                reps.push_back(std::make_unique<xo::TypedXBuffer<dz::View>>(4096, xo::XBuffer::maximum_capacity(kLimit)));
            std::string d;
            for (int step = 0; step < steps && errors[t].empty(); ++step) {
                auto& v = src.root();
                v.tick = std::uint32_t(step + 1);
                if (v.slots.size() < 48 || rng() % 4 == 0) v.slots.emplace_back().id = std::uint32_t(rng());
                for (auto& u : v.slots) {
                    if (rng() % 3 == 0) u.hp -= 1;
                    if (rng() % 16 == 0) u.name = rng() % 2 ? "short" : "a name that does not fit inline";
                }
                for (int r = 0; r < replicas; ++r) {
                    (void)src.save_delta_into(d, bases[std::size_t(r)]);
                    if (d.empty()) continue;
                    auto res = reps[std::size_t(r)]->try_apply_delta_verified(d, curs[std::size_t(r)]);
                    if (!res) {
                        errors[t] = "thread delta rejected: " + res.error().message;
                        break;
                    }
                    if (payload_of(*reps[std::size_t(r)]) != payload_of(src)) {
                        errors[t] = "thread replica differs";
                        break;
                    }
                }
            }
            LateApply& l = late[std::size_t(t)];
            xo::XDeltaBaseline fresh;
            (void)src.save_delta_into(l.key, fresh);
            l.expect = payload_of(src);
            l.rep = std::make_unique<xo::TypedXBuffer<dz::View>>(4096, xo::XBuffer::maximum_capacity(kLimit));
            at_exit.run = [&l] { l.applied = bool(l.rep->try_apply_delta_verified(l.key, l.cur)); };
        });
    for (auto& th : pool) th.join();
    for (int t = 0; t < threads; ++t) {
        need(errors[std::size_t(t)].empty(), errors[std::size_t(t)]);
        const LateApply& l = late[std::size_t(t)];
        need(l.applied && payload_of(*l.rep) == l.expect, "key delta applied at thread exit");
    }
}

void wrong_type_case() {
    const auto cap = xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic);
    auto view = xo::TypedXBuffer<dz::View>::create(4096, cap);
    view.root().tick = 1;
    xo::XDeltaBaseline vbase;
    std::string vkey;
    (void)view.save_delta_into(vkey, vbase);
    Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit));
    xo::XDeltaCursor cur;
    auto r = apply_entry(rep, vkey, cur);
    need(!r && r.code() == xo::XErrc::root_type_mismatch && cur.empty(), "key delta of another root type applied");
}
} // namespace

int main(int argc, char** argv) {
    const int streams = argc > 1 ? std::atoi(argv[1]) : 40;
    const int steps = argc > 2 ? std::atoi(argv[2]) : 60;
    fixed_cases();
    diff_reference_case();
    block_section_case();
    check_path_case();
    wrong_type_case();
    view_sizes();
    touched_fixed_case();
    touched_growth_case();
    touches_fixed_case();
    thread_scratch_case();
    for (int s = 0; s < streams; ++s) random_stream(20261102u + std::uint64_t(s), steps);
    for (int s = 0; s < streams; ++s) touched_stream(20261103u + std::uint64_t(s), steps);
    need(touch_totals.compared_touched * 4 < touch_totals.compared_whole * 3 && touch_totals.relayed > 0 &&
             touch_totals.replaced > 0 && touch_totals.second > 0,
         "touched_only: compared too much, or a path was never taken");
    for (int s = 0; s < streams; ++s) touches_stream(20261104u + std::uint64_t(s), steps);
    need(touches_totals.deltas > touches_totals.keys && touches_totals.relayed > 0 && touches_totals.interleaved > 0,
         "touches: a path was never taken");
    need(totals.keys >= std::size_t(streams), "too few key deltas");
    need(totals.shrinks > 0 && totals.digests > 0 && totals.corrupt_rejected > 0 && totals.late_joins > 0,
         "a path was never taken");
    need(streams < 10 || (totals.checks[1] > 0 && totals.checks[2] > 0 && totals.checks[3] > 0),
         "a receiver check path was never taken");
    std::cout << "test_delta_sync: " << streams << " streams x " << steps << " steps, " << totals.deltas
              << " deltas (" << totals.keys << " key, " << totals.digests << " with digest), " << totals.empties
              << " empty, " << totals.shrinks << " shrinks, " << totals.late_joins << " late joins; receiver checks: "
              << totals.checks[1] << " none, " << totals.checks[2] << " in place, " << totals.checks[3]
              << " full; corrupted: " << totals.corrupt_rejected << " rejected, "
              << totals.corrupt_accepted << " accepted as valid payloads; delta bytes " << totals.delta_bytes
              << " vs full wires " << totals.full_bytes << "; touched_only: " << touch_totals.deltas << " deltas, "
              << touch_totals.relayed << " relayed, compared " << touch_totals.compared_touched << " of "
              << touch_totals.compared_whole << " bytes; touches: " << touches_totals.deltas << " deltas ("
              << touches_totals.keys << " key), " << touches_totals.relayed << " relayed, " << touches_totals.bytes
              << " bytes\n";
    return 0;
}
