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
// xxHash64 digest against reference values, and a viewport-like model whose
// delta sizes are reported. With exceptions on, the throwing entry must
// report exactly what the try_ entry reports. No try/catch of its own: also
// built as test_delta_sync_noexc.
#include "fuzz/v2_fuzz_core.hpp"

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
    std::size_t corrupt_rejected = 0, corrupt_accepted = 0;
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

// Corrupted copies of `d` applied to a copy of the replica.
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
        case 5: { // a byte inside the run section
            const std::size_t from = std::min<std::size_t>(bad.size() - 1, 32 + (digest ? 8 : 0));
            bad[from + rng() % (bad.size() - from)] ^= char(1 + rng() % 255);
        } break;
        }
        if (bad == d) continue;
        auto trial = rep.clone();
        auto tc = cur;
        auto r = apply_entry(trial, bad, tc);
        if (!r) {
            need(payload_of(trial) == before && holes_of(trial) == before_holes,
                 where + ": a rejected delta changed the replica (" + r.error().message + ")");
            need(tc.version() == cur.version() && tc.stream() == cur.stream(), where + ": a rejected delta moved the cursor");
            need(dump(trial.root()) == before_dump, where + ": a rejected delta changed the contents");
            ++totals.corrupt_rejected;
        } else {
            auto again = typed_load(trial.save_verified());
            need(bool(again), where + ": an accepted corrupted delta left a payload that does not verify");
            if (digest) need(payload_of(trial) == expected_payload, where + ": a digest delta accepted another result");
            ++totals.corrupt_accepted;
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

void random_stream(std::uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    auto src = Buffer::create(4096, xo::XBuffer::maximum_capacity(kLimit).with_growth(xo::XGrowPolicy::Automatic));
    fill(src.root(), rng);
    xo::XDeltaBaseline base(seed * 2 + 1);
    Buffer rep(4096, xo::XBuffer::maximum_capacity(kLimit)); // an empty arena: only a key delta applies
    xo::XDeltaCursor cur;
    for (int step = 0; step < steps; ++step) {
        const std::string where = "seed " + std::to_string(seed) + " step " + std::to_string(step);
        const auto op = rng() % 20;
        if (op == 0) {
            base.reset(); // the receiver asked for a resync: a key delta onto its current content
        } else if (op == 1) {
            src.reset(); // the payload shrinks
            fill(src.root(), rng);
            ++totals.shrinks;
        } else if (op != 2) { // op 2: nothing changes
            mutate_valid(src.root(), rng);
            if (rng() % 4 == 0) src.root().epoch ^= std::uint64_t(1) << (rng() % 64); // one bit
        }
        xo::XDeltaOptions options;
        options.digest = rng() % 2;
        options.merge_gap = std::uint32_t(rng() % 5);
        std::string d = "stale bytes from an earlier delta";
        const auto stats = src.save_delta_into(d, base, options);
        need(stats.delta_bytes == d.size(), where + ": stats disagree with the delta size");
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
        if (!stats.key && rng() % 3 == 0) corrupt_trials(rep, cur, d, options.digest, payload_of(src), rng, where);
        auto r = apply_entry(rep, d, cur);
        need(bool(r), where + ": delta rejected: " + (r ? std::string() : r.error().message));
        same_state(src.root(), src, rep, where);
        need(cur.version() == base.version() && cur.stream() == base.stream(), where + ": cursor does not follow");
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
        need(small.size() <= sizeof(xo::XDeltaHeader) + 3 + 8, "one changed scalar gave a large delta: " +
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
        // The root offset (offset 24) one word further: the header checks
        // pass, verification of the patched payload rejects.
        std::uint32_t root = 0;
        std::memcpy(&root, key.data() + 24, 4);
        std::string bad = key;
        const std::uint32_t moved = root + 8;
        std::memcpy(bad.data() + 24, &moved, 4);
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
    std::uint64_t delta_sum = 0, full_sum = 0, compact_sum = 0, runs = 0;
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
        runs += stats.runs;
        full_sum += server.save_verified().size();
        compact_sum += server.save_compact(kTight).size();
        need(bool(client.try_apply_delta_verified(d, cur)), "view delta rejected at tick " + std::to_string(t));
        need(payload_of(client) == payload_of(server), "view replica differs at tick " + std::to_string(t));
    }
    std::cout << "view (120 units, 30% move, 5% hp, 2 leave + 2 enter per tick): key delta " << key_bytes
              << " B; per tick: delta " << delta_sum / ticks << " B in " << runs / ticks << " runs, save_verified "
              << full_sum / ticks << " B, save_compact " << compact_sum / ticks << " B\n";
    need(delta_sum * 4 < compact_sum, "view deltas are not much smaller than compact wires");
}

// The wrong-type case uses the typed entry of the right type for the sender.
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
    wrong_type_case();
    view_sizes();
    for (int s = 0; s < streams; ++s) random_stream(20261102u + std::uint64_t(s), steps);
    need(totals.keys >= std::size_t(streams), "too few key deltas");
    need(totals.shrinks > 0 && totals.digests > 0 && totals.corrupt_rejected > 0, "a path was never taken");
    std::cout << "test_delta_sync: " << streams << " streams x " << steps << " steps, " << totals.deltas
              << " deltas (" << totals.keys << " key, " << totals.digests << " with digest), " << totals.empties
              << " empty, " << totals.shrinks << " shrinks; corrupted: " << totals.corrupt_rejected << " rejected, "
              << totals.corrupt_accepted << " accepted as valid payloads; delta bytes " << totals.delta_bytes
              << " vs full wires " << totals.full_bytes << "\n";
    return 0;
}
